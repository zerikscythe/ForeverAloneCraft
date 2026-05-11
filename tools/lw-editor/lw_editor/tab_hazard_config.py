"""
tab_hazard_config.py -- World-rules and party-rules editor panels.

This file now exposes two top-level tabs used by the main editor:

- WorldRulesTab: true world/global rules like hazard auras and hazard tuning
- PartyRulesTab: direct companion / party behaviour rules backed by
  living_world_bot_global_config
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


class WorldRulesTab(ttk.Frame):
    def __init__(self, parent: tk.Widget):
        super().__init__(parent)
        inner = ttk.Notebook(self)
        inner.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self._aura_panel  = _AuraPanel(inner)
        self._role_panel  = _RoleRulePanel(inner)
        self._tune_panel  = _TuningPanel(inner)
        inner.add(self._aura_panel,  text="  Hazard Auras  ")
        inner.add(self._role_panel,  text="  Role Rules  ")
        inner.add(self._tune_panel,  text="  Hazard Tuning  ")

    def refresh(self):
        if not db.ok():
            return
        self._aura_panel.refresh()
        self._role_panel.refresh()
        self._tune_panel.refresh()


class PartyRulesTab(ttk.Frame):
    def __init__(self, parent: tk.Widget):
        super().__init__(parent)
        self._behavior_panel = _BotBehaviorPanel(self)
        self._behavior_panel.pack(fill=tk.BOTH, expand=True)

    def refresh(self):
        if not db.ok():
            return
        self._behavior_panel.refresh()


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
    ("combat_follow_override_distance", "Combat Follow Override", "float",
     "If ranged/healer bots drift farther than this from owner, they snap back into follow behaviour."),
    ("reposition_distance",    "Passive Reposition",   "float",
     "In Passive mode, reissue follow when farther than this from the owner."),
    ("ranged_min_distance",    "Ranged Min Distance",  "float",
     "Back away when a ranged/healer bot is closer than this to its combat target."),
    ("ranged_optimal_distance", "Ranged Optimal Distance", "float",
     "Preferred chase stop distance for ranged/healer combat positioning."),
    ("ranged_cast_range",      "Ranged Cast Range",    "float",
     "If target is farther than this, move closer before trying to cast."),
    ("ranged_retreat_distance", "Ranged Retreat Step",  "float",
     "Backstep distance when retreating from melee range."),
    ("ranged_retreat_trigger_pct", "Ranged Retreat Trigger %", "float",
     "Retreat when ranged bot HP drops below this percent in melee range."),
    ("ranged_retreat_reset_pct", "Ranged Retreat Reset %", "float",
     "After a retreat, do not retreat again until HP drops below this percent."),
    ("assist_use_current_victim", "Assist: Current Victim", "bool",
     "Normal assist mode: keep fighting the bot's current valid victim."),
    ("assist_use_owner_victim", "Assist: Owner Victim", "bool",
     "Normal assist mode: allow switching to the owner's current victim."),
    ("assist_owner_victim_must_target_owner", "Assist: Owner Victim Must Fight Back", "bool",
     "Require the owner's victim to be actively targeting the owner before assist picks it."),
    ("attack_lock_use_owner_victim", "Attack-Lock: Owner Victim", "bool",
     "During attack-lock, fall back to the owner's current victim when the bot's current victim is unavailable."),
    ("attack_lock_use_owner_selection", "Attack-Lock: Owner Selection", "bool",
     "During attack-lock, consider the owner's current selected target as a fallback source."),
    ("guard_use_current_victim", "Guard: Current Victim", "bool",
     "Guard mode: keep the bot's current valid victim before looking for new threats."),
    ("guard_use_owner_attackers", "Guard: Owner Attackers", "bool",
     "Guard mode: consider units actively attacking the owner."),
    ("assist_require_targetable_for_attack", "Assist/Guard: Require Attackable Target", "bool",
     "Normal assist and guard modes: require the candidate to currently pass attackable-for-attack checks."),
    ("command_require_targetable_for_attack", "Command/Attack-Lock: Require Attackable Target", "bool",
     "Forced-target and attack-lock assist: require the candidate to currently pass attackable-for-attack checks instead of tolerating pull/setup flicker."),
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
            "Direct party / companion behaviour rules. These govern how your own summoned bots "
            "follow, position, assist, and react in combat. Changes are picked up within 60 seconds "
            "without a server restart."
        ), wraplength=820, justify=tk.LEFT).pack(anchor="w", padx=8, pady=(6, 4))

        nb = ttk.Notebook(self)
        nb.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 4))

        self._build_follow_tab(nb)
        self._build_combat_movement_tab(nb)
        self._build_targeting_tab(nb)
        self._build_misc_tab(nb)

        ttk.Button(self, text="Save All Changes",
                   command=self._save_all).pack(anchor="e", padx=8, pady=6)

    def _build_follow_tab(self, nb):
        frame = ttk.Frame(nb)
        nb.add(frame, text="  Follow & Formation  ")

        fd = ttk.LabelFrame(frame, text="Follow Distance (yards)")
        fd.pack(fill=tk.X, padx=8, pady=8)

        ttk.Label(fd, text=(
            "Role-based spacing for your direct companion party. Melee stays tighter, healers hold cast range, "
            "and ranged bots hang farther back."
        ), wraplength=760, justify=tk.LEFT, foreground="#555").grid(
            row=0, column=0, columnspan=6, sticky="w", padx=8, pady=(4, 6))

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

        for key, default in [("follow_distance_melee", "1.0"),
                              ("follow_distance_healer", "1.5"),
                              ("follow_distance_ranged", "2.5"),
                              ("follow_distance", "2.0")]:
            self._vars[key].set(default)

        ff = ttk.LabelFrame(frame, text="Follow Formation")
        ff.pack(fill=tk.X, padx=8, pady=(0, 8))

        ttk.Label(ff, text=(
            "Formation controls apply to your direct active companion group, not ambient world population."
        ), wraplength=760, justify=tk.LEFT, foreground="#555").grid(
            row=0, column=0, columnspan=5, sticky="w", padx=8, pady=(4, 6))

        ttk.Label(ff, text="Style:").grid(row=1, column=0, sticky="e", padx=(12, 2), pady=8)
        fv = tk.StringVar()
        ttk.Combobox(ff, textvariable=fv, values=FORMATION_OPTIONS,
                     state="readonly", width=14).grid(row=1, column=1, sticky="w", padx=(0, 16))
        self._vars["follow_formation"] = fv
        fv.set(FORMATION_OPTIONS[0])

        ttk.Label(ff, text="Ring Slot Count:").grid(row=1, column=2, sticky="e", padx=(0, 2))
        sv = tk.StringVar(value="7")
        ttk.Entry(ff, textvariable=sv, width=5).grid(row=1, column=3, sticky="w", padx=(0, 8))
        self._vars["follow_slot_count"] = sv

        ttk.Label(ff, text="(3–9 slots, Ring formation only)",
                  foreground="#555").grid(row=1, column=4, sticky="w", padx=8)

    def _build_combat_movement_tab(self, nb):
        frame = ttk.Frame(nb)
        nb.add(frame, text="  Combat Movement  ")

        combat = ttk.LabelFrame(frame, text="Combat Positioning")
        combat.pack(fill=tk.X, padx=8, pady=8)

        ttk.Label(combat, text=(
            "These thresholds drive ranged/healer movement decisions inside your party: when companions snap back, "
            "close distance, or step out of melee pressure."
        ), wraplength=760, justify=tk.LEFT, foreground="#555").grid(
            row=0, column=0, columnspan=8, sticky="w", padx=8, pady=(4, 6))

        combat_fields = [
            ("combat_follow_override_distance", "Combat Follow"),
            ("reposition_distance", "Passive Reposition"),
            ("ranged_min_distance", "Min Distance"),
            ("ranged_optimal_distance", "Optimal Distance"),
            ("ranged_cast_range", "Cast Range"),
            ("ranged_retreat_distance", "Retreat Step"),
            ("ranged_retreat_trigger_pct", "Retreat %"),
            ("ranged_retreat_reset_pct", "Reset %"),
        ]
        for idx, (key, label) in enumerate(combat_fields):
            row = 1 + idx // 4
            col = (idx % 4) * 2
            ttk.Label(combat, text=label + ":").grid(
                row=row, column=col, sticky="e", padx=(12 if col == 0 else 8, 2), pady=6)
            var = tk.StringVar()
            ttk.Entry(combat, textvariable=var, width=7).grid(
                row=row, column=col + 1, sticky="w", pady=6, padx=(0, 4))
            self._vars[key] = var

        for key, default in [
            ("combat_follow_override_distance", "20.0"),
            ("reposition_distance", "8.0"),
            ("ranged_min_distance", "8.0"),
            ("ranged_optimal_distance", "25.0"),
            ("ranged_cast_range", "30.0"),
            ("ranged_retreat_distance", "5.0"),
            ("ranged_retreat_trigger_pct", "80.0"),
            ("ranged_retreat_reset_pct", "60.0"),
        ]:
            self._vars[key].set(default)

    def _build_targeting_tab(self, nb):
        frame = ttk.Frame(nb)
        nb.add(frame, text="  Targeting  ")

        target = ttk.LabelFrame(frame, text="Assist / Guard / Attack-Lock")
        target.pack(fill=tk.X, padx=8, pady=8)

        ttk.Label(target, text=(
            "These are direct party targeting rules: which sources companions may use in assist, guard, and attack-lock, "
            "plus how strict target attackability should be in each context."
        ), wraplength=760, justify=tk.LEFT, foreground="#555").grid(
            row=0, column=0, columnspan=4, sticky="w", padx=8, pady=(4, 6))

        bool_fields = [
            ("assist_use_current_victim", "Assist: Current Victim", True),
            ("assist_use_owner_victim", "Assist: Owner Victim", True),
            ("assist_owner_victim_must_target_owner", "Assist: Victim Must Fight Back", True),
            ("attack_lock_use_owner_victim", "Attack-Lock: Owner Victim", True),
            ("attack_lock_use_owner_selection", "Attack-Lock: Owner Selection", True),
            ("guard_use_current_victim", "Guard: Current Victim", True),
            ("guard_use_owner_attackers", "Guard: Owner Attackers", True),
            ("assist_require_targetable_for_attack", "Assist/Guard: Require Attackable Target", True),
            ("command_require_targetable_for_attack", "Command/Attack-Lock: Require Attackable Target", False),
        ]
        for idx, (key, label, default) in enumerate(bool_fields):
            row = 1 + idx // 2
            col = idx % 2
            var = tk.BooleanVar(value=default)
            ttk.Checkbutton(target, text=label, variable=var).grid(
                row=row, column=col, padx=12, pady=6, sticky="w")
            self._vars[key] = var

    def _build_misc_tab(self, nb):
        frame = ttk.Frame(nb)
        nb.add(frame, text="  Misc  ")

        misc = ttk.LabelFrame(frame, text="Misc Party Behaviour")
        misc.pack(fill=tk.X, padx=8, pady=8)

        ttk.Label(misc, text=(
            "These companion-party controls are reserved for future implementation, but they belong with party rules rather than world rules."
        ), wraplength=760, justify=tk.LEFT, foreground="#555").grid(
            row=0, column=0, columnspan=4, sticky="w", padx=8, pady=(4, 6))

        mv = tk.BooleanVar(value=True)
        ttk.Checkbutton(misc, text="Mount With Owner",
                        variable=mv).grid(row=1, column=0, padx=12, pady=8, sticky="w")
        ttk.Label(misc, text="(pending)", foreground="#999").grid(row=1, column=1, sticky="w")
        self._vars["mount_with_owner"] = mv

        av = tk.BooleanVar(value=False)
        ttk.Checkbutton(misc, text="Auto-Loot",
                        variable=av).grid(row=1, column=2, padx=24, pady=8, sticky="w")
        ttk.Label(misc, text="(pending)", foreground="#999").grid(row=1, column=3, sticky="w")
        self._vars["auto_loot"] = av

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
