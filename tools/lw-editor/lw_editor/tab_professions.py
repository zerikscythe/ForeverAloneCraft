"""
tab_professions.py -- ProfessionsPanel: embedded professions browser for CharacterEditorTab.
"""
import json
import pathlib
import tkinter as tk
from tkinter import ttk

from .constants import (
    SECONDARY_PROF_SKILL_LINES,
    CRAFTING_PROF_SKILL_LINES,
    GATHERING_PROF_SKILL_LINES,
    PROF_SPECIALIZATIONS,
    ALL_PLAYER_PROF_SKILL_LINES,
)
from .helpers import make_tv_sortable
from lw_editor import DATA_DIR

# ── Load recipe data once at module import ─────────────────────────────────
_SKILL_LINE_DATA: dict[str, list] = {}
_skill_line_path = DATA_DIR / "skill_line_abilities.json"
if _skill_line_path.exists():
    try:
        with _skill_line_path.open(encoding="utf-8") as _f:
            _SKILL_LINE_DATA = json.load(_f)
    except Exception:
        pass

# Reverse map: sub-skill-line-id → parent skill-line-id
_SPEC_TO_PARENT: dict[int, int] = {}
for _parent, _specs in PROF_SPECIALIZATIONS.items():
    for _spec_sl in _specs:
        _SPEC_TO_PARENT[_spec_sl] = _parent


