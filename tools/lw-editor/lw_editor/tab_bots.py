"""
tab_bots.py -- DefaultProfilesTab and clone-profile editor tab.
"""
import tkinter as tk
from tkinter import ttk, messagebox
from mysql.connector import Error as MySQLError
from .constants import (
    WOW_CLASSES, ROLE_OPTS, SPEC_TO_CLASS, SPEC_ALIAS_TO_CLASS,
    CONSERVATION_MODES, AOE_MODES,
    STAT_KEYS, SUBJECT_KEYS,
    _normalize_role,
    CANONICAL_DEFAULT_PROFILES, CANONICAL_SPEC_LOOKUP, PROFILE_CONTEXTS,
    TALENT_DATA, CLASS_NAME_TO_ID,
)
from .db import db
from .helpers import lbl, entry_w, combo_w, check_w, unix_text
from .widgets import ProfileHeaderFrame, DefaultProfilePicker
from .rotation import RotationEditor, _class_from_spec
from .ooc_panel import OocProfilePanel

# ═══════════════════════════════════════════════════════════════════════════

class DefaultTalentTemplateEditor(ttk.Frame):

    MAX_TALENT_POINTS = 71

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._profile = None
        self._template = None
        self._class_id = 0
        self._spec_key = ""
        self._profiles_provider = None
        self._build()
        self.clear()

    def set_profiles_provider(self, provider):
        self._profiles_provider = provider

    def _build(self):
        top = ttk.Frame(self)
        top.pack(fill=tk.X, padx=4, pady=4)

        self.v_template_status = tk.StringVar()
        self.v_talent_points = tk.StringVar()

        ttk.Label(top, textvariable=self.v_template_status, foreground="#555").pack(
            side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(top, text="⧉ Copy From Profile", command=self._copy_from_profile).pack(
            side=tk.RIGHT, padx=2)
        ttk.Button(top, text="➕ Add Talent Point", command=self._add_talent_point).pack(
            side=tk.RIGHT, padx=2)
        ttk.Button(top, text="🔄 Reset All Talents", command=self._reset_talents).pack(
            side=tk.RIGHT, padx=2)
        ttk.Label(top, textvariable=self.v_talent_points,
                  font=("TkDefaultFont", 10, "bold")).pack(side=tk.RIGHT, padx=(8, 0))

        trees_frame = ttk.Frame(self)
        trees_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))
        trees_frame.columnconfigure(0, weight=1)
        trees_frame.columnconfigure(1, weight=1)
        trees_frame.columnconfigure(2, weight=1)

        self._tal_tree_frames = []
        self._tal_tvs = []
        for col in range(3):
            lf = ttk.LabelFrame(trees_frame, text=f"Tree {col}", padding=4)
            lf.grid(row=0, column=col, sticky="nsew", padx=4, pady=4)
            lf.rowconfigure(0, weight=1)
            lf.columnconfigure(0, weight=1)

            tv = ttk.Treeview(
                lf,
                columns=("name", "rank", "row"),
                show="headings",
                height=22,
                selectmode="browse",
            )
            tv.heading("name", text="Talent")
            tv.heading("rank", text="Rank")
            tv.heading("row", text="Row")
            tv.column("name", width=170)
            tv.column("rank", width=70, anchor="center")
            tv.column("row", width=40, anchor="center")
            scr = ttk.Scrollbar(lf, orient="vertical", command=tv.yview)
            tv.configure(yscrollcommand=scr.set)
            tv.grid(row=0, column=0, sticky="nsew")
            scr.grid(row=0, column=1, sticky="ns")
            tv.tag_configure("maxed", foreground="#00aa00")
            tv.tag_configure("partial", foreground="#cc8800")
            self._tal_tree_frames.append(lf)
            self._tal_tvs.append(tv)

    @staticmethod
    def _normalize_talent_name(text: str) -> str:
        return "".join(ch for ch in (text or "").lower() if ch.isalnum())

    def _trees_for_current_class(self) -> dict:
        return TALENT_DATA.get(str(self._class_id), {}) if self._class_id else {}

    def _display_spec_name(self, trees: dict | None = None) -> str:
        trees = trees or self._trees_for_current_class()
        wanted = self._normalize_talent_name(self._spec_key)
        for tree in trees.values():
            tree_name = tree.get("tree_name", "")
            if self._normalize_talent_name(tree_name) == wanted:
                return tree_name
        return self._spec_key or "Unknown"

    def _template_display_name(self, trees: dict | None = None) -> str:
        class_name = (self._profile or {}).get("class_key") or "Unknown"
        return f"{self._display_spec_name(trees)} {class_name} Talent Template"

    def _current_ranks(self) -> dict[int, int]:
        entries = (self._template or {}).get("entries", [])
        return {
            int(row.get("talent_id") or 0): int(row.get("desired_rank") or 0)
            for row in entries
            if int(row.get("talent_id") or 0) > 0
        }

    def _primary_tree_index(self, trees: dict) -> int:
        wanted = self._normalize_talent_name(self._spec_key)
        for tree_idx in range(3):
            tree_name = (trees.get(str(tree_idx), {}) or {}).get("tree_name", "")
            if self._normalize_talent_name(tree_name) == wanted:
                return tree_idx
        return 0

    def _priority_for_talent(self, talent: dict, tree_idx: int, trees: dict) -> int:
        primary = self._primary_tree_index(trees)
        if tree_idx == primary:
            offset = 0
        else:
            others = [idx for idx in range(3) if idx != primary]
            offset = 200 if tree_idx == others[0] else 300
        return offset + (int(talent.get("row") or 0) * 10) + int(talent.get("col") or 0)

    def clear(self):
        self._profile = None
        self._template = None
        self._class_id = 0
        self._spec_key = ""
        self.v_template_status.set("")
        self.v_talent_points.set("")
        for tv in self._tal_tvs:
            tv.delete(*tv.get_children())
        for lf in self._tal_tree_frames:
            lf.configure(text="—")

    def load_profile(self, profile: dict | None):
        self.clear()
        if not profile:
            return

        self._profile = dict(profile)
        class_name = (profile.get("class_key") or "").strip()
        self._spec_key = (profile.get("spec_key") or "").strip()
        self._class_id = CLASS_NAME_TO_ID.get(class_name, 0)

        if not TALENT_DATA:
            self.v_template_status.set("Run extract_dbc_data.py to enable talent template display")
            return
        if not self._class_id or not self._spec_key:
            self.v_template_status.set("Choose a class/spec to view the talent template")
            return

        trees = self._trees_for_current_class()
        if not trees:
            self.v_template_status.set("No talent tree data for this class")
            return

        self._template = db.load_default_talent_template(self._spec_key, self._class_id)
        template_name = (self._template or {}).get("display_name") or self._template_display_name(trees)
        self.v_template_status.set(
            f"{template_name} — shared across all {class_name} {self._display_spec_name(trees)} default profiles")

        ranks = self._current_ranks()
        total_spent = 0
        for tree_idx in range(3):
            tree = trees.get(str(tree_idx), {})
            tree_name = tree.get("tree_name", f"Tree {tree_idx}")
            talents = tree.get("talents", [])
            tv = self._tal_tvs[tree_idx]
            lf = self._tal_tree_frames[tree_idx]
            lf.configure(text=tree_name)

            for talent in talents:
                talent_id = int(talent.get("talent_id") or 0)
                current_rank = int(ranks.get(talent_id, 0))
                max_rank = int(talent.get("max_rank") or len(talent.get("rank_spell_ids", [])) or 0)
                total_spent += current_rank
                tag = "maxed" if current_rank == max_rank and max_rank > 0 else (
                    "partial" if current_rank > 0 else "")
                tv.insert(
                    "", "end",
                    iid=f"tmpl_{self._class_id}_{self._spec_key}_{talent_id}",
                    values=(talent.get("name", ""), f"{current_rank}/{max_rank}", talent.get("row", 0)),
                    tags=(tag,),
                )

        available = max(0, self.MAX_TALENT_POINTS - total_spent)
        self.v_talent_points.set(
            f"{available} available  ({total_spent} spent / {self.MAX_TALENT_POINTS} total)")

    def _add_talent_point(self):
        if not self._profile:
            return
        if not TALENT_DATA:
            messagebox.showinfo("No data", "Run extract_dbc_data.py first to load talent data.")
            return

        trees = self._trees_for_current_class()
        if not trees:
            messagebox.showinfo("No data", "No talent tree data for this class.")
            return

        current_ranks = self._current_ranks()
        total_spent = sum(current_ranks.values())
        available = self.MAX_TALENT_POINTS - total_spent
        if available <= 0:
            messagebox.showinfo("No points", "This template already spends all available talent points.")
            return

        choices = []
        for tree_idx in range(3):
            tree = trees.get(str(tree_idx), {})
            tree_name = tree.get("tree_name", f"Tree {tree_idx}")
            for talent in tree.get("talents", []):
                talent_id = int(talent.get("talent_id") or 0)
                current_rank = int(current_ranks.get(talent_id, 0))
                max_rank = int(talent.get("max_rank") or len(talent.get("rank_spell_ids", [])) or 0)
                if current_rank < max_rank:
                    label = f"[{tree_name}] {talent.get('name', '')}  ({current_rank}/{max_rank})"
                    choices.append((label, tree_idx, talent, current_rank))

        if not choices:
            messagebox.showinfo("Maxed", "All talents in this template are already at max rank.")
            return

        dlg = tk.Toplevel(self)
        dlg.title("Add Talent Point")
        dlg.resizable(False, False)
        dlg.grab_set()

        ttk.Label(dlg, text="Select talent:").pack(anchor="w", padx=8, pady=(8, 0))
        choice_var = tk.StringVar()
        combo = ttk.Combobox(dlg, textvariable=choice_var, state="readonly", width=52)
        combo["values"] = [row[0] for row in choices]
        combo.current(0)
        combo.pack(fill=tk.X, padx=8, pady=4)

        ttk.Label(dlg, text="Points to add:").pack(anchor="w", padx=8)
        pts_var = tk.StringVar(value="1")
        ttk.Entry(dlg, textvariable=pts_var, width=6).pack(anchor="w", padx=8, pady=4)

        def _apply():
            idx = combo.current()
            if idx < 0:
                return
            _, tree_idx, talent, current_rank = choices[idx]
            try:
                pts = int(pts_var.get())
            except ValueError:
                messagebox.showerror("Invalid", "Points must be a number.", parent=dlg)
                return
            if pts < 1:
                messagebox.showerror("Invalid", "Points must be at least 1.", parent=dlg)
                return

            max_rank = int(talent.get("max_rank") or len(talent.get("rank_spell_ids", [])) or 0)
            can_add = min(pts, max_rank - current_rank, available)
            if can_add <= 0:
                messagebox.showerror("Invalid", "Cannot add any more points to this talent.", parent=dlg)
                return

            dep_id = talent.get("depends_on")
            dep_rank = int(talent.get("depends_on_rank") or 0)
            if dep_id:
                dep_talent = next(
                    (t for tree in trees.values()
                     for t in tree.get("talents", [])
                     if int(t.get("talent_id") or 0) == int(dep_id)),
                    None,
                )
                if dep_talent:
                    dep_known = int(current_ranks.get(int(dep_id), 0))
                    if dep_known < dep_rank + 1:
                        messagebox.showerror(
                            "Prerequisite",
                            f"Requires {dep_rank + 1} point(s) in '{dep_talent.get('name', '')}' first.",
                            parent=dlg,
                        )
                        return

            try:
                db.upsert_default_talent_template_entry(
                    self._spec_key,
                    self._class_id,
                    self._template_display_name(trees),
                    int(talent.get("talent_id") or 0),
                    talent.get("name", ""),
                    current_rank + can_add,
                    self._priority_for_talent(talent, tree_idx, trees),
                )
                dlg.destroy()
                self.load_profile(self._profile)
            except Exception as e:
                messagebox.showerror("DB error", str(e), parent=dlg)

        btns = ttk.Frame(dlg)
        btns.pack(fill=tk.X, padx=8, pady=8)
        ttk.Button(btns, text="Apply", command=_apply).pack(side=tk.LEFT, padx=4)
        ttk.Button(btns, text="Cancel", command=dlg.destroy).pack(side=tk.LEFT, padx=4)

    def _reset_talents(self):
        if not self._profile or not self._class_id or not self._spec_key:
            return
        label = f"{self._profile.get('class_key', '')} {self._display_spec_name()}"
        if not messagebox.askyesno(
            "Reset Talent Template",
            f"Remove all saved talent points from the shared {label} template?\n\n"
            "This affects all default profiles for this class/spec regardless of PvE/PvP context."):
            return
        try:
            db.reset_default_talent_template(self._spec_key, self._class_id)
            self.load_profile(self._profile)
            messagebox.showinfo("Done", f"Talent template reset for {label}.")
        except Exception as e:
            messagebox.showerror("DB error", str(e))

    def _copy_from_profile(self):
        if not self._profile or not self._class_id or not self._spec_key:
            return

        profiles = self._profiles_provider() if callable(self._profiles_provider) else []
        same_class = []
        current_id = self._profile.get("default_profile_id")
        current_class = (self._profile.get("class_key") or "").strip()
        current_ctx = (self._profile.get("context_key") or "PvE").strip()

        for profile in profiles:
            if profile.get("default_profile_id") == current_id:
                continue
            if (profile.get("class_key") or "").strip() != current_class:
                continue
            src_ctx = (profile.get("context_key") or "PvE").strip()
            src_spec = (profile.get("spec_key") or "").strip()
            label = f"{profile.get('display_name') or f'{current_class} — {src_spec}'} [{src_ctx}]"
            same_class.append((label, profile))

        if not same_class:
            messagebox.showinfo(
                "No source profiles",
                "There are no other default profiles for this class to copy talents from.")
            return

        dlg = tk.Toplevel(self)
        dlg.title("Copy Talent Template From Profile")
        dlg.resizable(False, False)
        dlg.grab_set()

        ttk.Label(
            dlg,
            text=(
                f"Copy the talent layout into {current_class} {self._display_spec_name()} [{current_ctx}]\n"
                "from another default profile of the same class:"
            ),
            justify="left",
        ).pack(anchor="w", padx=10, pady=(10, 6))

        choice_var = tk.StringVar(value=same_class[0][0])
        combo = ttk.Combobox(dlg, textvariable=choice_var, state="readonly", width=52)
        combo["values"] = [label for label, _profile in same_class]
        combo.current(0)
        combo.pack(fill=tk.X, padx=10, pady=(0, 8))

        def _apply_copy():
            idx = combo.current()
            if idx < 0:
                return
            source = same_class[idx][1]
            src_spec = (source.get("spec_key") or "").strip()
            src_class_id = CLASS_NAME_TO_ID.get((source.get("class_key") or "").strip(), 0)
            src_label = source.get("display_name") or src_spec
            if not messagebox.askyesno(
                "Confirm Copy",
                f"Replace the current talent layout with the template from '{src_label}'?\n\n"
                "This will overwrite the current shared class/spec talent entries.",
                parent=dlg,
            ):
                return
            try:
                db.copy_default_talent_template(
                    src_spec,
                    src_class_id,
                    self._spec_key,
                    self._class_id,
                    self._template_display_name(),
                )
                dlg.destroy()
                self.load_profile(self._profile)
                messagebox.showinfo(
                    "Copied",
                    f"Copied talent entries from '{src_label}' into {current_class} {self._display_spec_name()}.")
            except Exception as e:
                messagebox.showerror("DB error", str(e), parent=dlg)

        btns = ttk.Frame(dlg)
        btns.pack(fill=tk.X, padx=10, pady=(0, 10))
        ttk.Button(btns, text="Copy", command=_apply_copy).pack(side=tk.LEFT, padx=4)
        ttk.Button(btns, text="Cancel", command=dlg.destroy).pack(side=tk.LEFT, padx=4)


