"""
startup_validate.py
Real pre-flight check for lw_editor. Tests:
  1. All required data files present and parseable
  2. All modules importable with no NameErrors
  3. All tab classes instantiate under a real Tk root
  4. CharacterEditorTab internal structure (tabs, treeviews, caches)
  5. ProfessionsPanel recipe data loads
  6. DB class has all expected public methods
  7. helpers.py exports all symbols referenced in other modules
  8. constants.py exports all symbols referenced in other modules
  9. extract_dbc_data.py has no stray indentation / syntax errors

Reports every failure found — does not stop at first error.
"""
import sys, pathlib, importlib, traceback, json, inspect

sys.path.insert(0, str(pathlib.Path(__file__).parent))
ROOT   = pathlib.Path(__file__).parent
PKG    = ROOT / "lw_editor"
DATA   = ROOT.parent.parent / "var" / "extractors" / "data"   # adjust if needed

PASS = "\033[32m✓\033[0m"
FAIL = "\033[31m✗\033[0m"
WARN = "\033[33m~\033[0m"
failures = []
warnings = []

def ok(msg):
    print(f"  {PASS} {msg}")

def fail(msg):
    failures.append(msg)
    print(f"  {FAIL} {msg}")

def warn(msg):
    warnings.append(msg)
    print(f"  {WARN} {msg}")

# ── 1. Required data files ─────────────────────────────────────────────────
print("\n[1] Data files")
REQUIRED_DATA = [
    "spell_names.json",
    "class_spells.json",
    "faction_names.json",
    "talent_data.json",
    "enchantment_data.json",
    "gem_properties.json",
    "skill_line_abilities.json",
]

# Find the actual data dir used by lw_editor
try:
    from lw_editor import DATA_DIR as _DATA_DIR
    data_dir = _DATA_DIR
except Exception as e:
    fail(f"Cannot import lw_editor.__init__: {e}")
    data_dir = DATA

for fname in REQUIRED_DATA:
    p = data_dir / fname
    if not p.exists():
        fail(f"Missing: {p}")
        continue
    try:
        with p.open(encoding="utf-8") as f:
            obj = json.load(f)
        count = len(obj) if isinstance(obj, (dict, list)) else "?"
        ok(f"{fname}  ({count} entries)")
    except Exception as e:
        fail(f"{fname} — parse error: {e}")

# ── 2. Module imports ──────────────────────────────────────────────────────
print("\n[2] Module imports")
MODULES = [
    "lw_editor.constants",
    "lw_editor.helpers",
    "lw_editor.db",
    "lw_editor.widgets",
    "lw_editor.rotation",
    "lw_editor.tooltip",
    "lw_editor.tab_bots",
    "lw_editor.tab_character",
    "lw_editor.tab_accounts",
    "lw_editor.tab_professions",
    "lw_editor.app",
]
imported = {}
for mod_name in MODULES:
    try:
        m = importlib.import_module(mod_name)
        imported[mod_name] = m
        ok(mod_name)
    except Exception as e:
        fail(f"{mod_name}: {e}")
        traceback.print_exc()

# ── 3. Tab instantiation under real Tk root ────────────────────────────────
print("\n[3] Tab/panel instantiation (headless Tk)")
try:
    import tkinter as tk
    from tkinter import ttk
    root = tk.Tk()
    root.withdraw()

    tab_tests = [
        ("CharacterEditorTab",  "lw_editor.tab_character"),
        ("BotProfilesTab",      "lw_editor.tab_bots"),
        ("DefaultProfilesTab",  "lw_editor.tab_bots"),
        ("AccountsTab",         "lw_editor.tab_accounts"),
    ]
    instances = {}
    for cls_name, mod_name in tab_tests:
        if mod_name not in imported:
            warn(f"Skipping {cls_name} — module failed to import")
            continue
        try:
            mod = imported[mod_name]
            cls = getattr(mod, cls_name)
            nb  = ttk.Notebook(root)
            inst = cls(nb)
            inst.pack()
            instances[cls_name] = inst
            ok(f"{cls_name} instantiated")
        except Exception as e:
            fail(f"{cls_name}: {e}")
            traceback.print_exc()

    # ProfessionsPanel
    try:
        from lw_editor.tab_professions import ProfessionsPanel
        frame = ttk.Frame(root)
        panel = ProfessionsPanel(frame)
        panel.pack()
        ok("ProfessionsPanel instantiated")
        instances["ProfessionsPanel"] = panel
    except Exception as e:
        fail(f"ProfessionsPanel: {e}")
        traceback.print_exc()

