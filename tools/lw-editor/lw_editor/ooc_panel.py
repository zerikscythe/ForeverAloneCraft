"""
ooc_panel.py  --  Out-of-combat behaviour panel for the Clone Profiles tab.

Each clone profile slot carries its own OOC settings so a healing slot can buff
the full party while a DPS slot buffs only itself, and gathering/looting rules
can be tuned per character regardless of which combat profile is active.
"""
import tkinter as tk
from tkinter import ttk, messagebox

from .constants import ITEM_QUALITY_OPTIONS, LOOT_CATEGORY_FLAGS
from .db import db

# ── Item quality lookup ──────────────────────────────────────────────────────
# Derived from the shared ITEM_QUALITY_OPTIONS constant so names and ordering
# stay in sync with the rest of the editor.
# We skip "Any" (None id) and cap at Epic(4) since skinning/looting above that
# is never a practical threshold.
QUALITY_TIERS: list[tuple[int, str]] = [
    (qid, label)
    for label, qid in ITEM_QUALITY_OPTIONS
    if qid is not None and qid <= 4
]
# [(0, "Poor / Gray"), (1, "Common / White"), (2, "Uncommon / Green"),
#  (3, "Rare / Blue"), (4, "Epic / Purple")]

# WoW quality colours used as foreground on the badge label.
QUALITY_HEX: dict[int, str] = {
    0: "#9d9d9d",  # Poor    – gray
    1: "#ffffff",  # Common  – white  (shown on dark bg)
    2: "#1eff00",  # Uncommon– green
    3: "#0070dd",  # Rare    – blue
    4: "#a335ee",  # Epic    – purple
}
# Common/white is invisible on the default background so use a dark fallback.
QUALITY_BADGE_BG: dict[int, str] = {
    0: "#3a3a3a",
    1: "#3a3a3a",
    2: "#1a2a1a",
    3: "#1a1a2a",
    4: "#1a1a2a",
}

_QUALITY_LABELS = [label for _, label in QUALITY_TIERS]
_QUALITY_IDS    = [qid   for qid,  _  in QUALITY_TIERS]


def _quality_label(qid: int) -> str:
    for tid, label in QUALITY_TIERS:
        if tid == qid:
            return label
    return QUALITY_TIERS[0][1]


def _quality_id(label: str) -> int:
    for qid, lbl in QUALITY_TIERS:
        if lbl == label:
            return qid
    return 0


class _QualityCombo(ttk.Frame):
    """Combobox + coloured badge that shows the WoW quality colour."""

    def __init__(self, parent, width=22, **kw):
        super().__init__(parent, **kw)
        self._var = tk.StringVar(value=_QUALITY_LABELS[0])

        self._cb = ttk.Combobox(self, textvariable=self._var,
                                values=_QUALITY_LABELS,
                                state="readonly", width=width)
        self._cb.pack(side=tk.LEFT)

        self._badge = tk.Label(self, text="  ●  ", font=("TkDefaultFont", 10, "bold"),
                               relief="flat", padx=2)
        self._badge.pack(side=tk.LEFT, padx=(4, 0))

        self._var.trace_add("write", lambda *_: self._refresh_badge())
        self._refresh_badge()

    def _refresh_badge(self):
        qid = _quality_id(self._var.get())
        fg = QUALITY_HEX.get(qid, "#9d9d9d")
        bg = QUALITY_BADGE_BG.get(qid, "#3a3a3a")
        self._badge.config(foreground=fg, background=bg)

    def get_id(self) -> int:
        return _quality_id(self._var.get())

    def set_id(self, qid: int):
        self._var.set(_quality_label(qid))

    @property
    def var(self) -> tk.StringVar:
        return self._var


BUFF_SCOPE_OPTS   = ["Off", "Self only", "Full party"]
AUTO_LOOT_OPTS    = ["Use global", "Off", "On"]