# ═══════════════════════════════════════════════════════════════════════════

class DefaultProfilesTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._profiles  = []
        self._sel       = None
        self._ctx_filter = tk.StringVar(value="All")
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # ── Left: profile list ───────────────────────────────────────────────
        left = ttk.Frame(pane, width=220)
        pane.add(left, weight=0)

        ttk.Label(left, text="Default profiles").pack(anchor="w", padx=4, pady=2)
        ttk.Label(left,
            text="These are live rules for world, guild & BG bots and serve as "
                 "the starting template when creating a new account-bot profile.",
            foreground="#555", wraplength=200, justify="left"
        ).pack(anchor="w", padx=4, pady=(0, 4))

        # Context filter bar
        filter_row = ttk.Frame(left)
        filter_row.pack(fill=tk.X, padx=4, pady=(0, 2))
        ttk.Label(filter_row, text="Show:").pack(side=tk.LEFT)
        for ctx in ["All"] + PROFILE_CONTEXTS:
            ttk.Radiobutton(filter_row, text=ctx, value=ctx,
                            variable=self._ctx_filter,
                            command=self._refresh_listbox).pack(side=tk.LEFT, padx=2)

        self._lb = tk.Listbox(left, selectmode=tk.SINGLE, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_select)

        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New",   command=self._new_profile).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="✕ Delete",command=self._del_profile).pack(side=tk.LEFT, padx=2)

        # ── Right: editor ────────────────────────────────────────────────────
        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self._hdr = ProfileHeaderFrame(right, is_default=True)
        self._hdr.pack(fill=tk.X, padx=4, pady=4)

        save_row = ttk.Frame(right)
        save_row.pack(fill=tk.X, padx=4)
        ttk.Button(save_row, text="💾 Save profile header",
                   command=self._save_header).pack(side=tk.LEFT, padx=2)

        ttk.Separator(right, orient="horizontal").pack(fill=tk.X, pady=4)

        profile_nb = ttk.Notebook(right)
        profile_nb.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        combat_tab = ttk.Frame(profile_nb)
        talent_tab = ttk.Frame(profile_nb)
        profile_nb.add(combat_tab, text="  Combat Rotations  ")
        profile_nb.add(talent_tab, text="  Talent Point Layout  ")

        cbs = dict(
            load_entries    = db.load_default_entries,
            upsert_entry    = db.upsert_default_entry,
            delete_entry    = db.delete_default_entry,
            load_actions    = db.load_default_actions,
            upsert_action   = db.upsert_default_action,
            load_conditions = db.load_default_conditions,
            upsert_condition= db.upsert_default_condition,
            delete_condition= db.delete_default_condition,
        )
        self._rot = RotationEditor(combat_tab, cbs)
        self._rot.pack(fill=tk.BOTH, expand=True)

        self._talent = DefaultTalentTemplateEditor(talent_tab)
        self._talent.set_profiles_provider(lambda: self._profiles)
        self._talent.pack(fill=tk.BOTH, expand=True)

        self._hdr._on_class_change_cb = lambda _class_id: self._on_header_identity_changed()
        self._hdr._spec_cb.bind("<<ComboboxSelected>>", self._on_header_identity_changed)

    def refresh(self):
        if not db.ok():
            return
        try:
            self._profiles = db.load_default_profiles()
            self._refresh_listbox()
            self._sel = None
            self._hdr.clear()
            self._rot.clear()
            self._talent.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_header_identity_changed(self, _=None):
        class_id = CLASS_NAME_TO_ID.get(self._hdr.v_class.get())
        self._rot.set_class(class_id)
        if not self._sel:
            self._talent.clear()
            return
        preview = dict(self._sel)
        preview["class_key"] = self._hdr.v_class.get().strip() or preview.get("class_key")
        preview["spec_key"] = self._hdr.v_spec.get().strip() or preview.get("spec_key")
        preview["display_name"] = self._hdr.v_display.get().strip() or preview.get("display_name")
        self._talent.load_profile(preview)

    def _select_profile_id(self, profile_id: int | None):
        if not profile_id:
            return
        idx = next((i for i, row in enumerate(self._profiles)
                    if row.get("default_profile_id") == profile_id), None)
        if idx is None:
            return
        lb_pos = next((j for j, row_idx in enumerate(self._lb_idx) if row_idx == idx), None)
        if lb_pos is None:
            return
        self._lb.selection_clear(0, tk.END)
        self._lb.selection_set(lb_pos)
        self._lb.see(lb_pos)
        self._on_select()

    def _refresh_listbox(self):
        ctx = self._ctx_filter.get()
        self._lb.delete(0, tk.END)
        self._lb_idx: list[int] = []  # maps listbox row → self._profiles index
        for i, p in enumerate(self._profiles):
            p_ctx = p.get("context_key") or "PvE"
            if ctx != "All" and p_ctx != ctx:
                continue
            self._lb.insert(tk.END, self._profile_label(p))
            self._lb_idx.append(i)

    @staticmethod
    def _profile_label(p: dict) -> str:
        cls  = (p.get("class_key")   or "").strip()
        spec = (p.get("spec_key")    or "").strip()
        role = (p.get("role_key")    or "").strip()
        ctx  = (p.get("context_key") or "PvE").strip()
        name = (p.get("display_name") or "").strip()
        is_canonical = (cls, spec, ctx) in CANONICAL_SPEC_LOOKUP
        prefix = "" if is_canonical else "⚠ "
        ctx_badge = f"[{ctx}] " if ctx != "PvE" else ""
        return f"{prefix}{ctx_badge}{name or f'{cls} — {spec}'} ({role})"

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel = self._profiles[self._lb_idx[sel[0]]]
        self._hdr.load(self._sel)
        self._rot.load_profile(self._sel["default_profile_id"])
        self._talent.load_profile(self._sel)

    def _save_header(self):
        if not self._sel:
            return
        self._hdr.collect(self._sel)
        try:
            profile_id = db.upsert_default_profile(self._sel)
            self.refresh()
            self._select_profile_id(profile_id)
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _new_profile(self):
        if not db.ok():
            return
        covered = {(p.get("class_key", "").strip(),
                    p.get("spec_key",   "").strip(),
                    (p.get("context_key") or "PvE").strip())
                   for p in self._profiles}
        available = [(cls, spec, role, ctx, name)
                     for cls, spec, role, ctx, name in CANONICAL_DEFAULT_PROFILES
                     if (cls, spec, ctx) not in covered]
        if not available:
            messagebox.showinfo("Complete",
                "All 60 canonical class/spec/context default profiles already exist.")
            return

        win = tk.Toplevel(self.winfo_toplevel())
        win.title("New Default Profile")
        win.grab_set()
        win.resizable(False, False)

        ttk.Label(win, text="Select a class / spec / context:",
                  wraplength=380).pack(padx=12, pady=(10, 4))

        # Context filter inside the picker
        filter_var = tk.StringVar(value="All")
        frow = ttk.Frame(win)
        frow.pack(padx=12, fill=tk.X)
        ttk.Label(frow, text="Filter:").pack(side=tk.LEFT)
        for ctx in ["All"] + PROFILE_CONTEXTS:
            ttk.Radiobutton(frow, text=ctx, value=ctx,
                            variable=filter_var).pack(side=tk.LEFT, padx=2)

        lb = tk.Listbox(win, selectmode=tk.SINGLE, exportselection=False,
                        width=48, height=min(len(available), 18))
        lb.pack(padx=12, fill=tk.BOTH, expand=True)

        _visible: list[tuple] = []

        def _repopulate(*_):
            lb.delete(0, tk.END)
            _visible.clear()
            fv = filter_var.get()
            for row in available:
                if fv != "All" and row[3] != fv:
                    continue
                _visible.append(row)
                ctx_badge = f"[{row[3]}] " if row[3] != "PvE" else ""
                lb.insert(tk.END, f"{ctx_badge}{row[4]}  ({row[2]})")
            if _visible:
                lb.selection_set(0)

        filter_var.trace_add("write", _repopulate)
        _repopulate()

        chosen: list[tuple] = []

        def _ok():
            sel = lb.curselection()
            if sel:
                chosen.append(_visible[sel[0]])
            win.destroy()

        def _cancel():
            win.destroy()

        btn_row = ttk.Frame(win)
        btn_row.pack(pady=8)
        ttk.Button(btn_row, text="Create", command=_ok).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_row, text="Cancel", command=_cancel).pack(side=tk.LEFT, padx=4)
        lb.bind("<Double-1>", lambda _: _ok())
        win.wait_window()

        if not chosen:
            return
        cls, spec, role, ctx, name = chosen[0]
        p = dict(class_key=cls, spec_key=spec, role_key=role,
                 context_key=ctx, display_name=name,
                 conservation_mode=1, resource_low_water=55, resource_high_water=75,
                 enable_down_rank=1, down_rank_floor=2,
                 default_aoe_mode=0, default_aoe_min_targets=2, default_aoe_scan_radius=10.0,
                 targeting_mode=1 if ctx == "PvE" else 2,
                 current_target_bias=80.0, assist_target_bias=140.0, focus_fire_bias=55.0,
                 protect_ally_bias=170.0, prefer_healer_bias=220.0, prefer_dps_bias=140.0,
                 avoid_tank_bias=120.0)
        try:
            pid = db.upsert_default_profile(p)
            p["default_profile_id"] = pid
            self.refresh()
            self._select_profile_id(pid)
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _del_profile(self):
        if not self._sel:
            return
        if not messagebox.askyesno("Confirm",
                f"Delete '{self._sel.get('display_name')}' and all its entries?"):
            return
        try:
            db.delete_default_profile(self._sel["default_profile_id"])
            self._sel = None
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