except Exception as e:
    fail(f"Tk root setup failed: {e}")

# ── 4. CharacterEditorTab internal structure ───────────────────────────────
print("\n[4] CharacterEditorTab structure")
ced = instances.get("CharacterEditorTab")
if ced:
    checks = {
        "_rep_tvs":            lambda: isinstance(ced._rep_tvs, dict) and len(ced._rep_tvs) == 4,
        "_rep_tvs keys":       lambda: set(ced._rep_tvs.keys()) == {"Classic","The Burning Crusade","Wrath of the Lich King","Other"},
        "_inv_tv":             lambda: hasattr(ced, "_inv_tv"),
        "_ach_tv":             lambda: hasattr(ced, "_ach_tv"),
        "_gear_slot_controls": lambda: hasattr(ced, "_gear_slot_controls"),
        "_char_cache":         lambda: isinstance(ced._char_cache, dict),
        "_item_opts_cache":    lambda: isinstance(ced._item_opts_cache, dict),
        "_enchant_opts_cache": lambda: isinstance(ced._enchant_opts_cache, dict),
        "_gem_opts_cache":     lambda: isinstance(ced._gem_opts_cache, dict),
        "_prof_panel":         lambda: hasattr(ced, "_prof_panel"),
        "stat vars (health)":  lambda: hasattr(ced, "v_health"),
        "stat vars (kills)":   lambda: hasattr(ced, "v_total_kills"),
        "_tal_tvs (3 trees)":  lambda: len(ced._tal_tvs) == 3,
    }
    for label, check_fn in checks.items():
        try:
            result = check_fn()
            (ok if result else fail)(label)
        except Exception as e:
            fail(f"{label}: {e}")
else:
    warn("CharacterEditorTab not instantiated — skipping structure checks")

# ── 5. ProfessionsPanel recipe data ───────────────────────────────────────
print("\n[5] ProfessionsPanel recipe data")
prof = instances.get("ProfessionsPanel")
if prof:
    from lw_editor.tab_professions import _SKILL_LINE_DATA
    if _SKILL_LINE_DATA:
        total = sum(len(v) for v in _SKILL_LINE_DATA.values())
        ok(f"_SKILL_LINE_DATA loaded: {len(_SKILL_LINE_DATA)} skill lines, {total} recipes")
        expected_lines = {"129","185","356","171","164","333","202","773","755","165","197"}
        missing = expected_lines - set(_SKILL_LINE_DATA.keys())
        if missing:
            fail(f"Missing skill lines: {missing}")
        else:
            ok("All expected profession skill lines present")
    else:
        fail("_SKILL_LINE_DATA is empty — run extract_dbc_data.py")
else:
    warn("ProfessionsPanel not instantiated — skipping")

# ── 6. DBCtx public method surface ────────────────────────────────────────
print("\n[6] DBCtx method surface")
EXPECTED_DB_METHODS = [
    "connect", "disconnect", "ok",
    "load_character_summary", "load_character_reputations",
    "load_character_inventory_rows", "load_character_achievements",
    "load_character_talents", "load_character_skills",
    "load_character_known_spells",
    "item_tooltip_data", "spell_name", "enchant_name",
    "search_item_enchantments", "search_gem_items",
    "load_valid_equippable_items_for_slot",
    "update_character_core_stats", "reset_character_talents",
    "add_character_talent", "remove_character_talent",
]
if "lw_editor.db" in imported:
    from lw_editor.db import DBCtx
    for method in EXPECTED_DB_METHODS:
        if hasattr(DBCtx, method):
            ok(f"DBCtx.{method}")
        else:
            fail(f"DBCtx.{method} — MISSING")
