"""
tab_bots.py -- DefaultProfilesTab and BotProfilesTab.
"""
import tkinter as tk
from tkinter import ttk, messagebox
from mysql.connector import Error as MySQLError
from .constants import (
    WOW_CLASSES, ROLE_OPTS, SPEC_TO_CLASS, SPEC_ALIAS_TO_CLASS,
    CONSERVATION_MODES, AOE_MODES,
    STAT_KEYS, SUBJECT_KEYS,
    _normalize_role,
)
from .db import db
from .helpers import lbl, entry_w, combo_w, check_w, unix_text
from .widgets import ProfileHeaderFrame, DefaultProfilePicker
from .rotation import RotationEditor, _class_from_spec
from .ooc_panel import OocProfilePanel

# ═══════════════════════════════════════════════════════════════════════════

class DefaultProfilesTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._profiles  = []
        self._sel       = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # ── Left: profile list ───────────────────────────────────────────────
        left = ttk.Frame(pane, width=220)
        pane.add(left, weight=0)

        ttk.Label(left, text="Default profiles").pack(anchor="w", padx=4, pady=2)
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
        self._rot = RotationEditor(right, cbs)
        self._hdr._on_class_change_cb = self._rot.set_class
        self._rot.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

    def refresh(self):
        if not db.ok():
            return
        try:
            self._profiles = db.load_default_profiles()
            self._lb.delete(0, tk.END)
            for p in self._profiles:
                self._lb.insert(tk.END, p.get("display_name") or
                                f"{p['spec_key']} {p['role_key']}")
            self._sel = None
            self._hdr.clear()
            self._rot.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel = self._profiles[sel[0]]
        self._hdr.load(self._sel)
        self._rot.load_profile(self._sel["default_profile_id"])

    def _save_header(self):
        if not self._sel:
            return
        self._hdr.collect(self._sel)
        try:
            db.upsert_default_profile(self._sel)
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _new_profile(self):
        if not db.ok():
            return
        existing = {(p.get("spec_key", ""), _normalize_role(p.get("role_key", "")))
                    for p in self._profiles}
        next_idx = 1
        while (f"NewSpec{next_idx}", "DPS") in existing:
            next_idx += 1
        p = dict(spec_key=f"NewSpec{next_idx}", role_key="DPS",
                 display_name=f"New Profile {next_idx}",
                 conservation_mode=1, mana_low_water=55, mana_high_water=75,
                 enable_down_rank=1, down_rank_floor=2,
                 default_aoe_mode=0, default_aoe_min_targets=2, default_aoe_scan_radius=10.0)
        try:
            pid = db.upsert_default_profile(p)
            p["default_profile_id"] = pid
            self.refresh()
            # Select the new one
            idx = next((i for i, x in enumerate(self._profiles)
                        if x["default_profile_id"] == pid), None)
            if idx is not None:
                self._lb.selection_set(idx)
                self._on_select()
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

        ttk.Label(left, text="Profile slots (1-10)").pack(anchor="w", padx=4)
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
        target["mana_low_water"] = src.get("mana_low_water", 55)
        target["mana_high_water"] = src.get("mana_high_water", 75)
        target["enable_down_rank"] = src.get("enable_down_rank", 1)
        target["down_rank_floor"] = src.get("down_rank_floor", 2)
        if "default_aoe_mode" in src:
            target["default_aoe_mode"] = src.get("default_aoe_mode", 0)
        if "default_aoe_min_targets" in src:
            target["default_aoe_min_targets"] = src.get("default_aoe_min_targets", 2)
        if "default_aoe_scan_radius" in src:
            target["default_aoe_scan_radius"] = src.get("default_aoe_scan_radius", 10.0)
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
                                    "Select a character on the left before creating a bot profile slot.")
                return
            used_slots = {p["slot"] for p in self._profiles}
            slot = next((s for s in range(1, 11) if s not in used_slots), None)
            if slot is None:
                messagebox.showinfo("Full", "All 10 profile slots are already used.")
                return
            picker = DefaultProfilePicker(
                self.winfo_toplevel(),
                self._default_profile_options_for_class(self._sel_char.get("class")),
                title="New Bot Profile")
            if picker.cancelled:
                return
            p = dict(source_character_guid=self._sel_char["guid"],
                     owner_account_id=self._sel_char["account"],
                     slot=slot, profile_name=f"Profile {slot}",
                     guessed_spec_key="", guessed_role_key="DPS",
                     spec_override_key=None, role_override_key=None,
                     conservation_mode=1, mana_low_water=55, mana_high_water=75,
                     enable_down_rank=1, down_rank_floor=2,
                     default_aoe_mode=0, default_aoe_min_targets=2,
                     default_aoe_scan_radius=10.0)
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