class ProfessionsPanel(ttk.Frame):
    """
    Profession recipe browser embedded as a sub-tab inside CharacterEditorTab.

    Call load(skills, known_spells) to populate for a character.
    Call clear() when no character is selected.
    """

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._current_skills: dict[int, dict] = {}   # skill_id → {value, max}
        self._known_spells: set = set()
        self._build()

    # ── Build UI ───────────────────────────────────────────────────────────

    def _build(self):
        # Top info bar
        hdr = ttk.Frame(self)
        hdr.pack(fill=tk.X, padx=4, pady=(4, 2))
        ttk.Label(hdr, text="Professions", font=("TkDefaultFont", 10, "bold")).pack(side=tk.LEFT)
        self._hdr_lbl = ttk.Label(hdr, text="", foreground="gray")
        self._hdr_lbl.pack(side=tk.LEFT, padx=8)

        if not _SKILL_LINE_DATA:
            ttk.Label(self,
                text="⚠  No recipe data.  Run extract_dbc_data.py first.",
                foreground="orange").pack(padx=8, pady=8)
            self._nb = None
            return

        # Sub-notebook — one tab per profession
        self._nb = ttk.Notebook(self)
        self._nb.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Fixed secondary profession tabs (always present)
        self._fixed_tabs: dict[int, dict] = {}
        for skill_id, name in SECONDARY_PROF_SKILL_LINES.items():
            frame = ttk.Frame(self._nb)
            self._nb.add(frame, text=f"  {name}  ")
            tv, count_lbl = self._build_recipe_frame(frame)
            self._fixed_tabs[skill_id] = {"frame": frame, "tv": tv, "count_lbl": count_lbl}

        # Dynamic crafting profession tabs — populated in load()
        self._craft_tabs: list[dict] = []
        for _ in range(2):
            frame = ttk.Frame(self._nb)
            tv, count_lbl = self._build_recipe_frame(frame)
            self._craft_tabs.append({
                "frame": frame, "tv": tv, "count_lbl": count_lbl,
                "skill_id": None,
            })
            # Hidden by default — shown when a character with that prof is loaded
            # (we just don't add them to the notebook until needed)

    def _build_recipe_frame(self, parent: ttk.Frame) -> tuple:
        """Create the recipe treeview + count label inside a frame. Returns (tv, count_lbl)."""
        top = ttk.Frame(parent)
        top.pack(fill=tk.X, padx=4, pady=(4, 2))

        skill_lbl = ttk.Label(top, text="Skill: —", font=("TkDefaultFont", 9, "bold"))
        skill_lbl.pack(side=tk.LEFT)
        count_lbl = ttk.Label(top, text="", foreground="gray")
        count_lbl.pack(side=tk.LEFT, padx=12)

        # Store skill_lbl reference inside count_lbl widget for later update
        count_lbl._skill_lbl = skill_lbl

        wrap = ttk.Frame(parent)
        wrap.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        tv = ttk.Treeview(wrap,
            columns=("known", "name", "min_skill"),
            show="headings",
            selectmode="browse",
        )
        tv.heading("known",     text="✓")
        tv.heading("name",      text="Recipe")
        tv.heading("min_skill", text="Min Skill")
        tv.column("known",     width=28,  anchor="center", stretch=False)
        tv.column("name",      width=360, anchor="w")
        tv.column("min_skill", width=80,  anchor="center")

        vsb = ttk.Scrollbar(wrap, orient="vertical",   command=tv.yview)
        hsb = ttk.Scrollbar(wrap, orient="horizontal", command=tv.xview)
        tv.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)

        vsb.pack(side=tk.RIGHT,  fill=tk.Y)
        hsb.pack(side=tk.BOTTOM, fill=tk.X)
        tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Tag for known recipes (green text)
        tv.tag_configure("known",   foreground="#1a7f1a")
        tv.tag_configure("unknown", foreground="#888888")
        tv.tag_configure("section", foreground="#1a4a8a", font=("TkDefaultFont", 9, "bold"))

        make_tv_sortable(tv, numeric_cols={"min_skill"})

        return tv, count_lbl

    # ── Public interface ───────────────────────────────────────────────────

    def load(self, skills: list, known_spells: set):
        """Populate professions from cached character data."""
        if self._nb is None:
            return

        self._known_spells = known_spells
        self._current_skills = {int(r["skill"]): r for r in skills}

        # ── Identify which crafting professions this character has ─────────
        craft_skill_ids: list[int] = []
        for skill_id in CRAFTING_PROF_SKILL_LINES:
            if skill_id in self._current_skills:
                craft_skill_ids.append(skill_id)

        # Build header info string (show gathering too)
        gather_parts = []
        for skill_id, name in GATHERING_PROF_SKILL_LINES.items():
            if skill_id in self._current_skills:
                s = self._current_skills[skill_id]
                gather_parts.append(f"{name} {s['value']}/{s['max']}")
        self._hdr_lbl.configure(text="  •  ".join(gather_parts) if gather_parts else "")

        # ── Fill fixed secondary tabs ──────────────────────────────────────
        for skill_id, tab_info in self._fixed_tabs.items():
            skill_row = self._current_skills.get(skill_id)
            self._populate_prof_tab(tab_info, skill_id, skill_row)

        # ── Manage dynamic crafting tabs ───────────────────────────────────
        # Remove old dynamic tabs from notebook first
        for slot in self._craft_tabs:
            if slot["frame"] in self._nb.tabs():
                try:
                    self._nb.forget(slot["frame"])
                except Exception:
                    pass

        for i, slot in enumerate(self._craft_tabs):
            if i >= len(craft_skill_ids):
                slot["skill_id"] = None
                # Clear the treeview
                slot["tv"].delete(*slot["tv"].get_children())
                slot["count_lbl"].configure(text="")
                continue

            skill_id = craft_skill_ids[i]
            slot["skill_id"] = skill_id
            skill_row = self._current_skills.get(skill_id)

            # Determine tab label + spec info
            prof_name = CRAFTING_PROF_SKILL_LINES[skill_id]
            spec_name = self._detect_spec(skill_id)
            if spec_name:
                tab_label = f"  {prof_name} ({spec_name})  "
            else:
                tab_label = f"  {prof_name}  "

            self._nb.add(slot["frame"], text=tab_label)
            self._populate_prof_tab(slot, skill_id, skill_row, spec_name=spec_name)

    def clear(self):
        """Reset all tabs to empty state."""
        if self._nb is None:
            return
        for tab_info in self._fixed_tabs.values():
            tab_info["tv"].delete(*tab_info["tv"].get_children())
            tab_info["count_lbl"].configure(text="")
            tab_info["count_lbl"]._skill_lbl.configure(text="Skill: —")
        for slot in self._craft_tabs:
            slot["skill_id"] = None
            slot["tv"].delete(*slot["tv"].get_children())
            slot["count_lbl"].configure(text="")
            try:
                self._nb.forget(slot["frame"])
            except Exception:
                pass
        self._hdr_lbl.configure(text="")
        self._current_skills = {}
        self._known_spells = set()

    # ── Private helpers ────────────────────────────────────────────────────

    def _detect_spec(self, skill_id: int) -> str | None:
        """
        Return the specialisation name if the character's known spells
        include any recipe that belongs to a spec sub-skill-line.
        Returns None if no spec detected.
        """
        specs = PROF_SPECIALIZATIONS.get(skill_id)
        if not specs:
            return None
        for spec_skill_id, spec_name in specs.items():
            recipes = _SKILL_LINE_DATA.get(str(spec_skill_id), [])
            # If any spec-specific recipe is known by this character, spec is detected
            if any(r["spell_id"] in self._known_spells for r in recipes):
                return spec_name
        return None

    def _populate_prof_tab(self, tab_info: dict, skill_id: int,
                            skill_row: dict | None, spec_name: str | None = None):
        """Fill the recipe treeview for one profession tab."""
        tv        = tab_info["tv"]
        count_lbl = tab_info["count_lbl"]
        tv.delete(*tv.get_children())

        # Update skill level label
        if skill_row:
            val = skill_row.get("value", 0)
            mx  = skill_row.get("max",   0)
            count_lbl._skill_lbl.configure(text=f"Skill: {val} / {mx}")
        else:
            count_lbl._skill_lbl.configure(text="Skill: Not Learned")

        # Gather recipes from the base skill line + any detected spec sub-lines
        skill_lines_to_show: list[tuple[int, str]] = [(skill_id, "")]
        specs = PROF_SPECIALIZATIONS.get(skill_id, {})
        for spec_sl, sname in specs.items():
            # Show spec recipes if we detected a match OR include all if no spec found
            if spec_name is None or sname == spec_name:
                skill_lines_to_show.append((spec_sl, sname))

        total_recipes = 0
        total_known   = 0

        for sl_id, section_label in skill_lines_to_show:
            sl_recipes = _SKILL_LINE_DATA.get(str(sl_id), [])
            if not sl_recipes:
                continue

            if section_label:
                # Insert a non-selectable section header for spec recipes
                tv.insert("", "end", iid=f"__hdr_{sl_id}",
                          values=("", f"── {section_label} ──", ""),
                          tags=("section",))

            for recipe in sl_recipes:
                spell_id   = recipe["spell_id"]
                spell_name = recipe["spell_name"]
                min_skill  = recipe["min_skill"]
                known      = spell_id in self._known_spells
                tag        = "known" if known else "unknown"
                check      = "✓" if known else "—"
                tv.insert("", "end",
                          values=(check, spell_name, min_skill if min_skill else "—"),
                          tags=(tag,))
                total_recipes += 1
                if known:
                    total_known += 1

        count_lbl.configure(
            text=f"{total_known} / {total_recipes} known" if total_recipes else "No data",
            foreground="#1a7f1a" if total_known == total_recipes and total_recipes else "gray",
        )