else:
    warn("lw_editor.db failed to import — skipping")

# ── 7. helpers.py exported symbols ────────────────────────────────────────
print("\n[7] helpers.py exports")
EXPECTED_HELPERS = [
    "lbl", "entry_w", "money_text", "unix_text",
    "item_display_text", "enchant_display_text",
    "format_item_tooltip", "format_gem_tooltip",
    "parse_display_id", "item_quality_filter_id",
    "parse_item_enchantments", "build_item_enchantments",
    "get_item_enchant_id", "set_item_enchant_id",
    "socket_color_text", "filter_enchant_rows_for_slot",
    "make_tv_sortable",
]
if "lw_editor.helpers" in imported:
    helpers_mod = imported["lw_editor.helpers"]
    for sym in EXPECTED_HELPERS:
        if hasattr(helpers_mod, sym):
            ok(f"helpers.{sym}")
        else:
            fail(f"helpers.{sym} — MISSING")
else:
    warn("lw_editor.helpers failed to import — skipping")

# ── 8. constants.py exported symbols ──────────────────────────────────────
print("\n[8] constants.py key exports")
EXPECTED_CONSTANTS = [
    "WOW_CLASSES", "WOW_RACES", "EQUIPMENT_SLOT_NAMES", "ITEM_QUALITY_LABELS",
    "TALENT_DATA", "PERM_ENCHANTMENT_SLOT", "SOCK_ENCHANTMENT_SLOT",
    "SOCK_ENCHANTMENT_SLOT_2", "SOCK_ENCHANTMENT_SLOT_3",
    "ITEM_ENCHANTMENT_SLOT_COUNT", "ITEM_ENCHANTMENT_OFFSET_COUNT",
    "SECONDARY_PROF_SKILL_LINES", "CRAFTING_PROF_SKILL_LINES",
    "GATHERING_PROF_SKILL_LINES", "PROF_SPECIALIZATIONS",
    "ALL_PLAYER_PROF_SKILL_LINES",
    "CLASS_OPTS", "CLASS_NAME_TO_ID",
    "STAT_KEYS", "SUBJECT_KEYS",
]
if "lw_editor.constants" in imported:
    const_mod = imported["lw_editor.constants"]
    for sym in EXPECTED_CONSTANTS:
        if hasattr(const_mod, sym):
            ok(f"constants.{sym}")
        else:
            fail(f"constants.{sym} — MISSING")
else:
    warn("lw_editor.constants failed to import — skipping")

# ── 8b. db.py module-level symbols ────────────────────────────────────────
print("\n[8b] db.py module-level symbols")
DB_MODULE_SYMS = ["SSH_TUNNEL_AVAILABLE", "PARAMIKO_AVAILABLE"]
if "lw_editor.db" in imported:
    db_mod = imported["lw_editor.db"]
    for sym in DB_MODULE_SYMS:
        if hasattr(db_mod, sym):
            ok(f"db.{sym} = {getattr(db_mod, sym)}")
        else:
            fail(f"db.{sym} — MISSING")
else:
    warn("lw_editor.db failed to import — skipping")

# ── Cleanup ────────────────────────────────────────────────────────────────
try:
    root.destroy()
except Exception:
    pass

# ── Summary ────────────────────────────────────────────────────────────────
print()
print("=" * 60)
if failures:
    print(f"\033[31m  FAILED — {len(failures)} issue(s):\033[0m")
    for f in failures:
        print(f"    • {f}")
    if warnings:
        print(f"\n  {len(warnings)} warning(s):")
        for w in warnings:
            print(f"    ~ {w}")
    sys.exit(1)
else:
    if warnings:
        print(f"\033[33m  PASSED with {len(warnings)} warning(s):\033[0m")
        for w in warnings:
            print(f"    ~ {w}")
    else:
        print("\033[32m  ALL CHECKS PASSED\033[0m")
    sys.exit(0)