class OocProfilePanel(ttk.LabelFrame):
    """Out-of-combat behaviour settings for a bot character (not per-profile)."""

    def __init__(self, parent, **kw):
        super().__init__(parent, text="Out-of-Combat Behaviour", **kw)
        self._char_guid: int | None = None
        self._build()

    def _build(self):
        nb = ttk.Notebook(self)
        nb.pack(fill=tk.BOTH, expand=True, padx=4, pady=(4, 0))

        self._build_buffs_tab(nb)
        self._build_loot_tab(nb)
        self._build_gather_tab(nb)

        ttk.Button(self, text="💾 Save Bot Behaviour",
                   command=self._save).pack(anchor="e", padx=6, pady=(2, 6))

    # ── Buffs ────────────────────────────────────────────────────────────────

    def _build_buffs_tab(self, nb):
        f = ttk.Frame(nb)
        nb.add(f, text="  Buffs  ")

        self.v_buff_scope        = tk.StringVar(value=BUFF_SCOPE_OPTS[2])
        self.v_buff_reapply_secs = tk.StringVar(value="30")
        self.v_buff_on_spawn     = tk.BooleanVar(value=True)
        self.v_follow_override   = tk.StringVar(value="")

        ttk.Label(f, text="Buff scope:").grid(row=0, column=0, sticky="e", padx=(8,2), pady=6)
        ttk.Combobox(f, textvariable=self.v_buff_scope,
                     values=BUFF_SCOPE_OPTS, state="readonly",
                     width=14).grid(row=0, column=1, sticky="w", pady=6)

        ttk.Label(f, text="Reapply when < ").grid(row=0, column=2, sticky="e", padx=(16,0))
        ttk.Entry(f, textvariable=self.v_buff_reapply_secs, width=5).grid(
            row=0, column=3, sticky="w")
        ttk.Label(f, text="sec remaining").grid(row=0, column=4, sticky="w", padx=(2,16))

        ttk.Checkbutton(f, text="Apply buffs on spawn",
                        variable=self.v_buff_on_spawn).grid(
            row=0, column=5, sticky="w", padx=8)

        ttk.Label(f, text="Follow distance override:").grid(
            row=1, column=0, sticky="e", padx=(8,2), pady=4)
        ttk.Entry(f, textvariable=self.v_follow_override, width=6).grid(
            row=1, column=1, sticky="w")
        ttk.Label(f, text="yards  (blank = use global role distance)",
                  foreground="#555").grid(row=1, column=2, columnspan=4, sticky="w", padx=4)

    # ── Looting ──────────────────────────────────────────────────────────────

    def _build_loot_tab(self, nb):
        f = ttk.Frame(nb)
        nb.add(f, text="  Looting  ")

        self.v_auto_loot = tk.StringVar(value=AUTO_LOOT_OPTS[0])

        ttk.Label(f, text="Auto-loot:").grid(row=0, column=0, sticky="e", padx=(8,2), pady=8)
        ttk.Combobox(f, textvariable=self.v_auto_loot,
                     values=AUTO_LOOT_OPTS, state="readonly",
                     width=14).grid(row=0, column=1, sticky="w")
        ttk.Label(f, text="Overrides global setting for this profile slot. "
                           "Gold is always collected.",
                  foreground="#555").grid(row=0, column=2, sticky="w", padx=8)

        ttk.Label(f, text="Min loot quality:").grid(
            row=1, column=0, sticky="e", padx=(8,2), pady=6)
        self._loot_quality_combo = _QualityCombo(f, width=22)
        self._loot_quality_combo.grid(row=1, column=1, sticky="w")
        ttk.Label(f, text="Skip items below this quality (exceptions below override this).",
                  foreground="#555").grid(row=1, column=2, sticky="w", padx=8)

        ttk.Separator(f, orient="horizontal").grid(
            row=2, column=0, columnspan=3, sticky="ew", padx=8, pady=(8, 4))

        ttk.Label(f, text="Always loot regardless of quality:",
                  font=("TkDefaultFont", 9, "bold")).grid(
            row=3, column=0, columnspan=3, sticky="w", padx=8, pady=(2, 4))

        # One checkbox per category — two per row
        self._cat_vars: dict[int, tk.BooleanVar] = {}
        for i, (bit, label) in enumerate(LOOT_CATEGORY_FLAGS):
            var = tk.BooleanVar(value=False)
            self._cat_vars[bit] = var
            row = 4 + i // 2
            col = (i % 2) * 2
            ttk.Checkbutton(f, text=label, variable=var).grid(
                row=row, column=col, columnspan=2, sticky="w",
                padx=(28 if col == 0 else 16, 4), pady=2)

    # ── Gathering ────────────────────────────────────────────────────────────

    def _build_gather_tab(self, nb):
        f = ttk.Frame(nb)
        nb.add(f, text="  Gathering  ")

        self.v_gather_nodes = tk.BooleanVar(value=False)
        self.v_gather_skin  = tk.BooleanVar(value=False)

        ttk.Checkbutton(f, text="Gather nearby nodes (herbs & ore)",
                        variable=self.v_gather_nodes).grid(
            row=0, column=0, columnspan=3, sticky="w", padx=8, pady=(10, 2))
        ttk.Label(f, text="Bot auto-detects mining/herbing professions and gathers while travelling.",
                  foreground="#555").grid(row=1, column=0, columnspan=3, sticky="w", padx=28, pady=(0, 8))

        ttk.Separator(f, orient="horizontal").grid(
            row=2, column=0, columnspan=3, sticky="ew", padx=8, pady=4)

        ttk.Checkbutton(f, text="Skin corpses",
                        variable=self.v_gather_skin).grid(
            row=3, column=0, columnspan=3, sticky="w", padx=8, pady=(8, 2))
        ttk.Label(f, text="Bot skins after each kill if it has the Skinning profession.",
                  foreground="#555").grid(row=4, column=0, columnspan=3, sticky="w", padx=28, pady=(0, 8))

        ttk.Label(f, text="Skin if all loot ≤:").grid(
            row=5, column=0, sticky="e", padx=(28, 2), pady=6)
        self._skin_quality_combo = _QualityCombo(f, width=22)
        self._skin_quality_combo.grid(row=5, column=1, sticky="w")
        ttk.Label(f, text="Skip skinning if any loot item exceeds this quality.",
                  foreground="#555").grid(row=5, column=2, sticky="w", padx=8)

    # ── Load / Save / Clear ──────────────────────────────────────────────────

    # ── Public API ────────────────────────────────────────────────────────────

    def load_for_char(self, char_guid: int):
        self._char_guid = char_guid
        self._load_dict(db.load_bot_ooc_config(char_guid))

    def _save(self):
        if self._char_guid is None:
            messagebox.showinfo("No character", "Select a bot character first.")
            return
        p: dict = {}
        self._collect_dict(p)
        try:
            db.save_bot_ooc_config(self._char_guid, p)
        except Exception as exc:
            messagebox.showerror("DB error", str(exc))
            return
        messagebox.showinfo("Saved",
            "Out-of-combat behaviour saved.\n"
            "Server picks up the change within 30 seconds.")

    def clear(self):
        self._char_guid = None
        self._load_dict({})

    # ── Internal helpers ──────────────────────────────────────────────────────

    def _load_dict(self, p: dict):
        self.v_buff_scope.set(BUFF_SCOPE_OPTS[p.get("buff_scope", 2)])
        self.v_buff_reapply_secs.set(str(p.get("buff_reapply_secs", 30)))
        self.v_buff_on_spawn.set(bool(p.get("buff_on_spawn", 1)))

        ov = p.get("follow_dist_override")
        self.v_follow_override.set(str(ov) if ov is not None else "")

        al = p.get("auto_loot_override")
        self.v_auto_loot.set(AUTO_LOOT_OPTS[0] if al is None
                             else AUTO_LOOT_OPTS[1 if not al else 2])

        self._loot_quality_combo.set_id(p.get("loot_quality_min", 0))

        # Default all category flags ON (0x3F) when no row exists yet
        flags = p.get("loot_category_flags", 0x3F)
        for bit, var in self._cat_vars.items():
            var.set(bool(flags & bit))

        self.v_gather_nodes.set(bool(p.get("gather_nodes", 0)))
        self.v_gather_skin.set(bool(p.get("gather_skin", 0)))
        self._skin_quality_combo.set_id(p.get("skin_loot_quality_max", 0))

    def _collect_dict(self, p: dict):
        p["buff_scope"] = BUFF_SCOPE_OPTS.index(self.v_buff_scope.get())
        try:
            p["buff_reapply_secs"] = int(self.v_buff_reapply_secs.get())
        except ValueError:
            p["buff_reapply_secs"] = 30
        p["buff_on_spawn"] = 1 if self.v_buff_on_spawn.get() else 0

        ov_txt = self.v_follow_override.get().strip()
        try:
            p["follow_dist_override"] = float(ov_txt) if ov_txt else None
        except ValueError:
            p["follow_dist_override"] = None

        al_idx = AUTO_LOOT_OPTS.index(self.v_auto_loot.get())
        p["auto_loot_override"]    = None if al_idx == 0 else (al_idx - 1)
        p["loot_quality_min"]      = self._loot_quality_combo.get_id()
        flags = 0
        for bit, var in self._cat_vars.items():
            if var.get():
                flags |= bit
        p["loot_category_flags"]   = flags
        p["gather_nodes"]          = 1 if self.v_gather_nodes.get() else 0
        p["gather_skin"]           = 1 if self.v_gather_skin.get() else 0
        p["skin_loot_quality_max"] = self._skin_quality_combo.get_id()
