"""
rotation.py -- RotationEditor widget and spec/class helpers.
"""
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from mysql.connector import Error as MySQLError
from .constants import (
    WOW_CLASSES, CLASS_SPELL_FAMILY, CLASS_NAME_TO_ID,
    ACTION_TYPES, ACTION_INV, ACTION_OPTS,
    RANK_MODES, RANK_INV, RANK_OPTS,
    COND_LOGIC, COND_LOGIC_INV, COND_LOGIC_OPTS,
    COND_OPS, COND_OPS_INV, COND_OPS_OPTS,
    TARGET_KEYS, SUBJECT_KEYS, STAT_KEYS, BOOL_STAT_KEYS,
    AOE_MODES, AOE_INV, AOE_OPTS,
    SPEC_TO_CLASS, SPEC_ALIAS_TO_CLASS,
)
from .db import db
from .helpers import lbl, entry_w, combo_w, check_w

# ═══════════════════════════════════════════════════════════════════════════

class RotationEditor(ttk.Frame):
    """
    Full rotation editor.  Caller provides load/save callback bundles
    so the same widget works for both default and per-bot profiles.
    """

    def __init__(self, parent, cbs: dict, **kw):
        """
        cbs keys:
          load_entries(profile_id) -> [dict]
          upsert_entry(e, profile_id) -> entry_id
          delete_entry(entry_id)
          load_actions(entry_id) -> [dict]
          upsert_action(a, entry_id)
          load_conditions(entry_id) -> [dict]
          upsert_condition(c, entry_id)
          delete_condition(condition_id)
        """
        super().__init__(parent, **kw)
        self._cbs          = cbs
        self._profile_id   = None
        self._entries      = []          # list of entry dicts (in display order)
        self._sel_entry    = None        # currently selected entry dict
        self._actions      = {}          # slot -> action dict (may be empty)
        self._conditions   = []          # list of condition dicts
        self._class_id     = None        # current WoW class_id for spell list
        self._class_spells = []          # [{"id": int, "display": str}]
        self._build()

    def _build(self):
        # ── left: entry list ────────────────────────────────────────────────
        left = ttk.Frame(self)
        left.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 4))

        ttk.Label(left, text="Rotation entries (priority order)").pack(anchor="w")

        cols = ("pri", "label", "flags")
        self._tv = ttk.Treeview(left, columns=cols, show="headings",
                                height=14, selectmode="browse")
        self._tv.heading("pri",   text="Pri")
        self._tv.heading("label", text="Label")
        self._tv.heading("flags", text="Flags")
        self._tv.column("pri",   width=30, anchor="center")
        self._tv.column("label", width=160)
        self._tv.column("flags", width=80, anchor="center")
        self._tv.pack(fill=tk.Y, expand=True)
        self._tv.bind("<<TreeviewSelect>>", self._on_entry_select)

        btn_row = ttk.Frame(left)
        btn_row.pack(fill=tk.X, pady=2)
        ttk.Button(btn_row, text="+ Add",    command=self._add_entry).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_row, text="↑",        command=self._move_up).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_row, text="↓",        command=self._move_down).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_row, text="✕ Remove", command=self._del_entry).pack(side=tk.LEFT, padx=2)

        # ── right: detail panel ──────────────────────────────────────────────
        right = ttk.Frame(self)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Entry header
        hdr = ttk.LabelFrame(right, text="Entry settings", padding=4)
        hdr.pack(fill=tk.X, pady=(0, 4))

        lbl(hdr, "Label:",    0, 0)
        self.v_label    = tk.StringVar()
        entry_w(hdr, self.v_label, 0, 1, width=20)
        lbl(hdr, "Priority:", 0, 2)
        self.v_priority = tk.StringVar()
        entry_w(hdr, self.v_priority, 0, 3, width=4)

        self.v_enabled   = tk.BooleanVar(value=True)
        self.v_interrupt = tk.BooleanVar()
        self.v_break     = tk.BooleanVar()
        check_w(hdr, self.v_enabled,   "Enabled",      1, 0)
        check_w(hdr, self.v_interrupt, "Interrupt",     1, 1)
        check_w(hdr, self.v_break,     "Break cast",   1, 2)
        lbl(hdr, "Cond logic:", 1, 3)
        self.v_cond_logic = tk.StringVar(value=COND_LOGIC[0])
        combo_w(hdr, self.v_cond_logic, COND_LOGIC_OPTS, 1, 4, width=10)

        ttk.Button(hdr, text="Save entry", command=self._save_entry).grid(
            row=0, column=5, rowspan=2, padx=8, sticky="ns")

        # Actions (primary + secondary in sub-frames)
        act_frame = ttk.LabelFrame(right, text="Actions", padding=4)
        act_frame.pack(fill=tk.X, pady=(0, 4))
        self._action_widgets = {}
        for slot, label in ((0, "Primary"), (1, "Secondary")):
            self._action_widgets[slot] = self._build_action_row(act_frame, label, slot)

        # Conditions
        cond_frame = ttk.LabelFrame(right, text="Conditions", padding=4)
        cond_frame.pack(fill=tk.BOTH, expand=True)
        self._build_condition_panel(cond_frame)

        self._set_detail_enabled(False)

    # ── Action row builder ───────────────────────────────────────────────────

    def _build_action_row(self, parent, label: str, slot: int) -> dict:
        f = ttk.Frame(parent)
        f.pack(fill=tk.X, pady=2)
        ttk.Label(f, text=f"{label}:", width=10).pack(side=tk.LEFT)

        v = {}
        v["type"]         = tk.StringVar(value=ACTION_TYPES[0])
        v["spell_id"]     = tk.StringVar()  # spell_base_id for save
        v["item_id"]      = tk.StringVar()  # item_id for save
        v["combo_text"]   = tk.StringVar()  # what the combo box shows
        v["id_display"]   = tk.StringVar()  # resolved ID shown in badge (or "INF")
        v["rank_mode"]    = tk.StringVar(value=RANK_MODES[0])
        v["rank_val"]     = tk.StringVar(value="0")
        v["target"]       = tk.StringVar(value="enemy")
        v["aoe_mode"]     = tk.StringVar(value="")
        v["aoe_min"]      = tk.StringVar(value="")
        v["aoe_radius"]   = tk.StringVar(value="")

        # ── Type picker (Spell / Item) ──────────────────────────────────────
        type_cb = ttk.Combobox(f, textvariable=v["type"], values=ACTION_OPTS,
                               state="readonly", width=7)
        type_cb.pack(side=tk.LEFT, padx=2)
        v["_type_widget"] = type_cb

        def _on_type_changed(event, _v=v):
            self._on_type_change(_v)
        type_cb.bind("<<ComboboxSelected>>", _on_type_changed)

        # ── Spell / Item picker combo ───────────────────────────────────────
        # Spell mode: pre-filled with class spell list.
        # Item  mode: blank — user types a name and presses Enter to search.
        combo = ttk.Combobox(f, textvariable=v["combo_text"], values=[], width=32)
        combo.pack(side=tk.LEFT, padx=2)
        v["_combo_widget"] = combo

        def _on_combo_select(event, _v=v):
            if _v["type"].get() == "Spell":
                self._on_spell_combo_select(_v)
            else:
                self._on_item_combo_select(_v)
        combo.bind("<<ComboboxSelected>>", _on_combo_select)

        def _on_combo_action(event, _v=v):
            if _v["type"].get() == "Spell":
                self._on_spell_combo_typed(_v)
            else:
                self._on_item_search(_v)
        combo.bind("<Return>",   _on_combo_action)
        combo.bind("<FocusOut>", _on_combo_action)

        # ── Read-only ID badge ──────────────────────────────────────────────
        ttk.Label(f, text="Rank:").pack(side=tk.LEFT, padx=(4, 0))
        ttk.Combobox(f, textvariable=v["rank_mode"], values=RANK_OPTS,
                     state="readonly", width=12).pack(side=tk.LEFT, padx=2)
        ttk.Entry(f, textvariable=v["rank_val"], width=3).pack(side=tk.LEFT, padx=2)
        ttk.Label(f, text="Target:").pack(side=tk.LEFT, padx=(4, 0))
        ttk.Combobox(f, textvariable=v["target"], values=TARGET_KEYS,
                     state="readonly", width=14).pack(side=tk.LEFT, padx=2)

        ttk.Label(f, text="AoE:").pack(side=tk.LEFT, padx=(4, 0))
        ttk.Combobox(f, textvariable=v["aoe_mode"], values=[""] + AOE_OPTS,
                     state="readonly", width=10).pack(side=tk.LEFT, padx=2)
        ttk.Label(f, text="Min:").pack(side=tk.LEFT)
        ttk.Entry(f, textvariable=v["aoe_min"], width=3).pack(side=tk.LEFT, padx=2)
        ttk.Label(f, text="R:").pack(side=tk.LEFT)
        ttk.Entry(f, textvariable=v["aoe_radius"], width=4).pack(side=tk.LEFT, padx=2)
        return v

    # ── Spell / Item combo helpers ───────────────────────────────────────────

    def set_class(self, class_id: int | None):
        """Load spells for class_id; populate combos currently in Spell mode."""
        self._class_id     = class_id
        self._class_spells = db.load_class_spells(class_id) if class_id else []
        values = [s["display"] for s in self._class_spells]
        for w in self._action_widgets.values():
            if w["type"].get() == "Spell":
                w["_combo_widget"].configure(values=values)
        if hasattr(self, "_cond_spell_combo"):
            self._cond_spell_combo.configure(values=values)

    def _spell_display_for_id(self, spell_id) -> str:
        """Return 'Name  [ID]' for a spell_id, class list first then DB fallback."""
        if not spell_id:
            return ""
        try:
            sid = int(spell_id)
        except (ValueError, TypeError):
            return ""
        for s in self._class_spells:
            if s["id"] == sid:
                return s["display"]
        name = db.spell_name(sid)
        return f"{name}  [{sid}]" if name else f"#{sid}"

    def _parse_id_from_display(self, display: str):
        """Extract numeric ID from 'Name  [12345]', '#12345', or bare '12345'. Returns int|None."""
        s = display.strip()
        if s.endswith("]") and "[" in s:
            try:
                return int(s.rsplit("[", 1)[-1].rstrip("]").strip())
            except ValueError:
                pass
        if s.startswith("#"):
            try:
                return int(s[1:].strip())
            except ValueError:
                pass
        if s.isdigit():
            return int(s)
        return None

    def _condition_spell_display(self, raw_value) -> str:
        """Render aura spell IDs as 'Name [ID]' where possible."""
        if raw_value in (None, ""):
            return ""
        try:
            sid = int(float(raw_value))
        except (TypeError, ValueError):
            text = str(raw_value).strip()
            sid = self._parse_id_from_display(text)
            if sid is None:
                return text
        name = db.spell_name(sid)
        return f"{name} [{sid}]" if name else str(sid)

    def _condition_desc(self, c: dict) -> str:
        op = COND_OPS.get(c.get("comparison", 4), ">=")
        stat_key = c.get("stat_key", "")
        subject = c.get("subject_key", "")
        string_value = c.get("string_value", "")
        numeric_value = c.get("numeric_value", "")

        if stat_key == "aura":
            raw_value = string_value if string_value not in (None, "") else numeric_value
            value_text = self._condition_spell_display(raw_value)
        elif stat_key == "aura_stacks":
            raw_value = string_value if string_value not in (None, "") else numeric_value
            spell_display = self._condition_spell_display(raw_value)
            value_text = f"{spell_display} stacks={numeric_value}"
        elif stat_key in BOOL_STAT_KEYS:
            try:
                value_text = "True" if int(float(numeric_value or 0)) else "False"
            except (TypeError, ValueError):
                value_text = str(numeric_value)
        else:
            value_text = str(numeric_value)
            if string_value:
                value_text = f"{value_text} '{string_value}'"

        return f"{subject}.{stat_key} {op} {value_text}".strip()

    def _sync_condition_value_editor(self):
        aura_mode = self.v_stat.get() == "aura"
        aura_stacks_mode = self.v_stat.get() == "aura_stacks"
        bool_mode = self.v_stat.get() in BOOL_STAT_KEYS
        if aura_mode:
            self._cond_value_label.pack_forget()
            self._cond_nval_entry.pack_forget()
            self._cond_string_label.pack_forget()
            self._cond_sval_entry.pack_forget()
            self._cond_bool_label.pack_forget()
            self._cond_bool_combo.pack_forget()
            self._cond_aura_label.pack(side=tk.LEFT)
            self._cond_spell_combo.pack(side=tk.LEFT, padx=2)
        elif aura_stacks_mode:
            self._cond_bool_label.pack_forget()
            self._cond_bool_combo.pack_forget()
            self._cond_aura_label.pack(side=tk.LEFT)
            self._cond_spell_combo.pack(side=tk.LEFT, padx=2)
            self._cond_value_label.pack(side=tk.LEFT)
            self._cond_nval_entry.pack(side=tk.LEFT, padx=2)
            self._cond_string_label.pack_forget()
            self._cond_sval_entry.pack_forget()
        elif bool_mode:
            self._cond_aura_label.pack_forget()
            self._cond_spell_combo.pack_forget()
            self._cond_value_label.pack_forget()
            self._cond_nval_entry.pack_forget()
            self._cond_string_label.pack_forget()
            self._cond_sval_entry.pack_forget()
            self._cond_bool_label.pack(side=tk.LEFT)
            self._cond_bool_combo.pack(side=tk.LEFT, padx=2)
        else:
            self._cond_aura_label.pack_forget()
            self._cond_spell_combo.pack_forget()
            self._cond_bool_label.pack_forget()
            self._cond_bool_combo.pack_forget()
            self._cond_value_label.pack(side=tk.LEFT)
            self._cond_nval_entry.pack(side=tk.LEFT, padx=2)
            self._cond_string_label.pack(side=tk.LEFT)
            self._cond_sval_entry.pack(side=tk.LEFT, padx=2)

    def _on_cond_stat_changed(self, _=None):
        if self.v_stat.get() == "aura":
            raw_value = self.v_sval.get().strip() or self.v_nval.get().strip()
            self.v_cond_spell.set(self._condition_spell_display(raw_value))
        elif self.v_stat.get() == "aura_stacks":
            raw_value = self.v_sval.get().strip()
            self.v_cond_spell.set(self._condition_spell_display(raw_value))
        elif self.v_stat.get() in BOOL_STAT_KEYS:
            try:
                self.v_cond_bool.set("True" if int(float(self.v_nval.get() or 0)) else "False")
            except (TypeError, ValueError):
                self.v_cond_bool.set("False")
        self._sync_condition_value_editor()

    def _on_cond_bool_changed(self, _=None):
        self.v_nval.set("1" if self.v_cond_bool.get() == "True" else "0")
        self.v_sval.set("")

    def _on_cond_spell_pick(self, _=None):
        sid = self._parse_id_from_display(self.v_cond_spell.get())
        if sid is not None:
            if self.v_stat.get() == "aura_stacks":
                self.v_sval.set(str(sid))
            else:
                self.v_nval.set(str(sid))
                self.v_sval.set("")
            self.v_cond_spell.set(self._spell_display_for_id(sid))

    def _on_cond_spell_typed(self, _=None):
        raw = self.v_cond_spell.get().strip()
        if not raw:
            if self.v_stat.get() == "aura_stacks":
                self.v_sval.set("")
            else:
                self.v_nval.set("0")
                self.v_sval.set("")
            return
        sid = self._parse_id_from_display(raw)
        if sid is not None:
            if self.v_stat.get() == "aura_stacks":
                self.v_sval.set(str(sid))
            else:
                self.v_nval.set(str(sid))
                self.v_sval.set("")
            self.v_cond_spell.set(self._spell_display_for_id(sid))

    # ── Type toggle ─────────────────────────────────────────────────────────

    def _on_type_change(self, v: dict):
        """User switched Spell ↔ Item — reset combo and badge."""
        v["spell_id"].set("")
        v["item_id"].set("")
        v["combo_text"].set("")
        v["id_display"].set("")
        if v["type"].get() == "Spell":
            values = [s["display"] for s in self._class_spells]
            v["_combo_widget"].configure(values=values)
        else:
            v["_combo_widget"].configure(values=[])

    # ── Spell-mode handlers ──────────────────────────────────────────────────

    def _on_spell_combo_select(self, v: dict):
        """User picked a spell from the dropdown."""
        sid = self._parse_id_from_display(v["combo_text"].get())
        if sid is not None:
            v["spell_id"].set(str(sid))
            v["id_display"].set(str(sid))
        else:
            v["spell_id"].set("")
            v["id_display"].set("")

    def _on_spell_combo_typed(self, v: dict):
        """User typed a raw ID or name in spell combo; resolve to display form."""
        raw = v["combo_text"].get().strip()
        sid = self._parse_id_from_display(raw)
        if sid is not None:
            v["spell_id"].set(str(sid))
            v["id_display"].set(str(sid))
            v["combo_text"].set(self._spell_display_for_id(sid))

    # ── Item-mode handlers ───────────────────────────────────────────────────

    def _on_item_search(self, v: dict):
        """User pressed Enter in Item mode — search item_template by name."""
        raw = v["combo_text"].get().strip()
        if not raw:
            return
        # Bare numeric ID → reverse lookup
        if raw.isdigit():
            iid = int(raw)
            name = db.item_name(iid)
            if name:
                display = f"{name}  [{iid}]"
                v["item_id"].set(str(iid))
                v["id_display"].set(str(iid))
                v["combo_text"].set(display)
                v["_combo_widget"].configure(values=[display])
            else:
                v["item_id"].set("")
                v["id_display"].set("INF")
            return
        # Already resolved "Name [ID]" format
        sid = self._parse_id_from_display(raw)
        if sid is not None:
            v["item_id"].set(str(sid))
            v["id_display"].set(str(sid))
            return
        # Name search
        results = db.search_items(raw)
        if not results:
            v["item_id"].set("")
            v["id_display"].set("INF")
            v["_combo_widget"].configure(values=[])
            return
        displays = [r["display"] for r in results]
        v["_combo_widget"].configure(values=displays)
        first = results[0]
        v["combo_text"].set(first["display"])
        v["item_id"].set(str(first["id"]))
        v["id_display"].set(str(first["id"]))

    def _on_item_combo_select(self, v: dict):
        """User picked one of the item search results."""
        sid = self._parse_id_from_display(v["combo_text"].get())
        if sid is not None:
            v["item_id"].set(str(sid))
            v["id_display"].set(str(sid))
        else:
            v["item_id"].set("")
            v["id_display"].set("INF")

    # ── Condition panel ──────────────────────────────────────────────────────

    def _build_condition_panel(self, parent):
        edit = ttk.Frame(parent)
        edit.pack(fill=tk.X, pady=(0, 4))

        self.v_subj  = tk.StringVar(value=SUBJECT_KEYS[0])
        self.v_stat  = tk.StringVar(value=STAT_KEYS[0])
        self.v_op    = tk.StringVar(value=COND_OPS[4])    # ">=" default
        self.v_nval  = tk.StringVar(value="0")
        self.v_sval  = tk.StringVar()
        self.v_cond_spell = tk.StringVar()
        self.v_cond_bool = tk.StringVar(value="False")

        ttk.Label(edit, text="Subject:").pack(side=tk.LEFT)
        ttk.Combobox(edit, textvariable=self.v_subj, values=SUBJECT_KEYS,
                     state="readonly", width=12).pack(side=tk.LEFT, padx=2)
        ttk.Label(edit, text="Stat:").pack(side=tk.LEFT)
        stat_cb = ttk.Combobox(edit, textvariable=self.v_stat, values=STAT_KEYS,
                               state="readonly", width=14)
        stat_cb.pack(side=tk.LEFT, padx=2)
        stat_cb.bind("<<ComboboxSelected>>", self._on_cond_stat_changed)
        ttk.Label(edit, text="Op:").pack(side=tk.LEFT)
        ttk.Combobox(edit, textvariable=self.v_op, values=COND_OPS_OPTS,
                     state="readonly", width=6).pack(side=tk.LEFT, padx=2)
        self._cond_value_label = ttk.Label(edit, text="Value:")
        self._cond_value_label.pack(side=tk.LEFT)
        self._cond_nval_entry = ttk.Entry(edit, textvariable=self.v_nval, width=6)
        self._cond_nval_entry.pack(side=tk.LEFT, padx=2)
        self._cond_string_label = ttk.Label(edit, text="String:")
        self._cond_string_label.pack(side=tk.LEFT)
        self._cond_sval_entry = ttk.Entry(edit, textvariable=self.v_sval, width=10)
        self._cond_sval_entry.pack(side=tk.LEFT, padx=2)
        self._cond_aura_label = ttk.Label(edit, text="Aura:")
        self._cond_spell_combo = ttk.Combobox(
            edit, textvariable=self.v_cond_spell,
            values=[s["display"] for s in self._class_spells], width=28)
        self._cond_spell_combo.bind("<<ComboboxSelected>>", self._on_cond_spell_pick)
        self._cond_spell_combo.bind("<Return>", self._on_cond_spell_typed)
        self._cond_spell_combo.bind("<FocusOut>", self._on_cond_spell_typed)
        self._cond_bool_label = ttk.Label(edit, text="Value:")
        self._cond_bool_combo = ttk.Combobox(
            edit, textvariable=self.v_cond_bool, values=["True", "False"],
            state="readonly", width=7)
        self._cond_bool_combo.bind("<<ComboboxSelected>>", self._on_cond_bool_changed)
        self._sync_condition_value_editor()

        list_f = ttk.Frame(parent)
        list_f.pack(fill=tk.BOTH, expand=True, pady=(0, 4))

        cols = ("seq", "desc")
        self._cond_tv = ttk.Treeview(list_f, columns=cols, show="headings",
                                     height=8, selectmode="browse")
        self._cond_tv.heading("seq",  text="#")
        self._cond_tv.heading("desc", text="Condition")
        self._cond_tv.column("seq",  width=25, anchor="center")
        self._cond_tv.column("desc", width=300)
        self._cond_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self._cond_tv.bind("<<TreeviewSelect>>", self._on_cond_select)

        sb = ttk.Scrollbar(list_f, orient="vertical", command=self._cond_tv.yview)
        self._cond_tv.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.LEFT, fill=tk.Y)

        btn_row = ttk.Frame(parent)
        btn_row.pack(fill=tk.X)
        ttk.Button(btn_row, text="+ Save cond",
                   command=self._save_condition).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(btn_row, text="✕ Remove",
                   command=self._del_condition).pack(side=tk.LEFT)

    # ── Public API ───────────────────────────────────────────────────────────

    def load_profile(self, profile_id: int):
        self._profile_id = profile_id
        self._sel_entry  = None
        self._entries    = self._cbs["load_entries"](profile_id)
        self._refresh_entry_list()
        self._set_detail_enabled(False)

    def clear(self):
        self._profile_id = None
        self._sel_entry  = None
        self._entries    = []
        self._refresh_entry_list()
        self._set_detail_enabled(False)

    # ── Internal ─────────────────────────────────────────────────────────────

    def _refresh_entry_list(self):
        self._tv.delete(*self._tv.get_children())
        for e in self._entries:
            flags = []
            if e.get("is_interrupt"):  flags.append("INT")
            if not e.get("enabled"):   flags.append("off")
            iid = str(e.get("entry_id", id(e)))
            self._tv.insert("", "end", iid=iid,
                            values=(e.get("priority", 0),
                                    e.get("label", ""),
                                    " ".join(flags)))

    def _on_entry_select(self, _=None):
        sel = self._tv.selection()
        if not sel:
            return
        iid = sel[0]
        self._sel_entry = next(
            (e for e in self._entries if str(e.get("entry_id", id(e))) == iid), None)
        if self._sel_entry:
            self._load_entry_detail(self._sel_entry)
            self._set_detail_enabled(True)

    def _load_entry_detail(self, e: dict):
        self.v_label.set(e.get("label", ""))
        self.v_priority.set(str(e.get("priority", 0)))
        self.v_enabled.set(bool(e.get("enabled", 1)))
        self.v_interrupt.set(bool(e.get("is_interrupt", 0)))
        self.v_break.set(bool(e.get("breaks_current_cast", 0)))
        self.v_cond_logic.set(COND_LOGIC.get(e.get("condition_logic", 0), "All (AND)"))

        eid = e.get("entry_id")
        actions = {a["slot"]: a for a in (self._cbs["load_actions"](eid) if eid else [])}
        for slot, w in self._action_widgets.items():
            a = actions.get(slot, {})
            action_type = a.get("action_type", 0)
            w["type"].set(ACTION_TYPES.get(action_type, "Spell"))
            w["rank_mode"].set(RANK_MODES.get(a.get("rank_mode", 0), "Best Known"))
            w["rank_val"].set(str(a.get("rank_value", 0) or 0))
            w["target"].set(a.get("target_key", "enemy") or "enemy")
            w["aoe_mode"].set(AOE_MODES.get(a.get("aoe_mode"), "") if a.get("aoe_mode") is not None else "")
            w["aoe_min"].set("" if a.get("aoe_min_targets") in (None, "") else str(a.get("aoe_min_targets")))
            w["aoe_radius"].set("" if a.get("aoe_radius") in (None, "") else str(a.get("aoe_radius")))

            if action_type == 0:  # Spell
                sid = a.get("spell_base_id") or 0
                w["spell_id"].set(str(sid) if sid else "")
                w["item_id"].set("")
                display = self._spell_display_for_id(sid) if sid else ""
                w["combo_text"].set(display)
                w["id_display"].set(str(sid) if sid else "")
                w["_combo_widget"].configure(
                    values=[s["display"] for s in self._class_spells])
            else:  # Item
                iid = a.get("item_id") or 0
                w["spell_id"].set("")
                w["item_id"].set(str(iid) if iid else "")
                if iid:
                    name = db.item_name(int(iid))
                    display = f"{name}  [{iid}]" if name else f"#{iid}"
                    w["combo_text"].set(display)
                    w["id_display"].set(str(iid))
                    w["_combo_widget"].configure(values=[display])
                else:
                    w["combo_text"].set("")
                    w["id_display"].set("")
                    w["_combo_widget"].configure(values=[])

        self._conditions = self._cbs["load_conditions"](eid) if eid else []
        self._refresh_cond_list()

    def _refresh_cond_list(self):
        self._cond_tv.delete(*self._cond_tv.get_children())
        for c in self._conditions:
            self._cond_tv.insert("", "end",
                                 iid=str(c.get("condition_id", id(c))),
                                 values=(c.get("sequence", 0), self._condition_desc(c)))

    def _on_cond_select(self, _=None):
        sel = self._cond_tv.selection()
        if not sel:
            return
        iid = sel[0]
        c = next((x for x in self._conditions
                  if str(x.get("condition_id", id(x))) == iid), None)
        if c:
            self.v_subj.set(c.get("subject_key", SUBJECT_KEYS[0]))
            self.v_stat.set(c.get("stat_key", STAT_KEYS[0]))
            self.v_op.set(COND_OPS.get(c.get("comparison", 4), ">="))
            self.v_nval.set(str(c.get("numeric_value", 0) or 0))
            self.v_sval.set(c.get("string_value", "") or "")
            if c.get("stat_key") == "aura":
                raw_value = c.get("string_value", "") or c.get("numeric_value", "")
                self.v_cond_spell.set(self._condition_spell_display(raw_value))
                self.v_cond_bool.set("False")
            elif c.get("stat_key") in BOOL_STAT_KEYS:
                try:
                    self.v_cond_bool.set(
                        "True" if int(float(c.get("numeric_value", 0) or 0)) else "False")
                except (TypeError, ValueError):
                    self.v_cond_bool.set("False")
                self.v_cond_spell.set("")
            else:
                self.v_cond_spell.set("")
                self.v_cond_bool.set("False")
            self._sync_condition_value_editor()

    def _set_detail_enabled(self, on: bool):
        state = "normal" if on else "disabled"

        def _apply(widget):
            try:
                widget.configure(state=state)
            except Exception:
                pass
            for child in widget.winfo_children():
                _apply(child)

        for w in self.winfo_children():
            _apply(w)

    # ── Entry CRUD ───────────────────────────────────────────────────────────

    def _add_entry(self):
        if not self._profile_id:
            return
        e = dict(priority=len(self._entries), label="New entry",
                 is_interrupt=0, breaks_current_cast=0, enabled=1, condition_logic=0)
        eid = self._cbs["upsert_entry"](e, self._profile_id)
        e["entry_id"] = eid
        self._entries.append(e)
        self._refresh_entry_list()
        # Select the new row
        self._tv.selection_set(str(eid))
        self._on_entry_select()

    def _del_entry(self):
        if not self._sel_entry:
            return
        if not messagebox.askyesno("Confirm", "Delete this entry and all its actions/conditions?"):
            return
        eid = self._sel_entry.get("entry_id")
        if eid:
            self._cbs["delete_entry"](eid)
        self._entries.remove(self._sel_entry)
        self._sel_entry = None
        self._refresh_entry_list()
        self._set_detail_enabled(False)

    def _move_up(self):
        self._swap_priority(-1)

    def _move_down(self):
        self._swap_priority(1)

    def _swap_priority(self, direction: int):
        if not self._sel_entry:
            return
        idx = self._entries.index(self._sel_entry)
        new_idx = idx + direction
        if new_idx < 0 or new_idx >= len(self._entries):
            return
        # Swap priorities
        a, b = self._entries[idx], self._entries[new_idx]
        a["priority"], b["priority"] = b["priority"], a["priority"]
        self._cbs["upsert_entry"](a, self._profile_id)
        self._cbs["upsert_entry"](b, self._profile_id)
        self._entries[idx], self._entries[new_idx] = b, a
        self._refresh_entry_list()
        self._tv.selection_set(str(self._sel_entry.get("entry_id", id(self._sel_entry))))

    def _save_entry(self):
        if not self._sel_entry or not self._profile_id:
            return
        self._sel_entry["label"]              = self.v_label.get().strip()
        self._sel_entry["priority"]           = int(self.v_priority.get() or 0)
        self._sel_entry["enabled"]            = int(self.v_enabled.get())
        self._sel_entry["is_interrupt"]       = int(self.v_interrupt.get())
        self._sel_entry["breaks_current_cast"]= int(self.v_break.get())
        self._sel_entry["condition_logic"]    = COND_LOGIC_INV.get(self.v_cond_logic.get(), 0)

        eid = self._cbs["upsert_entry"](self._sel_entry, self._profile_id)
        if not self._sel_entry.get("entry_id"):
            self._sel_entry["entry_id"] = eid

        # Save actions
        existing = {a["slot"]: a for a in self._cbs["load_actions"](eid)}
        for slot, w in self._action_widgets.items():
            a = existing.get(slot, {})
            aoe_mode_text = w["aoe_mode"].get().strip()
            aoe_min_text = w["aoe_min"].get().strip()
            aoe_radius_text = w["aoe_radius"].get().strip()
            a.update(slot=slot,
                     action_type=ACTION_INV.get(w["type"].get(), 0),
                     spell_base_id=int(w["spell_id"].get() or 0),
                     item_id=int(w["item_id"].get() or 0),
                     rank_mode=RANK_INV.get(w["rank_mode"].get(), 0),
                     rank_value=int(w["rank_val"].get() or 0),
                     target_key=w["target"].get() or "enemy",
                     aoe_mode=AOE_INV.get(aoe_mode_text) if aoe_mode_text else None,
                     aoe_min_targets=int(aoe_min_text) if aoe_min_text else None,
                     aoe_radius=float(aoe_radius_text) if aoe_radius_text else None)
            self._cbs["upsert_action"](a, eid)

        self._refresh_entry_list()
        self._tv.selection_set(str(eid))

    # ── Condition CRUD ────────────────────────────────────────────────────────

    def _save_condition(self):
        if not self._sel_entry:
            return
        eid = self._sel_entry.get("entry_id")
        if not eid:
            messagebox.showwarning("Save entry first", "Save the entry before adding conditions.")
            return
        # Find if editing existing
        sel = self._cond_tv.selection()
        existing_id = None
        if sel:
            iid = sel[0]
            matched = next((c for c in self._conditions
                            if str(c.get("condition_id", id(c))) == iid), None)
            if matched:
                existing_id = matched.get("condition_id")

        seq = (max((c.get("sequence", 0) for c in self._conditions), default=-1) + 1
               if not existing_id else
               next(c.get("sequence", 0) for c in self._conditions
                    if c.get("condition_id") == existing_id))

        numeric_value = float(self.v_nval.get() or 0)
        string_value = self.v_sval.get()
        if self.v_stat.get() == "aura":
            self._on_cond_spell_typed()
            numeric_value = float(self.v_nval.get() or 0)
            string_value = ""
        elif self.v_stat.get() in BOOL_STAT_KEYS:
            self._on_cond_bool_changed()
            numeric_value = float(self.v_nval.get() or 0)
            string_value = ""

        c = dict(condition_id=existing_id, sequence=seq,
                 subject_key=self.v_subj.get(), stat_key=self.v_stat.get(),
                 comparison=COND_OPS_INV.get(self.v_op.get(), 4),
                 numeric_value=numeric_value,
                 string_value=string_value)
        self._cbs["upsert_condition"](c, eid)
        self._conditions = self._cbs["load_conditions"](eid)
        self._refresh_cond_list()

    def _del_condition(self):
        sel = self._cond_tv.selection()
        if not sel or not self._sel_entry:
            return
        iid = sel[0]
        c = next((x for x in self._conditions
                  if str(x.get("condition_id", id(x))) == iid), None)
        if c and c.get("condition_id"):
            self._cbs["delete_condition"](c["condition_id"])
            self._conditions.remove(c)
            self._refresh_cond_list()


def _class_from_spec(*texts: str) -> int | None:
    """Guess class_id from profile text such as spec_key or display_name."""
    blob = " ".join(t for t in texts if t).strip()
    if not blob:
        return None
    sk = blob.lower()
    for name, cid in SPEC_TO_CLASS.items():
        if name in sk:
            return cid
    normalized = sk.replace("_", " ").replace("/", " ")
    for alias, cid in SPEC_ALIAS_TO_CLASS.items():
        if alias in normalized:
            return cid
    return None


