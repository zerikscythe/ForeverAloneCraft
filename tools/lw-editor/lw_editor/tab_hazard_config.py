"""
tab_hazard_config.py -- Hazard Sensor Configuration tab.

Three inner sub-tabs:
  Hazard Auras    -- living_world_hazard_auras
  Class Rules     -- living_world_hazard_class_rules
  Tuning          -- living_world_hazard_config (key/value)
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk, messagebox

from .db import db

ROLE_KEYS = [
    ("TANK",          "Tank",         "Warrior / Death Knight holding aggro on their target"),
    ("HEALER",        "Healer",       "Pure healer (Priest)"),
    ("HYBRID_HEALER", "Hybrid Healer","Paladin / Shaman / Druid — may be healing or DPS spec"),
    ("MELEE_DPS",     "Melee DPS",    "Rogue, Feral Druid, Enhancement Shaman, Ret Paladin, etc."),
    ("RANGED_DPS",    "Ranged DPS",   "Mage, Warlock, Hunter, Shadow Priest, Balance Druid, etc."),
]
ROLE_KEY_TO_LABEL = {k: lbl for k, lbl, _ in ROLE_KEYS}
ROLE_LABEL_TO_KEY = {lbl: k for k, lbl, _ in ROLE_KEYS}

TUNING_KEYS = [
    ("damage_threshold_pct",     "Damage Threshold %",
     "HP% drop per tick to trigger layer-2 detection."),
    ("consecutive_damage_ticks", "Consecutive Ticks",
     "Ticks of damage in a row needed before declaring layer-2 danger."),
    ("max_movement_yards",       "Max Movement Yards",
     "If bot moved more than this between ticks, HP loss is ignored."),
    ("anchor_search_radius",     "Anchor Search Radius (yards)",
     "How far to search for a clean party member to escape toward."),
    ("escape_step_yards",        "Escape Step (yards)",
     "Distance the bot steps toward the anchor per escape tick."),
    ("commit_window_ms",         "Commit Window (ms)",
     "Once an anchor is chosen, keep it this long to avoid jitter."),
]


def _safe_float(val: str, default: float = 0.0) -> float:
    try:
        return float(val)
    except (ValueError, TypeError):
        return default


class HazardConfigTab(ttk.Frame):
    def __init__(self, parent: tk.Widget):
        super().__init__(parent)
        inner = ttk.Notebook(self)
        inner.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self._behavior_panel = _BotBehaviorPanel(inner)
        self._aura_panel  = _AuraPanel(inner)
        self._role_panel  = _RoleRulePanel(inner)
        self._tune_panel  = _TuningPanel(inner)
        inner.add(self._behavior_panel, text="  Bot Behaviour  ")
        inner.add(self._aura_panel,  text="  Hazard Auras  ")
        inner.add(self._role_panel,  text="  Role Rules  ")
        inner.add(self._tune_panel,  text="  Hazard Tuning  ")

    def refresh(self):
        if not db.ok():
            return
        self._behavior_panel.refresh()
        self._aura_panel.refresh()
        self._role_panel.refresh()
        self._tune_panel.refresh()


FORMATION_OPTIONS = ["Ring", "V-Shape", "Line", "Cluster"]

BEHAVIOR_KEYS = [
    # (config_key, label, widget_type, description)
    ("follow_distance_melee",  "Melee / Tank",        "float",
     "Yards: Warriors, Death Knights, Rogues — close in, ready to engage."),
    ("follow_distance_healer", "Healer",              "float",
     "Yards: Healer and Hybrid Healer — out of the melee pile but in cast range."),
    ("follow_distance_ranged", "Ranged / Caster",     "float",
     "Yards: Mages, Warlocks, Hunters — further back, pre-positioned for spells."),
    ("follow_distance",        "Fallback",            "float",
     "Used when role cannot be determined (e.g. Passive mode)."),
    ("follow_formation",       "Formation Style",     "formation",
     "Ring=evenly spread around owner, V=fan behind, Line=single file, Cluster=bunched."),
    ("follow_slot_count",      "Ring Slot Count",     "int",
     "Positions in Ring formation (3–9). Ignored for other styles."),
    ("mount_with_owner",       "Mount With Owner",    "bool",
     "Bots mount up when the owner mounts. (Implementation pending.)"),
    ("auto_loot",              "Auto-Loot",           "bool",
     "Bots loot nearby corpses automatically. (Implementation pending.)"),
]


class _BotBehaviorPanel(ttk.Frame):

    def __init__(self, parent):
        super().__init__(parent)
        self._vars: dict[str, tk.Variable] = {}
        self._build()

    def _build(self):
        ttk.Label(self, text=(
            "Global behaviour settings that apply to every bot. "
            "Changes are picked up within 60 seconds without a server restart."
        ), wraplength=820, justify=tk.LEFT).pack(anchor="w", padx=8, pady=(6, 4))

        # ── Follow Distance section ──────────────────────────────────────────
        fd = ttk.LabelFrame(self, text="Follow Distance (yards)")
        fd.pack(fill=tk.X, padx=8, pady=4)

        ttk.Label(fd, text=(
            "Bots follow at a role-appropriate distance so melee bots crowd in, "
            "healers stay at cast range, and ranged bots hang back."
        ), wraplength=760, justify=tk.LEFT, foreground="#555").grid(
            row=0, column=0, columnspan=6, sticky="w", padx=8, pady=(4, 6))

        # Row: Role labels + entries on one line
        role_fields = [
            ("follow_distance_melee",  "Melee / Tank"),
            ("follow_distance_healer", "Healer"),
            ("follow_distance_ranged", "Ranged / Caster"),
            ("follow_distance",        "Fallback"),
        ]
        for col, (key, label) in enumerate(role_fields):
            ttk.Label(fd, text=label + ":").grid(
                row=1, column=col * 2, sticky="e", padx=(12 if col == 0 else 8, 2), pady=6)
            var = tk.StringVar()
            ttk.Entry(fd, textvariable=var, width=6).grid(
                row=1, column=col * 2 + 1, sticky="w", pady=6, padx=(0, 4))
            self._vars[key] = var

        # default hints
        for key, default in [("follow_distance_melee","1.0"),
                              ("follow_distance_healer","1.5"),
                              ("follow_distance_ranged","2.5"),
                              ("follow_distance","2.0")]:
            self._vars[key].set(default)

        # ── Formation section ────────────────────────────────────────────────
        ff = ttk.LabelFrame(self, text="Follow Formation")
        ff.pack(fill=tk.X, padx=8, pady=4)

        ttk.Label(ff, text="Style:").grid(row=0, column=0, sticky="e", padx=(12, 2), pady=8)
        fv = tk.StringVar()
        ttk.Combobox(ff, textvariable=fv, values=FORMATION_OPTIONS,
                     state="readonly", width=14).grid(row=0, column=1, sticky="w", padx=(0, 16))
        self._vars["follow_formation"] = fv
        fv.set(FORMATION_OPTIONS[0])

        ttk.Label(ff, text="Ring Slot Count:").grid(row=0, column=2, sticky="e", padx=(0, 2))
        sv = tk.StringVar(value="7")
        ttk.Entry(ff, textvariable=sv, width=5).grid(row=0, column=3, sticky="w", padx=(0, 8))
        self._vars["follow_slot_count"] = sv

        ttk.Label(ff, text="(3–9 slots, Ring formation only)",
                  foreground="#555").grid(row=0, column=4, sticky="w", padx=8)

        # ── Misc toggles ─────────────────────────────────────────────────────
        misc = ttk.LabelFrame(self, text="Misc")
        misc.pack(fill=tk.X, padx=8, pady=4)

        mv = tk.BooleanVar(value=True)
        ttk.Checkbutton(misc, text="Mount With Owner",
                        variable=mv).grid(row=0, column=0, padx=12, pady=8, sticky="w")
        ttk.Label(misc, text="(pending)", foreground="#999").grid(row=0, column=1, sticky="w")
        self._vars["mount_with_owner"] = mv

        av = tk.BooleanVar(value=False)
        ttk.Checkbutton(misc, text="Auto-Loot",
                        variable=av).grid(row=0, column=2, padx=24, pady=8, sticky="w")
        ttk.Label(misc, text="(pending)", foreground="#999").grid(row=0, column=3, sticky="w")
        self._vars["auto_loot"] = av

        ttk.Button(self, text="Save All Changes",
                   command=self._save_all).pack(anchor="e", padx=8, pady=6)

    def refresh(self):
        rows = db.q(db.world,
            "SELECT config_key, config_value "
            "FROM living_world_bot_global_config") or []
        current = {r.get("config_key"): r.get("config_value") for r in rows}

        for key, _, wtype, *__ in BEHAVIOR_KEYS:
            val = current.get(key)
            if val is None:
                continue
            var = self._vars.get(key)
            if var is None:
                continue
            if wtype == "bool":
                var.set(float(val) >= 0.5)
            elif wtype == "formation":
                idx = int(float(val))
                var.set(FORMATION_OPTIONS[idx] if 0 <= idx < len(FORMATION_OPTIONS)
                        else FORMATION_OPTIONS[0])
            else:
                var.set(str(val))

    def _save_all(self):
        errors = []
        for key, label, wtype, _ in BEHAVIOR_KEYS:
            var = self._vars.get(key)
            if var is None:
                continue
            if wtype == "bool":
                value = 1.0 if var.get() else 0.0
            elif wtype == "formation":
                text = var.get()
                try:
                    value = float(FORMATION_OPTIONS.index(text))
                except ValueError:
                    value = 0.0
            else:
                try:
                    value = float(var.get())
                    if wtype == "int":
                        value = float(int(value))
                except ValueError:
                    errors.append(f"  {label}: '{var.get()}' is not a valid number")
                    continue
            db.run(db.world,
                "INSERT INTO living_world_bot_global_config (config_key, config_value) "
                "VALUES (%s, %s) ON DUPLICATE KEY UPDATE config_value = %s",
                (key, value, value))
        if errors:
            messagebox.showerror("Validation", "\n".join(errors))
        else:
            messagebox.showinfo("Saved",
                "Global behaviour settings saved.\n"
                "Server will apply them within 60 seconds.")


class _AuraPanel(ttk.Frame):
    _COLS = ("spell_display", "severity", "enabled", "notes")
    _MAX_RESULTS = 80

    def __init__(self, parent):
        super().__init__(parent)
        self._resolved_spell_id: int | None = None
        self._build()

    def _build(self):
        ttk.Label(self, text=(
            "Ground-effect hazard auras the sensor watches. Search by spell name — "
            "the ID resolves automatically. Add new encounter-specific auras here "
            "without recompiling."
        ), wraplength=820, justify=tk.LEFT).pack(anchor="w", padx=8, pady=(6, 0))

        tf = ttk.Frame(self)
        tf.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)
        self._tv = ttk.Treeview(tf, columns=self._COLS, show="headings",
                                 selectmode="browse", height=12)
        for col, hdr, w, anc in [
            ("spell_display", "Spell Name  [ID]", 280, "w"),
            ("severity",      "Severity",          80, "center"),
            ("enabled",       "Enabled",            70, "center"),
            ("notes",         "Notes",             410, "w")]:
            self._tv.heading(col, text=hdr)
            self._tv.column(col, width=w, anchor=anc)
        sb = ttk.Scrollbar(tf, orient=tk.VERTICAL, command=self._tv.yview)
        self._tv.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._tv.pack(fill=tk.BOTH, expand=True)
        self._tv.bind("<<TreeviewSelect>>", self._on_select)

        form = ttk.LabelFrame(self, text="Edit / Add Aura")
        form.pack(fill=tk.X, padx=8, pady=(0, 6))
        self.v_severity = tk.StringVar(value="1.0")
        self.v_enabled  = tk.BooleanVar(value=True)
        self.v_notes    = tk.StringVar()
        self.v_search   = tk.StringVar()

        r = ttk.Frame(form)
        r.pack(fill=tk.X, padx=6, pady=4)

        ttk.Label(r, text="Spell:").grid(row=0, column=0, sticky="e")
        self._spell_combo = ttk.Combobox(r, textvariable=self.v_search, values=[], width=38)
        self._spell_combo.grid(row=0, column=1, padx=4)
        self._spell_combo.bind("<KeyRelease>", self._on_search_type)
        self._spell_combo.bind("<<ComboboxSelected>>", self._on_spell_selected)

        ttk.Label(r, text="ID:").grid(row=0, column=2, sticky="e", padx=(8, 2))
        self._id_badge = ttk.Label(r, text="—", width=8, relief="sunken",
                                    anchor="center", foreground="#0055cc",
                                    font=("TkDefaultFont", 9, "bold"))
        self._id_badge.grid(row=0, column=3, padx=(0, 12))

        ttk.Label(r, text="Severity:").grid(row=0, column=4, sticky="e", padx=(8, 2))
        ttk.Entry(r, textvariable=self.v_severity, width=8).grid(row=0, column=5, padx=4)
        ttk.Checkbutton(r, text="Enabled", variable=self.v_enabled).grid(row=0, column=6, padx=8)
        ttk.Label(r, text="Notes:").grid(row=0, column=7, sticky="e", padx=(8, 2))
        ttk.Entry(r, textvariable=self.v_notes, width=34).grid(
            row=0, column=8, padx=4, sticky="ew")
        r.columnconfigure(8, weight=1)

        b = ttk.Frame(form)
        b.pack(anchor="e", padx=6, pady=(0, 4))
        ttk.Button(b, text="New",    command=self._clear).pack(side=tk.LEFT, padx=2)
        ttk.Button(b, text="Save",   command=self._save).pack(side=tk.LEFT, padx=2)
        ttk.Button(b, text="Delete", command=self._delete).pack(side=tk.LEFT, padx=2)

    def _spell_cache(self) -> list[tuple[int, str]]:
        return [(sid, name) for sid, name in db.spell_name_cache().items() if name]

    def _on_search_type(self, _=None):
        query = self.v_search.get().strip().lower()
        if not query:
            self._spell_combo.configure(values=[])
            self._resolved_spell_id = None
            self._id_badge.config(text="—")
            return
        matches = [f"{name}  [{sid}]"
                   for sid, name in self._spell_cache()
                   if query in name.lower()][:self._MAX_RESULTS]
        self._spell_combo.configure(values=matches)
        if matches:
            # Values are set but Tkinter won't open the dropdown automatically.
            # Schedule it for the next event loop tick so the list is ready.
            self._spell_combo.after(10, self._open_dropdown)
        if len(matches) == 1:
            self._resolve_from_display(matches[0])

    def _open_dropdown(self):
        try:
            self._spell_combo.event_generate('<Down>')
        except Exception:
            pass

    def _on_spell_selected(self, _=None):
        self._resolve_from_display(self.v_search.get())

    def _resolve_from_display(self, text: str):
        text = text.strip()
        if text.endswith("]") and "[" in text:
            try:
                sid = int(text.rsplit("[", 1)[-1].rstrip("]").strip())
                self._resolved_spell_id = sid
                self._id_badge.config(text=str(sid))
                return
            except ValueError:
                pass
        self._resolved_spell_id = None
        self._id_badge.config(text="—")

    def refresh(self):
        self._tv.delete(*self._tv.get_children())
        for r in (db.q(db.world,
                "SELECT spell_id, severity, enabled, COALESCE(notes,'') AS notes "
                "FROM living_world_hazard_auras ORDER BY spell_id") or []):
            sid  = r.get("spell_id", 0)
            name = db.spell_name(sid)
            disp = f"{name}  [{sid}]" if name else f"#{sid}"
            self._tv.insert("", tk.END, values=(
                disp,
                f"{float(r.get('severity', 1.0)):.2f}",
                "Yes" if r.get("enabled") else "No",
                r.get("notes", ""),
            ))

    def _on_select(self, _=None):
        sel = self._tv.selection()
        if not sel:
            return
        v = self._tv.item(sel[0], "values")
        disp = v[0]
        self.v_search.set(disp)
        self._resolve_from_display(disp)
        self.v_severity.set(v[1])
        self.v_enabled.set(v[2] == "Yes")
        self.v_notes.set(v[3])

    def _save(self):
        if not self._resolved_spell_id:
            messagebox.showerror("Error",
                "Search for a spell by name and select it from the dropdown first.")
            return
        sid  = self._resolved_spell_id
        sev  = _safe_float(self.v_severity.get(), 1.0)
        enb  = 1 if self.v_enabled.get() else 0
        note = self.v_notes.get().strip() or None
        db.run(db.world,
            "INSERT INTO living_world_hazard_auras (spell_id,severity,enabled,notes) "
            "VALUES (%s,%s,%s,%s) ON DUPLICATE KEY UPDATE severity=%s,enabled=%s,notes=%s",
            (sid, sev, enb, note, sev, enb, note))
        self.refresh()

    def _delete(self):
        if not self._resolved_spell_id:
            return
        sid = self._resolved_spell_id
        name = db.spell_name(sid) or str(sid)
        if not messagebox.askyesno("Confirm", f"Remove '{name}' [{sid}] from hazard auras?"):
            return
        db.run(db.world, "DELETE FROM living_world_hazard_auras WHERE spell_id=%s", (sid,))
        self._clear()
        self.refresh()

    def _clear(self):
        self.v_search.set("")
        self.v_severity.set("1.0")
        self.v_enabled.set(True)
        self.v_notes.set("")
        self._resolved_spell_id = None
        self._id_badge.config(text="—")
        self._spell_combo.configure(values=[])
        self._tv.selection_remove(*self._tv.selection())


class _RoleRulePanel(ttk.Frame):
    _COLS = ("role", "skip_escape", "owner_hp_gate", "req_aggro", "notes")

    def __init__(self, parent):
        super().__init__(parent)
        self._build()

    def _build(self):
        ttk.Label(self, text=(
            "Controls whether each role participates in hazard escape. "
            "Tanks skip escape only while actively holding aggro on their target. "
            "Healers suppress escape when the owner's HP is critically low. "
            "Rows without any special handling use the global default (always escape)."
        ), wraplength=820, justify=tk.LEFT).pack(anchor="w", padx=8, pady=(6, 0))

        tf = ttk.Frame(self)
        tf.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)
        self._tv = ttk.Treeview(tf, columns=self._COLS, show="headings",
                                  selectmode="browse", height=8)
        for col, hdr, w, anc in [
            ("role",         "Role",                   160, "w"),
            ("skip_escape",  "Skip Escape",             90, "center"),
            ("owner_hp_gate","Owner HP Gate %",         130, "center"),
            ("req_aggro",    "Requires Aggro to Skip",  175, "center"),
            ("notes",        "Notes",                   240, "w")]:
            self._tv.heading(col, text=hdr)
            self._tv.column(col, width=w, anchor=anc)
        sb = ttk.Scrollbar(tf, orient=tk.VERTICAL, command=self._tv.yview)
        self._tv.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._tv.pack(fill=tk.BOTH, expand=True)
        self._tv.bind("<<TreeviewSelect>>", self._on_select)

        form = ttk.LabelFrame(self, text="Edit Role Rule")
        form.pack(fill=tk.X, padx=8, pady=(0, 6))
        self.v_role     = tk.StringVar()
        self.v_skip     = tk.BooleanVar()
        self.v_hp_gate  = tk.StringVar(value="0.0")
        self.v_req_aggro= tk.BooleanVar()
        self.v_notes    = tk.StringVar()

        r = ttk.Frame(form)
        r.pack(fill=tk.X, padx=6, pady=4)
        ttk.Label(r, text="Role:").grid(row=0, column=0, sticky="e")
        ttk.Combobox(r, textvariable=self.v_role,
                     values=[lbl for _, lbl, _ in ROLE_KEYS],
                     width=18, state="readonly").grid(row=0, column=1, padx=4)
        ttk.Checkbutton(r, text="Skip Escape",
                        variable=self.v_skip).grid(row=0, column=2, padx=8)
        ttk.Checkbutton(r, text="Requires Aggro to Skip",
                        variable=self.v_req_aggro).grid(row=0, column=3, padx=8)
        ttk.Label(r, text="Owner HP Gate %:").grid(row=0, column=4, sticky="e", padx=(8,2))
        ttk.Entry(r, textvariable=self.v_hp_gate, width=7).grid(row=0, column=5, padx=4)
        ttk.Label(r, text="Notes:").grid(row=0, column=6, sticky="e", padx=(8,2))
        ttk.Entry(r, textvariable=self.v_notes, width=34).grid(
            row=0, column=7, padx=4, sticky="ew")
        r.columnconfigure(7, weight=1)

        b = ttk.Frame(form)
        b.pack(anchor="e", padx=6, pady=(0, 4))
        ttk.Button(b, text="Save",  command=self._save).pack(side=tk.LEFT, padx=2)

    def refresh(self):
        self._tv.delete(*self._tv.get_children())
        for r in (db.q(db.world,
                "SELECT role_key, skip_escape, owner_hp_gate_pct, "
                "requires_aggro_to_skip, COALESCE(notes,'') AS notes "
                "FROM living_world_hazard_role_rules ORDER BY role_key") or []):
            rk = r.get("role_key", "")
            self._tv.insert("", tk.END, values=(
                ROLE_KEY_TO_LABEL.get(rk, rk),
                "Yes" if r.get("skip_escape") else "No",
                f"{float(r.get('owner_hp_gate_pct', 0.0)):.1f}",
                "Yes" if r.get("requires_aggro_to_skip") else "No",
                r.get("notes", ""),
            ))

    def _on_select(self, _=None):
        sel = self._tv.selection()
        if not sel:
            return
        v = self._tv.item(sel[0], "values")
        self.v_role.set(v[0])
        self.v_skip.set(v[1] == "Yes")
        self.v_hp_gate.set(v[2])
        self.v_req_aggro.set(v[3] == "Yes")
        self.v_notes.set(v[4])

    def _save(self):
        role_key = ROLE_LABEL_TO_KEY.get(self.v_role.get())
        if not role_key:
            messagebox.showerror("Error", "Select a role."); return
        skip   = 1 if self.v_skip.get() else 0
        hpg    = _safe_float(self.v_hp_gate.get(), 0.0)
        aggro  = 1 if self.v_req_aggro.get() else 0
        note   = self.v_notes.get().strip() or None
        db.run(db.world,
            "INSERT INTO living_world_hazard_role_rules "
            "  (role_key, skip_escape, owner_hp_gate_pct, requires_aggro_to_skip, notes) "
            "  VALUES (%s,%s,%s,%s,%s) "
            "ON DUPLICATE KEY UPDATE "
            "  skip_escape=%s, owner_hp_gate_pct=%s, requires_aggro_to_skip=%s, notes=%s",
            (role_key, skip, hpg, aggro, note,
             skip, hpg, aggro, note))
        self.refresh()


class _TuningPanel(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self._vars: dict[str, tk.StringVar] = {k: tk.StringVar() for k,_,_ in TUNING_KEYS}
        self._build()

    def _build(self):
        ttk.Label(self, text=(
            "Numeric constants governing hazard detection and escape behaviour. "
            "Saved values are picked up by the server within 60 seconds without a restart."
        ), wraplength=820, justify=tk.LEFT).pack(anchor="w", padx=8, pady=(6, 0))

        g = ttk.Frame(self)
        g.pack(fill=tk.X, padx=8, pady=8)
        for i, (key, label, desc) in enumerate(TUNING_KEYS):
            ttk.Label(g, text=label+":", anchor="e", width=28).grid(
                row=i, column=0, sticky="e", padx=(0,6), pady=4)
            ttk.Entry(g, textvariable=self._vars[key], width=12).grid(
                row=i, column=1, sticky="w", pady=4)
            ttk.Label(g, text=desc, foreground="#555",
                      wraplength=520, justify=tk.LEFT).grid(
                row=i, column=2, sticky="w", padx=10, pady=4)
        g.columnconfigure(2, weight=1)
        ttk.Button(self, text="Save All Changes",
                   command=self._save_all).pack(anchor="e", padx=8, pady=6)

    def refresh(self):
        cur = {r.get("config_key"): r.get("config_value")
               for r in (db.q(db.world,
                   "SELECT config_key, config_value FROM living_world_hazard_config") or [])}
        for key, var in self._vars.items():
            if key in cur and cur[key] is not None:
                var.set(str(cur[key]))

    def _save_all(self):
        errors = []
        for key, var in self._vars.items():
            raw = var.get().strip()
            try: value = float(raw)
            except ValueError: errors.append(f"  {key}: '{raw}' is not a number"); continue
            db.run(db.world,
                "INSERT INTO living_world_hazard_config (config_key,config_value) "
                "VALUES (%s,%s) ON DUPLICATE KEY UPDATE config_value=%s",
                (key, value, value))
        if errors:
            messagebox.showerror("Validation", "\n".join(errors))
        else:
            messagebox.showinfo("Saved",
                "Tuning values saved.\nServer will apply them within 60 seconds.")