# ═══════════════════════════════════════════════════════════════════════════
#  TAB: BOT PROFILES  (per source character)
# ═══════════════════════════════════════════════════════════════════════════

class BotProfilesTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._accounts = []
        self._chars    = []
        self._profiles = []
        self._account_by_label = {}
        self._selected_account_id = None
        self._sel_char = None
        self._sel_prof = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # ── Left: character + profile slot list ─────────────────────────────
        left = ttk.Frame(pane, width=240)
        pane.add(left, weight=0)

        ttk.Label(left, text="Account").pack(anchor="w", padx=4, pady=(4, 0))
        self.v_account = tk.StringVar()
        self._acct_cb = ttk.Combobox(left, textvariable=self.v_account, state="readonly", width=34)
        self._acct_cb.pack(fill=tk.X, padx=4)
        self._acct_cb.bind("<<ComboboxSelected>>", self._on_account_select)

        ttk.Label(left, text="Characters").pack(anchor="w", padx=4, pady=(4, 0))
        char_cols = ("name", "lvl", "class")
        self._char_tv = ttk.Treeview(left, columns=char_cols, show="headings",
                                     height=8, selectmode="browse")
        self._char_tv.heading("name",  text="Name")
        self._char_tv.heading("lvl",   text="Lvl")
        self._char_tv.heading("class", text="Class")
        self._char_tv.column("name",  width=100)
        self._char_tv.column("lvl",   width=30, anchor="center")
        self._char_tv.column("class", width=80)
        self._char_tv.pack(fill=tk.X, padx=4)
        self._char_tv.bind("<<TreeviewSelect>>", self._on_char_select)

        ttk.Separator(left, orient="horizontal").pack(fill=tk.X, pady=4)

        ttk.Label(left, text="Clone profile slots (1-10)").pack(anchor="w", padx=4)
        self._prof_lb = tk.Listbox(left, selectmode=tk.SINGLE, exportselection=False, height=10)
        self._prof_lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._prof_lb.bind("<<ListboxSelect>>", self._on_prof_select)

        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New slot",  command=self._new_profile).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="✕ Delete",    command=self._del_profile).pack(side=tk.LEFT, padx=2)

        # ── Right: character-level OOC behaviour + per-slot profile editor ───
        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        # OOC panel — character-level, one set of settings regardless of slot.
        self._ooc = OocProfilePanel(right)
        self._ooc.pack(fill=tk.X, padx=4, pady=4)

        ttk.Separator(right, orient="horizontal").pack(fill=tk.X)

        # Profile header and slot-level editor below.
        slot_frame = ttk.Frame(right)
        slot_frame.pack(fill=tk.BOTH, expand=True)

        self._hdr = ProfileHeaderFrame(slot_frame, is_default=False)
        self._hdr.pack(fill=tk.X, padx=4, pady=4)

        save_row = ttk.Frame(slot_frame)
        save_row.pack(fill=tk.X, padx=4)
        ttk.Button(save_row, text="💾 Save profile",
                   command=self._save_header).pack(side=tk.LEFT, padx=2)

        ttk.Separator(slot_frame, orient="horizontal").pack(fill=tk.X, pady=4)

        profile_nb = ttk.Notebook(slot_frame)
        profile_nb.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        combat_tab = ttk.Frame(profile_nb)
        profile_nb.add(combat_tab, text="  Combat  ")

        cbs = dict(
            load_entries    = db.load_bot_entries,
            upsert_entry    = db.upsert_bot_entry,
            delete_entry    = db.delete_bot_entry,
            load_actions    = db.load_bot_actions,
            upsert_action   = db.upsert_bot_action,
            load_conditions = db.load_bot_conditions,
            upsert_condition= db.upsert_bot_condition,
            delete_condition= db.delete_bot_condition,
        )
        self._rot = RotationEditor(combat_tab, cbs)
        self._hdr._on_class_change_cb = self._rot.set_class
        self._rot.pack(fill=tk.BOTH, expand=True)

    def _account_label(self, row: dict) -> str:
        return f"{row.get('username', '')} [{row.get('account_id')}] ({row.get('char_count', 0)} chars)"

    def _select_account_id(self, account_id: int | None):
        if not self._accounts:
            self.v_account.set("")
            self._selected_account_id = None
            return

        if account_id is None:
            account_id = self._accounts[0].get("account_id")

        for row in self._accounts:
            if row.get("account_id") == account_id:
                label = self._account_label(row)
                self.v_account.set(label)
                self._selected_account_id = account_id
                self._load_characters_for_selected_account()
                return

        first = self._accounts[0]
        self.v_account.set(self._account_label(first))
        self._selected_account_id = first.get("account_id")
        self._load_characters_for_selected_account()

    def _load_characters_for_selected_account(self):
        self._char_tv.delete(*self._char_tv.get_children())
        self._sel_char = None
        self._sel_prof = None
        self._profiles = []
        self._prof_lb.delete(0, tk.END)
        self._hdr.clear()
        self._rot.clear()

        if not self._selected_account_id:
            return

        self._chars = db.load_source_characters_for_account(self._selected_account_id)
        for c in self._chars:
            cls = WOW_CLASSES.get(c.get("class", 0), str(c.get("class", "")))
            self._char_tv.insert("", "end", iid=str(c["guid"]),
                                 values=(c["name"], c["level"], cls))

    def _on_account_select(self, _=None):
        label = self.v_account.get()
        account_id = self._account_by_label.get(label)
        self._selected_account_id = account_id
        app = self.winfo_toplevel()
        if hasattr(app, "set_preferred_game_account") and account_id:
            app.set_preferred_game_account(account_id)
        self._load_characters_for_selected_account()

    def refresh(self):
        if not db.ok():
            return
        try:
            self._accounts = db.load_bot_pool_accounts()
            labels = [self._account_label(row) for row in self._accounts]
            self._account_by_label = {self._account_label(row): row.get("account_id") for row in self._accounts}
            self._acct_cb.configure(values=labels)

            preferred_id = None
            app = self.winfo_toplevel()
            if hasattr(app, "get_preferred_game_account"):
                preferred_id = app.get_preferred_game_account()

            self._select_account_id(preferred_id)
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_char_select(self, _=None):
        sel = self._char_tv.selection()
        if not sel:
            return
        guid = int(sel[0])
        self._sel_char = next((c for c in self._chars if c["guid"] == guid), None)
        if not self._sel_char:
            return
        # Pre-load class spells so the rotation editor is ready when a profile loads
        self._rot.set_class(self._sel_char.get("class"))
        self._hdr.set_class_from_character(self._sel_char.get("class"))
        self._ooc.load_for_char(self._sel_char["guid"])
        try:
            self._profiles = db.load_bot_profiles(guid)
            self._prof_lb.delete(0, tk.END)
            for p in self._profiles:
                self._prof_lb.insert(tk.END,
                    f"Slot {p['slot']}: {p.get('profile_name') or '(unnamed)'}")
            self._sel_prof = None
            self._hdr.clear()
            self._ooc.clear()
            self._rot.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_prof_select(self, _=None):
        sel = self._prof_lb.curselection()
        if not sel:
            return
        self._sel_prof = self._profiles[sel[0]]
        self._hdr.load(self._sel_prof, forced_class_id=self._sel_char.get("class") if self._sel_char else None)
        self._ooc.load(self._sel_prof)
        self._rot.load_profile(self._sel_prof["profile_id"])

    def _save_header(self):
        if not self._sel_prof or not self._sel_char:
            return
        self._hdr.collect(self._sel_prof)
        try:
            db.upsert_bot_profile(self._sel_prof)
            self._on_char_select()   # refresh slot list
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _default_profile_options_for_class(self, class_id: int) -> list[tuple[str, int | None]]:
        options = [("Blank slate", None)]
        try:
            defaults = db.load_default_profiles()
        except MySQLError:
            return options

        for p in defaults:
            pid = p.get("default_profile_id")
            profile_class = _class_from_spec(p.get("spec_key", ""), p.get("display_name", ""))
            if profile_class != class_id:
                continue
            label = p.get("display_name") or f"{p.get('spec_key', '')} / {p.get('role_key', '')}"
            options.append((label, pid))
        return options

    def _copy_default_profile_to_bot(self, default_profile_id: int, bot_profile_id: int):
        defaults = {p["default_profile_id"]: p for p in db.load_default_profiles()}
        src = defaults.get(default_profile_id)
        if not src:
            return

        target = next((p for p in self._profiles if p.get("profile_id") == bot_profile_id), None)
        if not target:
            rows = db.load_bot_profiles(self._sel_char["guid"])
            target = next((p for p in rows if p.get("profile_id") == bot_profile_id), None)
        if not target:
            return

        target["spec_override_key"] = src.get("spec_key") or None
        target["role_override_key"] = _normalize_role(src.get("role_key", "")) or None
        target["conservation_mode"] = src.get("conservation_mode", 1)
        target["resource_low_water"] = src.get("resource_low_water", 55)
        target["resource_high_water"] = src.get("resource_high_water", 75)
        target["enable_down_rank"] = src.get("enable_down_rank", 1)
        target["down_rank_floor"] = src.get("down_rank_floor", 2)
        if "default_aoe_mode" in src:
            target["default_aoe_mode"] = src.get("default_aoe_mode", 0)
        if "default_aoe_min_targets" in src:
            target["default_aoe_min_targets"] = src.get("default_aoe_min_targets", 2)
        if "default_aoe_scan_radius" in src:
            target["default_aoe_scan_radius"] = src.get("default_aoe_scan_radius", 10.0)
        for key, default in (
            ("targeting_mode", 0),
            ("current_target_bias", 80.0),
            ("assist_target_bias", 140.0),
            ("focus_fire_bias", 55.0),
            ("protect_ally_bias", 170.0),
            ("prefer_healer_bias", 220.0),
            ("prefer_dps_bias", 140.0),
            ("avoid_tank_bias", 120.0),
        ):
            if key in src:
                target[key] = src.get(key, default)
        db.upsert_bot_profile(target)

        for src_entry in db.load_default_entries(default_profile_id):
            new_entry = dict(
                priority=src_entry.get("priority", 0),
                label=src_entry.get("label", ""),
                is_interrupt=src_entry.get("is_interrupt", 0),
                breaks_current_cast=src_entry.get("breaks_current_cast", 0),
                enabled=src_entry.get("enabled", 1),
                condition_logic=src_entry.get("condition_logic", 0),
            )
            new_entry_id = db.upsert_bot_entry(new_entry, bot_profile_id)

            for src_action in db.load_default_actions(src_entry["entry_id"]):
                new_action = dict(
                    slot=src_action.get("slot", 0),
                    action_type=src_action.get("action_type", 0),
                    spell_base_id=src_action.get("spell_base_id", 0),
                    item_id=src_action.get("item_id", 0),
                    rank_mode=src_action.get("rank_mode", 0),
                    rank_value=src_action.get("rank_value", 0),
                    target_key=src_action.get("target_key", "enemy"),
                    aoe_mode=src_action.get("aoe_mode"),
                    aoe_min_targets=src_action.get("aoe_min_targets"),
                    aoe_radius=src_action.get("aoe_radius"),
                )
                db.upsert_bot_action(new_action, new_entry_id)

            for src_cond in db.load_default_conditions(src_entry["entry_id"]):
                new_cond = dict(
                    sequence=src_cond.get("sequence", 0),
                    subject_key=src_cond.get("subject_key", SUBJECT_KEYS[0]),
                    stat_key=src_cond.get("stat_key", STAT_KEYS[0]),
                    comparison=src_cond.get("comparison", 4),
                    numeric_value=src_cond.get("numeric_value", 0),
                    string_value=src_cond.get("string_value", ""),
                )
                db.upsert_bot_condition(new_cond, new_entry_id)

    def _new_profile(self):
        try:
            if not db.ok():
                return
            if not self._sel_char:
                messagebox.showinfo("Select character",
                                    "Select a character on the left before creating a clone profile slot.")
                return
            used_slots = {p["slot"] for p in self._profiles}
            slot = next((s for s in range(1, 11) if s not in used_slots), None)
            if slot is None:
                messagebox.showinfo("Full", "All 10 profile slots are already used.")
                return
            picker = DefaultProfilePicker(
                self.winfo_toplevel(),
                self._default_profile_options_for_class(self._sel_char.get("class")),
                title="New Clone Profile")
            if picker.cancelled:
                return
            p = dict(source_character_guid=self._sel_char["guid"],
                     owner_account_id=self._sel_char["account"],
                     slot=slot, profile_name=f"Profile {slot}",
                     guessed_spec_key="", guessed_role_key="DPS",
                     spec_override_key=None, role_override_key=None,
                     conservation_mode=1, resource_low_water=55, resource_high_water=75,
                     enable_down_rank=1, down_rank_floor=2,
                     default_aoe_mode=0, default_aoe_min_targets=2,
                     default_aoe_scan_radius=10.0,
                     targeting_mode=0, current_target_bias=80.0,
                     assist_target_bias=140.0, focus_fire_bias=55.0,
                     protect_ally_bias=170.0, prefer_healer_bias=220.0,
                     prefer_dps_bias=140.0, avoid_tank_bias=120.0)
            new_profile_id = db.upsert_bot_profile(p)
            if picker.selection is not None:
                self._copy_default_profile_to_bot(picker.selection, new_profile_id)
            self._on_char_select()
            idx = next((i for i, prof in enumerate(self._profiles)
                        if prof.get("profile_id") == new_profile_id), None)
            if idx is not None:
                self._prof_lb.selection_clear(0, tk.END)
                self._prof_lb.selection_set(idx)
                self._on_prof_select()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))
        except Exception as e:
            messagebox.showerror("New profile error", str(e))

    def _del_profile(self):
        if not self._sel_prof:
            return
        if not messagebox.askyesno("Confirm",
                f"Delete slot {self._sel_prof['slot']} and all its entries?"):
            return
        try:
            db.delete_bot_profile(self._sel_prof["profile_id"])
            self._sel_prof = None
            self._on_char_select()
            self._ooc.clear()
            self._rot.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))






