"""
tab_character.py -- CharacterEditorTab.
"""
import tkinter as tk
from tkinter import ttk, messagebox
from mysql.connector import Error as MySQLError
from .constants import (
    WOW_CLASSES, WOW_RACES, EQUIPMENT_SLOT_NAMES, ITEM_QUALITY_LABELS,
    TALENT_DATA,
    PERM_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT,
    SOCK_ENCHANTMENT_SLOT_2, SOCK_ENCHANTMENT_SLOT_3,
    ITEM_ENCHANTMENT_SLOT_COUNT, ITEM_ENCHANTMENT_OFFSET_COUNT,
)
from .db import db
from .helpers import (
    lbl, entry_w, money_text, unix_text,
    item_display_text, enchant_display_text,
    format_item_tooltip, format_gem_tooltip,
    parse_display_id, item_quality_filter_id,
    parse_item_enchantments, build_item_enchantments,
    get_item_enchant_id, set_item_enchant_id,
    socket_color_text, filter_enchant_rows_for_slot,
    make_tv_sortable,
)
from .tooltip import Tooltip

# ═══════════════════════════════════════════════════════════════════════════

class CharacterEditorTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._accounts = []
        self._chars = []
        self._selected_account_id = None
        self._account_by_label = {}
        self._sel_char = None
        self._gear_rows = {}
        self._gear_slot_current_rows = {}
        self._selected_gear_row = None
        self._gear_slot_controls = {}

        # ── Session caches ────────────────────────────────────────────────
        # Per-character data: guid → {summary, reputations, inventory_rows,
        #                              achievements, talents}
        self._char_cache: dict[int, dict] = {}

        # Per-slot option lists (class+quality independent of which toon is
        # loaded — safe to keep for the whole session):
        #   (class_id, slot_id, quality_id) → [item rows]
        self._item_opts_cache: dict[tuple, list] = {}
        #   slot_id → [enchant rows]
        self._enchant_opts_cache: dict[int, list] = {}
        #   socket_color → [gem rows]
        self._gem_opts_cache: dict[int, list] = {}

        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=260)
        pane.add(left, weight=0)

        ttk.Label(left, text="Account").pack(anchor="w", padx=4, pady=(4, 0))
        self.v_account = tk.StringVar()
        self._acct_cb = ttk.Combobox(left, textvariable=self.v_account, state="readonly", width=36)
        self._acct_cb.pack(fill=tk.X, padx=4)
        self._acct_cb.bind("<<ComboboxSelected>>", self._on_account_select)

        ttk.Label(left, text="Characters").pack(anchor="w", padx=4, pady=(6, 0))
        cols = ("name", "lvl", "class")
        char_wrap = ttk.Frame(left)
        char_wrap.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        self._char_tv = ttk.Treeview(char_wrap, columns=cols, show="headings", height=18, selectmode="browse")
        self._char_tv.heading("name", text="Name")
        self._char_tv.heading("lvl", text="Lvl")
        self._char_tv.heading("class", text="Class")
        self._char_tv.column("name", width=115)
        self._char_tv.column("lvl", width=40, anchor="center")
        self._char_tv.column("class", width=85)
        char_scroll = ttk.Scrollbar(char_wrap, orient="vertical", command=self._char_tv.yview)
        self._char_tv.configure(yscrollcommand=char_scroll.set)
        self._char_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        char_scroll.pack(side=tk.LEFT, fill=tk.Y)
        self._char_tv.bind("<<TreeviewSelect>>", self._on_char_select)
        make_tv_sortable(self._char_tv, numeric_cols={"lvl"})

        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        top_btns = ttk.Frame(right)
        top_btns.pack(fill=tk.X, padx=4, pady=(4, 0))
        ttk.Button(top_btns, text="🔄 Refresh Character", command=self._reload_selected_character).pack(side=tk.LEFT, padx=2)

        detail_nb = ttk.Notebook(right)
        detail_nb.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        sum_tab  = ttk.Frame(detail_nb)
        rep_tab  = ttk.Frame(detail_nb)
        gear_tab = ttk.Frame(detail_nb)
        inv_tab  = ttk.Frame(detail_nb)
        ach_tab  = ttk.Frame(detail_nb)
        tal_tab  = ttk.Frame(detail_nb)
        prof_tab = ttk.Frame(detail_nb)
        detail_nb.add(sum_tab,  text="  Summary  ")
        detail_nb.add(rep_tab,  text="  Reputation  ")
        detail_nb.add(gear_tab, text="  Equipped Gear  ")
        detail_nb.add(inv_tab,  text="  Inventory  ")
        detail_nb.add(ach_tab,  text="  Achievements  ")
        detail_nb.add(tal_tab,  text="  Talents  ")
        detail_nb.add(prof_tab, text="  Professions  ")

        # ── Summary tab ────────────────────────────────────────────────────
        sum_inner = ttk.Frame(sum_tab, padding=6)
        sum_inner.pack(fill=tk.X, padx=4, pady=4)
        summary = sum_inner   # alias so the grid calls below are unchanged

        self.v_char_name = tk.StringVar()
        self.v_char_guid = tk.StringVar()
        self.v_char_account = tk.StringVar()
        self.v_char_class = tk.StringVar()
        self.v_char_race = tk.StringVar()
        self.v_char_guild = tk.StringVar()
        self.v_char_online = tk.StringVar()
        self.v_char_created = tk.StringVar()
        self.v_level = tk.StringVar()
        self.v_xp = tk.StringVar()
        self.v_money = tk.StringVar()
        self.v_money_text = tk.StringVar()
        self.v_bank_slots = tk.StringVar()
        self.v_guild_bank_money = tk.StringVar()
        self.v_location = tk.StringVar()

        # Individual stat StringVars (replaces old single v_stat_summary)
        self.v_health      = tk.StringVar()
        self.v_power       = tk.StringVar()
        self.v_armor       = tk.StringVar()
        self.v_strength    = tk.StringVar()
        self.v_agility     = tk.StringVar()
        self.v_stamina     = tk.StringVar()
        self.v_intellect   = tk.StringVar()
        self.v_spirit      = tk.StringVar()
        self.v_atk_power   = tk.StringVar()
        self.v_ranged_ap   = tk.StringVar()
        self.v_spell_power = tk.StringVar()
        self.v_resilience  = tk.StringVar()
        self.v_arena_pts   = tk.StringVar()
        self.v_honor_pts   = tk.StringVar()
        self.v_total_kills = tk.StringVar()

        lbl(summary, "Name:", 0, 0)
        entry_w(summary, self.v_char_name, 0, 1, width=18, state="readonly")
        lbl(summary, "GUID:", 0, 2)
        entry_w(summary, self.v_char_guid, 0, 3, width=8, state="readonly")
        lbl(summary, "Account:", 0, 4)
        entry_w(summary, self.v_char_account, 0, 5, width=8, state="readonly")

        lbl(summary, "Class:", 1, 0)
        entry_w(summary, self.v_char_class, 1, 1, width=18, state="readonly")
        lbl(summary, "Race:", 1, 2)
        entry_w(summary, self.v_char_race, 1, 3, width=14, state="readonly")
        lbl(summary, "Online:", 1, 4)
        entry_w(summary, self.v_char_online, 1, 5, width=10, state="readonly")

        lbl(summary, "Level:", 2, 0)
        entry_w(summary, self.v_level, 2, 1, width=8)
        lbl(summary, "XP:", 2, 2)
        entry_w(summary, self.v_xp, 2, 3, width=12)
        lbl(summary, "Bank Slots:", 2, 4)
        entry_w(summary, self.v_bank_slots, 2, 5, width=8)

        lbl(summary, "Gold (copper):", 3, 0)
        entry_w(summary, self.v_money, 3, 1, width=12)
        lbl(summary, "Gold:", 3, 2)
        entry_w(summary, self.v_money_text, 3, 3, width=16, state="readonly")
        lbl(summary, "Guild Bank Gold:", 3, 4)
        entry_w(summary, self.v_guild_bank_money, 3, 5, width=16, state="readonly")

        lbl(summary, "Guild:", 4, 0)
        entry_w(summary, self.v_char_guild, 4, 1, width=18, state="readonly")
        lbl(summary, "Location:", 4, 2)
        entry_w(summary, self.v_location, 4, 3, width=18, state="readonly")
        lbl(summary, "Created:", 4, 4)
        entry_w(summary, self.v_char_created, 4, 5, width=18, state="readonly")

        # ── Stored Stats grid ──────────────────────────────────────────────
        ttk.Separator(summary, orient="horizontal").grid(
            row=5, column=0, columnspan=6, sticky="ew", padx=4, pady=(6, 2))

        stats_frame = ttk.Frame(summary)
        stats_frame.grid(row=6, column=0, columnspan=6, sticky="w", padx=4, pady=(0, 4))

        def stat_pair(parent, row, col, label, var, width=10):
            ttk.Label(parent, text=label, foreground="gray").grid(
                row=row, column=col, sticky="e", padx=(8, 2), pady=1)
            ttk.Label(parent, textvariable=var, width=width, anchor="w",
                      font=("TkDefaultFont", 9, "bold")).grid(
                row=row, column=col+1, sticky="w", padx=(0, 6), pady=1)

        # Row 0 — Combat
        stat_pair(stats_frame, 0, 0, "Health:",      self.v_health,      width=14)
        stat_pair(stats_frame, 0, 2, "Power:",       self.v_power,       width=14)
        stat_pair(stats_frame, 0, 4, "Armor:",       self.v_armor,       width=8)

        # Row 1 — Primary attributes
        stat_pair(stats_frame, 1, 0, "Strength:",    self.v_strength,    width=6)
        stat_pair(stats_frame, 1, 2, "Agility:",     self.v_agility,     width=6)
        stat_pair(stats_frame, 1, 4, "Stamina:",     self.v_stamina,     width=6)
        stat_pair(stats_frame, 1, 6, "Intellect:",   self.v_intellect,   width=6)
        stat_pair(stats_frame, 1, 8, "Spirit:",      self.v_spirit,      width=6)

        # Row 2 — Offensive
        stat_pair(stats_frame, 2, 0, "Attack Power:",self.v_atk_power,   width=8)
        stat_pair(stats_frame, 2, 2, "Ranged AP:",   self.v_ranged_ap,   width=8)
        stat_pair(stats_frame, 2, 4, "Spell Power:", self.v_spell_power, width=8)
        stat_pair(stats_frame, 2, 6, "Resilience:",  self.v_resilience,  width=8)

        # Row 3 — PvP
        stat_pair(stats_frame, 3, 0, "Arena Points:",self.v_arena_pts,   width=8)
        stat_pair(stats_frame, 3, 2, "Honor Points:",self.v_honor_pts,   width=8)
        stat_pair(stats_frame, 3, 4, "Total Kills:", self.v_total_kills, width=8)

        save_row = ttk.Frame(sum_tab)
        save_row.pack(anchor="w", padx=8, pady=(0, 6))
        ttk.Button(save_row, text="💾 Save Basic Stats", command=self._save_basic_stats).pack(side=tk.LEFT)

        rep_wrap = ttk.Frame(rep_tab)
        rep_wrap.pack(fill=tk.BOTH, expand=True)

        # Sub-notebook: one tab per expansion + one "Unknown/Other" tab
        self._rep_nb = ttk.Notebook(rep_wrap)
        self._rep_nb.pack(fill=tk.BOTH, expand=True)

        EXPANSION_ORDER = [
            "Classic",
            "The Burning Crusade",
            "Wrath of the Lich King",
            "Other",
        ]
        self._rep_tvs: dict[str, ttk.Treeview] = {}
        for xpac in EXPANSION_ORDER:
            frame = ttk.Frame(self._rep_nb)
            short = {"Classic": "Classic", "The Burning Crusade": "TBC",
                     "Wrath of the Lich King": "WotLK", "Other": "Other"}
            self._rep_nb.add(frame, text=f"  {short.get(xpac, xpac)}  ")

            tv = ttk.Treeview(frame,
                columns=("faction_name", "group", "standing", "standing_label", "flags"),
                show="headings")
            tv.heading("faction_name",    text="Faction")
            tv.heading("group",           text="Group")
            tv.heading("standing",        text="Raw Standing")
            tv.heading("standing_label",  text="Rank")
            tv.heading("flags",           text="Flags")
            tv.column("faction_name",   width=220, anchor="w")
            tv.column("group",          width=160, anchor="w")
            tv.column("standing",       width=90,  anchor="center")
            tv.column("standing_label", width=100, anchor="center")
            tv.column("flags",          width=60,  anchor="center")

            # Color tags per standing tier
            tv.tag_configure("exalted",    foreground="#1a7f1a")
            tv.tag_configure("revered",    foreground="#4a7fc0")
            tv.tag_configure("honored",    foreground="#2a8f8f")
            tv.tag_configure("friendly",   foreground="#6a6a00")
            tv.tag_configure("neutral",    foreground="#888888")
            tv.tag_configure("unfriendly", foreground="#c06030")
            tv.tag_configure("hostile",    foreground="#c03030")
            tv.tag_configure("hated",      foreground="#990000")

            scr = ttk.Scrollbar(frame, orient="vertical", command=tv.yview)
            tv.configure(yscrollcommand=scr.set)
            scr.pack(side=tk.RIGHT, fill=tk.Y)
            tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            self._rep_tvs[xpac] = tv
            make_tv_sortable(tv, numeric_cols={"standing", "flags"})

        quick_gear = ttk.LabelFrame(gear_tab, text="Equipped Gear Editor", padding=6)
        quick_gear.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        gear_canvas = tk.Canvas(quick_gear, highlightthickness=0)
        gear_scroll = ttk.Scrollbar(quick_gear, orient="vertical", command=gear_canvas.yview)
        self._gear_rows_frame = ttk.Frame(gear_canvas)

        self._gear_rows_frame.bind(
            "<Configure>",
            lambda _e: gear_canvas.configure(scrollregion=gear_canvas.bbox("all"))
        )

        gear_canvas.create_window((0, 0), window=self._gear_rows_frame, anchor="nw")
        gear_canvas.configure(yscrollcommand=gear_scroll.set)

        gear_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        gear_scroll.pack(side=tk.LEFT, fill=tk.Y)

        ttk.Label(self._gear_rows_frame, text="Quality Filter").grid(row=0, column=1, sticky="w", padx=4, pady=(0, 4))
        ttk.Label(self._gear_rows_frame, text="Item").grid(row=0, column=2, sticky="w", padx=4, pady=(0, 4))
        ttk.Label(self._gear_rows_frame, text="Enchant").grid(row=0, column=3, sticky="w", padx=4, pady=(0, 4))
        ttk.Label(self._gear_rows_frame, text="Gems").grid(row=0, column=4, sticky="w", padx=4, pady=(0, 4))

        row_idx = 1
        for slot_id, slot_name in EQUIPMENT_SLOT_NAMES.items():
            var = tk.StringVar()
            quality_var = tk.StringVar(value=ITEM_QUALITY_LABELS[0])
            enchant_var = tk.StringVar()
            ttk.Label(self._gear_rows_frame, text=f"{slot_name}:").grid(row=row_idx, column=0, sticky="w", padx=4, pady=2)

            quality_combo = ttk.Combobox(
                self._gear_rows_frame,
                textvariable=quality_var,
                values=ITEM_QUALITY_LABELS,
                width=18,
                state="readonly",
            )
            quality_combo.grid(row=row_idx, column=1, sticky="w", padx=4, pady=2)

            combo = ttk.Combobox(self._gear_rows_frame, textvariable=var, width=34, state="readonly")
            combo.grid(row=row_idx, column=2, sticky="we", padx=4, pady=2)
            combo.bind("<<ComboboxSelected>>", lambda _e, sid=slot_id: self._refresh_gear_slot_row(sid))

            def _make_item_tip(iv=var):
                eid = parse_display_id(iv.get())
                if not eid:
                    return ""
                row = db.item_tooltip_data(eid)
                return format_item_tooltip(row, db._load_dbc_enchant_cache(),
                                           spell_name_fn=db.spell_name) if row else ""

            Tooltip(combo, text_fn=_make_item_tip)

            enchant_combo = ttk.Combobox(self._gear_rows_frame, textvariable=enchant_var, width=28, state="readonly")
            enchant_combo.grid(row=row_idx, column=3, sticky="we", padx=4, pady=2)

            def _make_enchant_tip(ev=enchant_var):
                eid = parse_display_id(ev.get())
                if not eid:
                    return ""
                cache = db._load_dbc_enchant_cache()
                entry = next((e for e in cache if int(e.get("ID", 0)) == eid), None)
                if not entry:
                    return ""
                spell     = entry.get("SpellName", "").strip()
                effects   = entry.get("Effects") or []
                min_level = int(entry.get("MinLevel", 0) or 0)
                # Title: prefer the enchanting spell name, fall back to stat text
                title = spell or entry.get("Name_Lang_enUS", "").strip()
                lines = [title] if title else []
                if effects:
                    lines.append("Effects:  " + "  •  ".join(effects))
                elif not spell:
                    # No decoded effects and no spell name — nothing extra to show
                    return ""
                if min_level:
                    lines.append(f"Min item level: {min_level}")
                return "\n".join(lines)

            Tooltip(enchant_combo, text_fn=_make_enchant_tip)

            gem_frame = ttk.Frame(self._gear_rows_frame)
            gem_frame.grid(row=row_idx, column=4, sticky="we", padx=4, pady=2)
            gem_controls = []
            for gem_index in range(3):
                gem_var = tk.StringVar()
                gem_combo = ttk.Combobox(gem_frame, textvariable=gem_var, width=22, state="readonly")
                gem_combo.grid(row=0, column=gem_index, sticky="we", padx=(0 if gem_index == 0 else 4, 0))

                def _make_gem_tip(gv=gem_var):
                    eid = parse_display_id(gv.get())
                    if not eid or not db.ok():
                        return ""
                    try:
                        rows = db.q(db.world,
                            "SELECT it.name, gp.Type AS gem_color, gp.Enchant_Id "
                            "FROM item_template it "
                            "JOIN gemproperties_dbc gp ON gp.ID = it.GemProperties "
                            "WHERE it.entry=%s LIMIT 1", (eid,))
                        if rows:
                            return format_gem_tooltip(rows[0], db._load_dbc_enchant_cache())
                    except Exception:
                        pass
                    # DB table empty — use item name + JSON gem properties cache
                    try:
                        name_rows = db.q(db.world,
                            "SELECT name, GemProperties FROM item_template WHERE entry=%s LIMIT 1",
                            (eid,))
                        if not name_rows:
                            return ""
                        name = name_rows[0].get("name", "")
                        gemprop_id = int(name_rows[0].get("GemProperties") or 0)
                        gemprops = db._load_dbc_gemproperties_cache()
                        gp = gemprops.get(gemprop_id, {})
                        gem_row = {
                            "name":      name,
                            "gem_color": gp.get("Type", 0),
                            "Enchant_Id": gp.get("Enchant_Id", 0),
                        }
                        return format_gem_tooltip(gem_row, db._load_dbc_enchant_cache())
                    except Exception:
                        return ""

                Tooltip(gem_combo, text_fn=_make_gem_tip)
                gem_controls.append({"var": gem_var, "combo": gem_combo})

            quality_combo.bind("<<ComboboxSelected>>", lambda _e, sid=slot_id: self._refresh_gear_slot_row(sid))
            self._gear_slot_controls[slot_id] = {
                "var": var,
                "combo": combo,
                "quality_var": quality_var,
                "quality_combo": quality_combo,
                "enchant_var": enchant_var,
                "enchant_combo": enchant_combo,
                "gem_frame": gem_frame,
                "gems": gem_controls,
            }
            row_idx += 1

        ttk.Button(self._gear_rows_frame, text="Apply Slot Rows", command=self._apply_gear_slot_rows).grid(
            row=row_idx, column=2, sticky="w", padx=4, pady=(8, 2))
        self._gear_rows_frame.columnconfigure(2, weight=1)
        self._gear_rows_frame.columnconfigure(3, weight=1)
        self._gear_rows_frame.columnconfigure(4, weight=1)

        inv_wrap = ttk.Frame(inv_tab)
        inv_wrap.pack(fill=tk.BOTH, expand=True)
        self._inv_tv = ttk.Treeview(inv_wrap, columns=("bag", "slot", "entry", "name", "count", "item_guid"), show="headings")
        self._inv_tv.heading("bag", text="Bag")
        self._inv_tv.heading("slot", text="Slot")
        self._inv_tv.heading("entry", text="Entry")
        self._inv_tv.heading("name", text="Item")
        self._inv_tv.heading("count", text="Count")
        self._inv_tv.heading("item_guid", text="Item GUID")
        self._inv_tv.column("bag", width=60, anchor="center")
        self._inv_tv.column("slot", width=60, anchor="center")
        self._inv_tv.column("entry", width=70, anchor="center")
        self._inv_tv.column("name", width=300)
        self._inv_tv.column("count", width=60, anchor="center")
        self._inv_tv.column("item_guid", width=90, anchor="center")
        inv_scroll = ttk.Scrollbar(inv_wrap, orient="vertical", command=self._inv_tv.yview)
        self._inv_tv.configure(yscrollcommand=inv_scroll.set)
        self._inv_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        inv_scroll.pack(side=tk.LEFT, fill=tk.Y)
        make_tv_sortable(self._inv_tv, numeric_cols={"bag", "slot", "entry", "count", "item_guid"})

        ach_wrap = ttk.Frame(ach_tab)
        ach_wrap.pack(fill=tk.BOTH, expand=True)
        self._ach_tv = ttk.Treeview(ach_wrap, columns=("achievement", "name", "date"), show="headings")
        self._ach_tv.heading("achievement", text="Achievement")
        self._ach_tv.heading("name", text="Name")
        self._ach_tv.heading("date", text="Earned")
        self._ach_tv.column("achievement", width=100, anchor="center")
        self._ach_tv.column("name", width=360)
        self._ach_tv.column("date", width=160)
        ach_scroll = ttk.Scrollbar(ach_wrap, orient="vertical", command=self._ach_tv.yview)
        self._ach_tv.configure(yscrollcommand=ach_scroll.set)
        self._ach_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        ach_scroll.pack(side=tk.LEFT, fill=tk.Y)
        make_tv_sortable(self._ach_tv, numeric_cols={"achievement"})
        tal_top = ttk.Frame(tal_tab)
        tal_top.pack(fill=tk.X, padx=4, pady=4)
        self.v_talent_points = tk.StringVar(value="")
        ttk.Label(tal_top, text="Available points:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Label(tal_top, textvariable=self.v_talent_points, font=("TkDefaultFont", 10, "bold")).pack(side=tk.LEFT)
        ttk.Button(tal_top, text="➕ Add Talent Point", command=self._add_talent_point).pack(side=tk.RIGHT, padx=2)
        ttk.Button(tal_top, text="🔄 Reset All Talents", command=self._reset_talents).pack(side=tk.RIGHT, padx=2)

        tal_trees_frame = ttk.Frame(tal_tab)
        tal_trees_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))
        tal_trees_frame.columnconfigure(0, weight=1)
        tal_trees_frame.columnconfigure(1, weight=1)
        tal_trees_frame.columnconfigure(2, weight=1)

        self._tal_tree_frames = []  # list of 3 ttk.LabelFrames
        self._tal_tvs = []          # list of 3 Treeview widgets
        for col in range(3):
            lf = ttk.LabelFrame(tal_trees_frame, text=f"Tree {col}", padding=4)
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
            tv.heading("row",  text="Row")
            tv.column("name", width=160)
            tv.column("rank", width=70,  anchor="center")
            tv.column("row",  width=40,  anchor="center")
            scr = ttk.Scrollbar(lf, orient="vertical", command=tv.yview)
            tv.configure(yscrollcommand=scr.set)
            tv.grid(row=0, column=0, sticky="nsew")
            scr.grid(row=0, column=1, sticky="ns")
            self._tal_tree_frames.append(lf)
            self._tal_tvs.append(tv)

        # ── Professions tab ────────────────────────────────────────────────
        from .tab_professions import ProfessionsPanel
        self._prof_panel = ProfessionsPanel(prof_tab)
        self._prof_panel.pack(fill=tk.BOTH, expand=True)

        self._clear_detail()

    def _account_label(self, row: dict) -> str:
        return f"{row.get('username', '')} [{row.get('account_id')}] ({row.get('char_count', 0)} chars)"

    # ── Cache helpers ──────────────────────────────────────────────────────

    def _fetch_char_data(self, guid: int) -> dict | None:
        """Fetch all character data from DB in one pass and return as a dict."""
        summary = db.load_character_summary(guid)
        if not summary:
            return None
        return {
            "summary":        summary,
            "reputations":    db.load_character_reputations(guid),
            "inventory_rows": db.load_character_inventory_rows(guid),
            "achievements":   db.load_character_achievements(guid),
            "talents":        db.load_character_talents(guid),
            "skills":         db.load_character_skills(guid),
            "known_spells":   db.load_character_known_spells(guid),
        }

    def _char_data(self, guid: int, force: bool = False) -> dict | None:
        """Return cached character data, fetching from DB only on first access."""
        if force or guid not in self._char_cache:
            data = self._fetch_char_data(guid)
            if data is None:
                return None
            self._char_cache[guid] = data
        return self._char_cache[guid]

    def _invalidate_char(self, guid: int, *keys: str):
        """Invalidate specific keys in the character cache (or the whole entry)."""
        if guid not in self._char_cache:
            return
        if keys:
            for k in keys:
                self._char_cache[guid].pop(k, None)
        else:
            del self._char_cache[guid]

    def _refresh_char_cache_key(self, guid: int, key: str):
        """Re-fetch one cache key from the DB without touching the rest."""
        fetchers = {
            "summary":        lambda: db.load_character_summary(guid),
            "reputations":    lambda: db.load_character_reputations(guid),
            "inventory_rows": lambda: db.load_character_inventory_rows(guid),
            "achievements":   lambda: db.load_character_achievements(guid),
            "talents":        lambda: db.load_character_talents(guid),
            "skills":         lambda: db.load_character_skills(guid),
            "known_spells":   lambda: db.load_character_known_spells(guid),
        }
        if key in fetchers and guid in self._char_cache:
            self._char_cache[guid][key] = fetchers[key]()

    def _get_item_options(self, class_id, slot_id: int, quality_id) -> list:
        """Cached equippable items for a slot — class+quality specific."""
        key = (class_id, slot_id, quality_id)
        if key not in self._item_opts_cache:
            self._item_opts_cache[key] = db.load_valid_equippable_items_for_slot(
                class_id, slot_id, quality=quality_id, limit=250)
        return self._item_opts_cache[key]

    def _get_enchant_options(self, slot_id: int) -> list:
        """Cached enchant list for a slot — never changes in a session."""
        if slot_id not in self._enchant_opts_cache:
            self._enchant_opts_cache[slot_id] = db.search_item_enchantments(
                "", slot_id=slot_id, limit=250)
        return self._enchant_opts_cache[slot_id]

    def _get_gem_options(self, socket_color: int) -> list:
        """Cached gem list for a socket color — never changes in a session."""
        if socket_color not in self._gem_opts_cache:
            rows = db.search_gem_items("", socket_color=socket_color)
            if not rows:
                rows = db.search_gem_items("", socket_color=0)
            self._gem_opts_cache[socket_color] = rows
        return self._gem_opts_cache[socket_color]

    def clear_session_caches(self):
        """Drop all caches — call on disconnect or account switch."""
        self._char_cache.clear()
        self._item_opts_cache.clear()
        self._enchant_opts_cache.clear()
        self._gem_opts_cache.clear()

    def _clear_detail(self):
        self._sel_char = None
        self._gear_rows = {}
        self._gear_slot_current_rows = {}
        for var in (
            self.v_char_name, self.v_char_guid, self.v_char_account, self.v_char_class,
            self.v_char_race, self.v_char_guild, self.v_char_online, self.v_char_created,
            self.v_level, self.v_xp, self.v_money, self.v_money_text, self.v_bank_slots,
            self.v_guild_bank_money, self.v_location,
            self.v_health, self.v_power, self.v_armor,
            self.v_strength, self.v_agility, self.v_stamina, self.v_intellect, self.v_spirit,
            self.v_atk_power, self.v_ranged_ap, self.v_spell_power, self.v_resilience,
            self.v_arena_pts, self.v_honor_pts, self.v_total_kills,
        ):
            var.set("")
        for tv in self._rep_tvs.values():
            tv.delete(*tv.get_children())
        self._inv_tv.delete(*self._inv_tv.get_children())
        self._ach_tv.delete(*self._ach_tv.get_children())
        self.v_talent_points.set("")
        for tv in self._tal_tvs:
            tv.delete(*tv.get_children())
        for lf in self._tal_tree_frames:
            lf.configure(text="Tree")
        for controls in self._gear_slot_controls.values():
            controls["var"].set("")
            controls["combo"].configure(values=[""])
            controls["quality_var"].set(ITEM_QUALITY_LABELS[0])
            controls["enchant_var"].set("")
            controls["enchant_combo"].configure(values=[""])
            for gem in controls["gems"]:
                gem["var"].set("")
                gem["combo"].configure(values=[""])
                gem["combo"].grid_remove()
        if hasattr(self, "_prof_panel"):
            self._prof_panel.clear()

    def _select_account_id(self, account_id: int | None):
        if not self._accounts:
            self.v_account.set("")
            self._selected_account_id = None
            self._char_tv.delete(*self._char_tv.get_children())
            self._clear_detail()
            return

        if account_id is None:
            account_id = self._accounts[0].get("account_id")

        for row in self._accounts:
            if row.get("account_id") == account_id:
                self.v_account.set(self._account_label(row))
                self._selected_account_id = account_id
                self._load_characters_for_selected_account()
                return

        first = self._accounts[0]
        self.v_account.set(self._account_label(first))
        self._selected_account_id = first.get("account_id")
        self._load_characters_for_selected_account()

    def _load_characters_for_selected_account(self):
        self._char_tv.delete(*self._char_tv.get_children())
        self._clear_detail()
        self._char_cache.clear()   # char data is account-scoped; drop stale entries

        if not self._selected_account_id:
            return

        self._chars = db.load_source_characters_for_account(self._selected_account_id)
        for c in self._chars:
            cls = WOW_CLASSES.get(c.get("class", 0), str(c.get("class", "")))
            self._char_tv.insert("", "end", iid=str(c["guid"]), values=(c["name"], c["level"], cls))

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
            self._accounts = db.load_player_accounts()
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

    def _reload_selected_character(self):
        """Force a full re-fetch from DB, bypassing the cache."""
        if self._sel_char:
            guid = self._sel_char["guid"]
            self._invalidate_char(guid)
            self._load_character_data(guid)

    def _on_char_select(self, _=None):
        sel = self._char_tv.selection()
        if not sel:
            return
        guid = int(sel[0])
        self._sel_char = next((c for c in self._chars if c["guid"] == guid), None)
        if not self._sel_char:
            return
        self._load_character_data(guid)

    def _load_character_data(self, guid: int):
        data = self._char_data(guid)
        if not data:
            self._clear_detail()
            return

        summary = data["summary"]

        self.v_char_name.set(summary.get("name", ""))
        self.v_char_guid.set(str(summary.get("guid", "")))
        self.v_char_account.set(str(summary.get("account", "")))
        self.v_char_class.set(WOW_CLASSES.get(summary.get("class", 0), str(summary.get("class", ""))))
        self.v_char_race.set(WOW_RACES.get(summary.get("race", 0), str(summary.get("race", ""))))
        self.v_char_guild.set(summary.get("guild_name") or "")
        self.v_char_online.set("Yes" if summary.get("online") else "No")
        self.v_char_created.set(str(summary.get("creation_date", "") or ""))
        self.v_level.set(str(summary.get("level", 0)))
        self.v_xp.set(str(summary.get("xp", 0)))
        self.v_money.set(str(summary.get("money", 0)))
        self.v_money_text.set(money_text(summary.get("money", 0)))
        self.v_bank_slots.set(str(summary.get("bankSlots", 0)))
        guild_bank_money = summary.get("guild_bank_money")
        self.v_guild_bank_money.set(money_text(guild_bank_money) if guild_bank_money is not None else "")
        self.v_location.set(f"Map {summary.get('map', 0)} / Zone {summary.get('zone', 0)}")

        self.v_health.set(f"{summary.get('health', 0)} / {summary.get('maxhealth', 0)}")
        self.v_power.set(f"{summary.get('power1', 0)} / {summary.get('maxpower1', 0)}")
        self.v_armor.set(str(summary.get('armor', 0)))
        self.v_strength.set(str(summary.get('strength', 0)))
        self.v_agility.set(str(summary.get('agility', 0)))
        self.v_stamina.set(str(summary.get('stamina', 0)))
        self.v_intellect.set(str(summary.get('intellect', 0)))
        self.v_spirit.set(str(summary.get('spirit', 0)))
        self.v_atk_power.set(str(summary.get('attackPower', 0)))
        self.v_ranged_ap.set(str(summary.get('rangedAttackPower', 0)))
        self.v_spell_power.set(str(summary.get('spellPower', 0)))
        self.v_resilience.set(str(summary.get('resilience', 0)))
        self.v_arena_pts.set(str(summary.get('arenaPoints', 0)))
        self.v_honor_pts.set(str(summary.get('totalHonorPoints', 0)))
        self.v_total_kills.set(str(summary.get('totalKills', 0)))

        # Standing breakpoints (AzerothCore stores cumulative standing 0-based from -42000)
        def standing_label(val: int) -> tuple[str, str]:
            """Return (rank_name, color_tag) for a raw standing value."""
            v = int(val)
            if v >= 42000:   return "Exalted",    "exalted"
            if v >= 21000:   return "Revered",    "revered"
            if v >= 9000:    return "Honored",    "honored"
            if v >= 3000:    return "Friendly",   "friendly"
            if v >= 0:       return "Neutral",    "neutral"
            if v >= -3000:   return "Unfriendly", "unfriendly"
            if v >= -6000:   return "Hostile",    "hostile"
            return "Hated",       "hated"

        for tv in self._rep_tvs.values():
            tv.delete(*tv.get_children())

        for row in data["reputations"]:
            xpac   = row.get("expansion", "Other")
            tv     = self._rep_tvs.get(xpac) or self._rep_tvs["Other"]
            raw    = int(row.get("standing", 0))
            slabel, stag = standing_label(raw)
            tv.insert("", "end", tags=(stag,), values=(
                row.get("faction_name") or f"Faction {row.get('faction', 0)}",
                row.get("parent_name", ""),
                raw,
                slabel,
                row.get("flags", 0),
            ))

        self._gear_rows = {}
        self._inv_tv.delete(*self._inv_tv.get_children())
        for row in data["inventory_rows"]:
            item_name = row.get("item_name") or f"Item {row.get('itemEntry', 0)}"
            values = (
                row.get("itemEntry", 0),
                item_name,
                row.get("count", 1),
                row.get("item_guid", 0),
            )
            if row.get("bag", 0) == 0 and row.get("slot", -1) in EQUIPMENT_SLOT_NAMES:
                item_guid = int(row.get("item_guid", 0) or 0)
                self._gear_rows[item_guid] = dict(row)
            else:
                self._inv_tv.insert("", "end", values=(
                    row.get("bag", 0),
                    row.get("slot", 0),
                    *values,
                ))

        self._ach_tv.delete(*self._ach_tv.get_children())
        for row in data["achievements"]:
            self._ach_tv.insert("", "end", values=(
                row.get("achievement", 0),
                row.get("achievement_name") or "",
                unix_text(row.get("date", 0)),
            ))

        self._load_talent_data(guid, summary.get("level", 1), summary.get("class", 0),
                               known_talents=data["talents"])

        self._prof_panel.load(
            skills=data.get("skills", []),
            known_spells=data.get("known_spells", set()),
        )

        # Clear any selections left over from a previously viewed character so
        # _refresh_gear_slot_row shows this character's gear, not the last one's.
        for controls in self._gear_slot_controls.values():
            controls["var"].set("")
            controls["enchant_var"].set("")
            for gem in controls["gems"]:
                gem["var"].set("")

        self._refresh_gear_slot_rows()

    def _refresh_gear_slot_row(self, slot_id: int):
        controls = self._gear_slot_controls.get(slot_id)
        if not controls:
            return

        class_id = self._sel_char.get("class") if self._sel_char else None
        current_row = next((r for r in self._gear_rows.values() if int(r.get("slot", -1)) == slot_id), None)
        self._gear_slot_current_rows[slot_id] = current_row

        selected_display = (controls["var"].get() or "").strip()
        current_display = ""
        selected_item_id = parse_display_id(selected_display)
        if current_row:
            current_display = item_display_text(current_row.get("itemEntry", 0), current_row.get("item_name") or "")

        quality_id = item_quality_filter_id(controls["quality_var"].get())
        options = self._get_item_options(class_id, slot_id, quality_id)
        displays = [""] + [item_display_text(r.get("entry"), r.get("name") or "") for r in options]

        preferred_display = selected_display or current_display
        if preferred_display and preferred_display not in displays:
            displays.append(preferred_display)

        controls["combo"].configure(values=displays)
        controls["var"].set(preferred_display)

        item_id = selected_item_id or parse_display_id(current_display)
        current_item_id = int(current_row.get("itemEntry", 0) or 0) if current_row else 0
        enchant_values = parse_item_enchantments(current_row.get("enchantments", "") if current_row else "")
        current_enchant_id = get_item_enchant_id(enchant_values, PERM_ENCHANTMENT_SLOT)
        enchant_rows = self._get_enchant_options(slot_id)
        enchant_displays = [""] + [
            enchant_display_text(r.get("ID"), r.get("Name_Lang_enUS") or "", r.get("SpellName") or "")
            for r in enchant_rows
        ]
        current_enchant_display = enchant_display_text(
            current_enchant_id,
            db.enchant_name(current_enchant_id) or "",
            db.enchant_spell_name(current_enchant_id) or "",
        )
        if current_enchant_display and current_enchant_display not in enchant_displays:
            enchant_displays.append(current_enchant_display)
        controls["enchant_combo"].configure(values=enchant_displays)
        controls["enchant_var"].set(current_enchant_display)

        socket_colors = []
        use_current_item_sockets = current_row and current_item_id and current_item_id == int(item_id or 0)
        if use_current_item_sockets:
            socket_colors = [
                int(current_row.get("socketColor_1", 0) or 0),
                int(current_row.get("socketColor_2", 0) or 0),
                int(current_row.get("socketColor_3", 0) or 0),
            ]

        if item_id and not use_current_item_sockets:
            item_rows = db.q(db.world,
                "SELECT socketColor_1, socketColor_2, socketColor_3 FROM item_template WHERE entry=%s LIMIT 1",
                (int(item_id),))
            if item_rows:
                socket_colors = [
                    int(item_rows[0].get("socketColor_1", 0) or 0),
                    int(item_rows[0].get("socketColor_2", 0) or 0),
                    int(item_rows[0].get("socketColor_3", 0) or 0),
                ]

        socket_slots = [SOCK_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT_2, SOCK_ENCHANTMENT_SLOT_3]
        for gem_index, gem_controls in enumerate(controls["gems"]):
            socket_color = socket_colors[gem_index] if gem_index < len(socket_colors) else 0
            gem_slot = socket_slots[gem_index]
            current_gem_enchant = get_item_enchant_id(enchant_values, gem_slot)
            current_gem_item = db.lookup_gem_item_by_enchant(current_gem_enchant) if current_gem_enchant else None
            current_gem_display = item_display_text(
                current_gem_item.get("entry") if current_gem_item else 0,
                current_gem_item.get("name") if current_gem_item else "",
            )

            if socket_color:
                gem_rows = self._get_gem_options(socket_color)
                gem_displays = [""] + [item_display_text(r.get("entry"), r.get("name") or "") for r in gem_rows]
                if current_gem_display and current_gem_display not in gem_displays:
                    gem_displays.append(current_gem_display)
                gem_controls["combo"].configure(values=gem_displays)
                gem_controls["var"].set(current_gem_display)
                gem_controls["combo"].grid()
            else:
                gem_controls["var"].set("")
                gem_controls["combo"].configure(values=[""])
                gem_controls["combo"].grid_remove()

    def _refresh_gear_slot_rows(self):
        self._gear_slot_current_rows = {}
        for slot_id in self._gear_slot_controls:
            self._refresh_gear_slot_row(slot_id)

    def _apply_gear_slot_rows(self):
        if not self._sel_char:
            return
        try:
            char_guid = int(self._sel_char["guid"])
            changed = 0

            for slot_id, controls in self._gear_slot_controls.items():
                selected_item_id = parse_display_id(controls["var"].get())
                current_row = self._gear_slot_current_rows.get(slot_id)

                if not selected_item_id:
                    continue

                enchantments = parse_item_enchantments(
                    current_row.get("enchantments", "") if current_row
                    else build_item_enchantments([0] * (ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT))
                )
                selected_enchant_id = parse_display_id(controls["enchant_var"].get()) or 0
                set_item_enchant_id(enchantments, PERM_ENCHANTMENT_SLOT, selected_enchant_id)

                for gem_index, gem_controls in enumerate(controls["gems"]):
                    gem_item_id = parse_display_id(gem_controls["var"].get()) or 0
                    gem_enchant_id = db.get_gem_enchant_id(gem_item_id) if gem_item_id else 0
                    gem_slot = [SOCK_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT_2, SOCK_ENCHANTMENT_SLOT_3][gem_index]
                    set_item_enchant_id(enchantments, gem_slot, gem_enchant_id)

                enchantments_text = build_item_enchantments(enchantments)

                if current_row:
                    current_item_id = int(current_row.get("itemEntry", 0) or 0)
                    current_enchantments = current_row.get("enchantments", "") or build_item_enchantments([0] * (ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT))
                    if current_item_id == selected_item_id and current_enchantments == enchantments_text:
                        continue

                    db.update_item_instance_equipment(
                        int(current_row.get("item_guid", 0) or 0),
                        selected_item_id,
                        enchantments_text,
                        db.get_item_max_durability(selected_item_id),
                    )
                    changed += 1
                else:
                    item_guid = db.create_equipped_item_for_character(char_guid, slot_id, selected_item_id)
                    db.update_item_instance_equipment(
                        int(item_guid),
                        selected_item_id,
                        enchantments_text,
                        db.get_item_max_durability(selected_item_id),
                    )
                    changed += 1

            if changed:
                # Inventory changed — refresh that cache key then re-render.
                self._refresh_char_cache_key(char_guid, "inventory_rows")
                self._load_character_data(char_guid)
                messagebox.showinfo("Saved", f"Updated {changed} gear slot(s).")
            else:
                messagebox.showinfo("No changes", "No gear slot changes were applied.")
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Gear update error", str(e))


    # ── Talent tab helpers ──────────────────────────────────────────────────

    def _load_talent_data(self, guid: int, level: int, class_id: int,
                          known_talents: dict | None = None):
        """Populate the three talent-tree treeviews for the selected character."""
        for tv in self._tal_tvs:
            tv.delete(*tv.get_children())
        for lf in self._tal_tree_frames:
            lf.configure(text="—")
        self.v_talent_points.set("")

        if not TALENT_DATA:
            self.v_talent_points.set("Run extract_dbc_data.py to enable talent display")
            return

        class_key = str(class_id)
        trees = TALENT_DATA.get(class_key)
        if not trees:
            self.v_talent_points.set("No talent data for this class")
            return

        # Use pre-fetched talent dict if provided, otherwise fetch (and cache).
        if known_talents is None:
            self._refresh_char_cache_key(guid, "talents")
            cached = self._char_cache.get(guid, {})
            known_talents = cached.get("talents") or db.load_character_talents(guid)

        total_spent = 0
        for tree_idx in range(3):
            tree_key = str(tree_idx)
            tree = trees.get(tree_key, {})
            tree_name = tree.get("tree_name", f"Tree {tree_idx}")
            talents   = tree.get("talents", [])
            tv = self._tal_tvs[tree_idx]
            lf = self._tal_tree_frames[tree_idx]
            lf.configure(text=tree_name)

            tree_spent = 0
            for t in talents:
                rank_spells = t.get("rank_spell_ids", [])
                current_rank = sum(
                    1 for s in rank_spells if s in known_talents
                )
                max_rank = t.get("max_rank", len(rank_spells))
                tree_spent += current_rank
                total_spent += current_rank

                tag = "maxed" if current_rank == max_rank and max_rank > 0 else (
                      "partial" if current_rank > 0 else "")
                iid = f"tal_{guid}_{t['talent_id']}"
                tv.insert(
                    "", "end",
                    iid=iid,
                    values=(
                        t["name"],
                        f"{current_rank}/{max_rank}",
                        t["row"],
                    ),
                    tags=(tag,),
                )

            tv.tag_configure("maxed",   foreground="#00aa00")
            tv.tag_configure("partial", foreground="#cc8800")

        available = max(0, level - 9) - total_spent
        self.v_talent_points.set(
            f"{available} available  ({total_spent} spent / {max(0, level - 9)} total)"
        )

    def _add_talent_point(self):
        """Open a dialog to add one or more points to a talent."""
        if not self._sel_char:
            return
        if not TALENT_DATA:
            messagebox.showinfo("No data", "Run extract_dbc_data.py first to load talent data.")
            return

        class_id  = self._sel_char.get("class", 0)
        class_key = str(class_id)
        trees     = TALENT_DATA.get(class_key, {})
        if not trees:
            messagebox.showinfo("No data", "No talent tree data for this class.")
            return

        guid      = int(self._sel_char["guid"])
        level     = int(self.v_level.get() or 0)
        known     = db.load_character_talents(guid)
        total_spent = sum(
            1
            for tree in trees.values()
            for t in tree.get("talents", [])
            for s in t.get("rank_spell_ids", [])
            if s in known
        )
        available = max(0, level - 9) - total_spent

        if available <= 0:
            messagebox.showinfo("No points", "No talent points available.")
            return

        # Build a flat list of (display_label, talent_entry, current_rank) for
        # talents that still have room.
        choices = []
        for tree_idx in range(3):
            tree = trees.get(str(tree_idx), {})
            tree_name = tree.get("tree_name", f"Tree {tree_idx}")
            for t in tree.get("talents", []):
                rank_spells = t.get("rank_spell_ids", [])
                current_rank = sum(1 for s in rank_spells if s in known)
                max_rank = t.get("max_rank", len(rank_spells))
                if current_rank < max_rank:
                    label = f"[{tree_name}] {t['name']}  ({current_rank}/{max_rank})"
                    choices.append((label, t, current_rank))

        if not choices:
            messagebox.showinfo("Maxed", "All talents are already at max rank.")
            return

        dlg = tk.Toplevel(self)
        dlg.title("Add Talent Point")
        dlg.resizable(False, False)
        dlg.grab_set()

        ttk.Label(dlg, text="Select talent:").pack(anchor="w", padx=8, pady=(8, 0))
        choice_var = tk.StringVar()
        combo = ttk.Combobox(dlg, textvariable=choice_var, state="readonly", width=52)
        combo["values"] = [c[0] for c in choices]
        combo.current(0)
        combo.pack(fill=tk.X, padx=8, pady=4)

        ttk.Label(dlg, text="Points to add:").pack(anchor="w", padx=8)
        pts_var = tk.StringVar(value="1")
        pts_entry = ttk.Entry(dlg, textvariable=pts_var, width=6)
        pts_entry.pack(anchor="w", padx=8, pady=4)

        def _apply():
            idx = combo.current()
            if idx < 0:
                return
            _, talent, current_rank = choices[idx]
            try:
                pts = int(pts_var.get())
            except ValueError:
                messagebox.showerror("Invalid", "Points must be a number.", parent=dlg)
                return
            if pts < 1:
                messagebox.showerror("Invalid", "Points must be at least 1.", parent=dlg)
                return

            rank_spells = talent.get("rank_spell_ids", [])
            max_rank    = talent.get("max_rank", len(rank_spells))
            can_add     = min(pts, max_rank - current_rank, available)
            if can_add <= 0:
                messagebox.showerror("Invalid", "Cannot add any more points to this talent.", parent=dlg)
                return

            # Prerequisites: depends_on talent must have depends_on_rank points.
            dep_id   = talent.get("depends_on")
            dep_rank = talent.get("depends_on_rank", 0)
            if dep_id:
                dep_talent = next(
                    (t for tree in trees.values()
                     for t in tree.get("talents", [])
                     if t["talent_id"] == dep_id),
                    None,
                )
                if dep_talent:
                    dep_known = sum(1 for s in dep_talent.get("rank_spell_ids", []) if s in known)
                    if dep_known < dep_rank + 1:
                        messagebox.showerror(
                            "Prerequisite",
                            f"Requires {dep_rank + 1} point(s) in '{dep_talent['name']}' first.",
                            parent=dlg,
                        )
                        return

            try:
                for r in range(can_add):
                    spell_id = rank_spells[current_rank + r]
                    db.add_character_talent(guid, spell_id, spec=0)
                dlg.destroy()
                self._load_talent_data(guid, level, class_id)
            except Exception as e:
                messagebox.showerror("DB error", str(e), parent=dlg)

        btn_frame = ttk.Frame(dlg)
        btn_frame.pack(fill=tk.X, padx=8, pady=8)
        ttk.Button(btn_frame, text="Apply", command=_apply).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_frame, text="Cancel", command=dlg.destroy).pack(side=tk.LEFT, padx=4)

    def _reset_talents(self):
        if not self._sel_char:
            return
        name = self._sel_char.get("name", "this character")
        if not messagebox.askyesno("Reset Talents",
                f"Remove ALL talent points from {name}?\n\n"
                "This only affects the database row. "
                "If the character is online the change will take effect on next login."):
            return
        try:
            guid  = int(self._sel_char["guid"])
            level = int(self.v_level.get() or 0)
            db.reset_character_talents(guid, spec=0)
            self._refresh_char_cache_key(guid, "talents")
            cached_talents = self._char_cache.get(guid, {}).get("talents", {})
            self._load_talent_data(guid, level, self._sel_char.get("class", 0),
                                   known_talents=cached_talents)
            messagebox.showinfo("Done", f"Talent points reset for {name}.")
        except Exception as e:
            messagebox.showerror("DB error", str(e))

    def _save_basic_stats(self):
        if not self._sel_char:
            return
        try:
            guid = int(self._sel_char["guid"])
            level = int(self.v_level.get() or 0)
            xp = int(self.v_xp.get() or 0)
            money = int(self.v_money.get() or 0)
            bank_slots = int(self.v_bank_slots.get() or 0)
            db.update_character_core_stats(guid, level, xp, money, bank_slots)
            self.v_money_text.set(money_text(money))
            cls = WOW_CLASSES.get(self._sel_char.get("class", 0), str(self._sel_char.get("class", "")))
            self._char_tv.item(str(guid), values=(self._sel_char.get("name", ""), level, cls))
            self._refresh_char_cache_key(guid, "summary")
            self._load_character_data(guid)
            messagebox.showinfo("Saved", f"Updated basic stats for {self._sel_char.get('name', '')}.")
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Save error", str(e))

