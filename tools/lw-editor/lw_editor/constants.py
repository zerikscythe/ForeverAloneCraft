"""
constants.py — All application-wide constants, enum maps and lookup tables.
"""
import json
import pathlib

VERSION = "0.1"

# ── DB enum int → display string ───────────────────────────────────────────
CONSERVATION_MODES = {0: "Full Force", 1: "Reserve", 2: "Conservative", 3: "JIT Casting"}
ACTION_TYPES       = {0: "Spell",      1: "Item"}
RANK_MODES         = {0: "Best Known", 1: "Exact Spell ID", 2: "Specific Rank"}
COND_LOGIC         = {0: "All (AND)",  1: "Any (OR)"}
COND_OPS           = {0: "==", 1: "!=", 2: "<", 3: "<=", 4: ">", 5: ">=",
                      6: "Has", 7: "NotHas", 8: "Exists"}
AOE_MODES          = {0: "Centroid",   1: "Feet"}

def _inv(d): return {v: k for k, v in d.items()}

CONSERVATION_INV = _inv(CONSERVATION_MODES)
ACTION_INV       = _inv(ACTION_TYPES)
RANK_INV         = _inv(RANK_MODES)
COND_LOGIC_INV   = _inv(COND_LOGIC)
COND_OPS_INV     = _inv(COND_OPS)
AOE_INV          = _inv(AOE_MODES)

CONSERVATION_OPTS = list(CONSERVATION_MODES.values())
ACTION_OPTS       = list(ACTION_TYPES.values())
RANK_OPTS         = list(RANK_MODES.values())
COND_LOGIC_OPTS   = list(COND_LOGIC.values())
COND_OPS_OPTS     = list(COND_OPS.values())
AOE_OPTS          = list(AOE_MODES.values())

TARGET_KEYS   = [
    "enemy", "enemy_primary", "enemy_trash", "enemy_primary_victim",
    "self", "owner", "lowest_hp_party",
]
SUBJECT_KEYS  = [
    "enemy", "enemy_primary", "enemy_trash", "enemy_primary_victim",
    "self", "owner", "lowest_hp_party",
]
STAT_KEYS     = [
    "exists",
    "hp_pct", "mana_pct", "power", "power_pct",
    "runic_power", "runic_power_pct",
    "distance",
    "creature_type",
    "aura", "aura_remaining_secs", "aura_stacks",
    "combo_points",
    "threat_pct", "is_aggro_holder",
    "runes_ready", "runes_available",
    "nearby_enemies", "party_members_below_hp_pct",
]
BOOL_STAT_KEYS = {"is_aggro_holder"}
STRING_STAT_KEYS = {"creature_type"}
CREATURE_TYPE_OPTS = [
    "Beast",
    "Dragonkin",
    "Demon",
    "Elemental",
    "Giant",
    "Undead",
    "Humanoid",
    "Critter",
    "Mechanical",
    "NotSpecified",
    "Totem",
    "NonCombatPet",
    "GasCloud",
]

WOW_CLASSES = {
    1: "Warrior", 2: "Paladin", 3: "Hunter", 4: "Rogue",
    5: "Priest", 6: "Death Knight", 7: "Shaman", 8: "Mage",
    9: "Warlock", 11: "Druid",
}

# ── Talent tree data ────────────────────────────────────────────────────────
_TALENT_DATA_PATH = pathlib.Path(__file__).resolve().parent.parent / "data" / "talent_data.json"
TALENT_DATA: dict = {}
if _TALENT_DATA_PATH.exists():
    try:
        with _TALENT_DATA_PATH.open(encoding="utf-8") as _f:
            TALENT_DATA = json.load(_f)
    except Exception:
        pass

CLASS_OPTS = [name for _, name in sorted(WOW_CLASSES.items())]
CLASS_NAME_TO_ID = {name: cid for cid, name in WOW_CLASSES.items()}
CLASS_SPEC_OPTS = {
    1: ["Arms", "Fury", "Protection"],
    2: ["Holy", "Protection", "Retribution"],
    3: ["BeastMastery", "Marksmanship", "Survival"],
    4: ["Assassination", "Combat", "Subtlety"],
    5: ["Discipline", "Holy", "Shadow"],
    6: ["Blood", "Frost", "Unholy"],
    7: ["Elemental", "Enhancement", "Restoration"],
    8: ["Arcane", "Fire", "Frost"],
    9: ["Affliction", "Demonology", "Destruction"],
    11: ["Balance", "Feral", "Restoration"],
}
ROLE_OPTS = ["DPS", "HEAL", "TANK", "OFF_TANK"]

# ── Canonical default profile definitions ──────────────────────────────────
# One entry per class per talent tree per context.
# (class_name, spec_key, role_key, context_key, display_name)
# context_key is 'PvE' or 'PvP'; future values: 'Arena', 'BG', etc.
CANONICAL_DEFAULT_PROFILES: list[tuple[str, str, str, str, str]] = []
_SPECS: list[tuple[str, str, str]] = [
    ("Warrior",      "Arms",          "DPS"),
    ("Warrior",      "Fury",          "DPS"),
    ("Warrior",      "Protection",    "TANK"),
    ("Paladin",      "Holy",          "HEAL"),
    ("Paladin",      "Protection",    "TANK"),
    ("Paladin",      "Retribution",   "DPS"),
    ("Hunter",       "BeastMastery",  "DPS"),
    ("Hunter",       "Marksmanship",  "DPS"),
    ("Hunter",       "Survival",      "DPS"),
    ("Rogue",        "Assassination", "DPS"),
    ("Rogue",        "Combat",        "DPS"),
    ("Rogue",        "Subtlety",      "DPS"),
    ("Priest",       "Discipline",    "HEAL"),
    ("Priest",       "Holy",          "HEAL"),
    ("Priest",       "Shadow",        "DPS"),
    ("Death Knight", "Blood",         "TANK"),
    ("Death Knight", "Frost",         "DPS"),
    ("Death Knight", "Unholy",        "DPS"),
    ("Shaman",       "Elemental",     "DPS"),
    ("Shaman",       "Enhancement",   "DPS"),
    ("Shaman",       "Restoration",   "HEAL"),
    ("Mage",         "Arcane",        "DPS"),
    ("Mage",         "Fire",          "DPS"),
    ("Mage",         "Frost",         "DPS"),
    ("Warlock",      "Affliction",    "DPS"),
    ("Warlock",      "Demonology",    "DPS"),
    ("Warlock",      "Destruction",   "DPS"),
    ("Druid",        "Balance",       "DPS"),
    ("Druid",        "Feral",         "DPS"),
    ("Druid",        "Restoration",   "HEAL"),
]
_SPEC_DISPLAY = {
    "BeastMastery": "Beast Mastery",
}
PROFILE_CONTEXTS = ["PvE", "PvP"]
for _ctx in PROFILE_CONTEXTS:
    for _cls, _spec, _role in _SPECS:
        _display_spec = _SPEC_DISPLAY.get(_spec, _spec)
        _suffix = f" ({_ctx})" if _ctx != "PvE" else ""
        CANONICAL_DEFAULT_PROFILES.append(
            (_cls, _spec, _role, _ctx, f"{_cls} \u2014 {_display_spec}{_suffix}")
        )
del _SPECS, _SPEC_DISPLAY, _ctx, _cls, _spec, _role, _display_spec, _suffix

# Fast lookup: (class_name, spec_key, context_key) → (role_key, display_name)
CANONICAL_SPEC_LOOKUP: dict[tuple[str, str, str], tuple[str, str]] = {
    (cls, spec, ctx): (role, name)
    for cls, spec, role, ctx, name in CANONICAL_DEFAULT_PROFILES
}

WOW_RACES = {
    1: "Human", 2: "Orc", 3: "Dwarf", 4: "Night Elf", 5: "Undead",
    6: "Tauren", 7: "Gnome", 8: "Troll", 10: "Blood Elf", 11: "Draenei",
}

EQUIPMENT_SLOT_NAMES = {
    0: "Head",     1: "Neck",      2: "Shoulder", 3: "Shirt",
    4: "Chest",    5: "Waist",     6: "Legs",     7: "Feet",
    8: "Wrist",    9: "Hands",     10: "Finger 1",11: "Finger 2",
    12: "Trinket 1",13: "Trinket 2",14: "Back",
    15: "Main Hand",16: "Off Hand", 17: "Ranged",  18: "Tabard",
}

ITEM_ENCHANTMENT_SLOT_COUNT  = 12
ITEM_ENCHANTMENT_OFFSET_COUNT = 3
PERM_ENCHANTMENT_SLOT  = 0
SOCK_ENCHANTMENT_SLOT  = 2
SOCK_ENCHANTMENT_SLOT_2 = 3
SOCK_ENCHANTMENT_SLOT_3 = 4

EQUIPMENT_SLOT_TO_INVENTORY_TYPES = {
    0: [1],           1: [2],           2: [3],           3: [4],
    4: [5, 20],       5: [6],           6: [7],           7: [8],
    8: [9],           9: [10],          10: [11],         11: [11],
    12: [12],         13: [12],         14: [16],
    15: [13, 17, 21], 16: [14, 22, 23], 17: [15, 25, 26, 28], 18: [19],
}

SOCKET_COLOR_NAMES = {0: "None", 1: "Meta", 2: "Red", 4: "Yellow", 8: "Blue", 16: "Prismatic"}

ARMOR_PROFICIENCY_SLOT_IDS = {0, 2, 4, 5, 6, 7, 8, 9}
CLASS_ARMOR_SUBCLASSES = {
    1: {1, 2, 3, 4}, 2: {1, 2, 3, 4}, 3: {1, 2, 3}, 4: {1, 2},
    5: {1},          6: {1, 2, 3, 4}, 7: {1, 2, 3}, 8: {1},
    9: {1},          11: {1, 2},
}

ITEM_QUALITY_OPTIONS = [
    ("Any", None), ("Poor / Gray", 0), ("Common / White", 1),
    ("Uncommon / Green", 2), ("Rare / Blue", 3), ("Epic / Purple", 4),
    ("Legendary / Orange", 5), ("Artifact / Light Yellow", 6), ("Heirloom / Gold", 7),
]
ITEM_QUALITY_LABEL_TO_ID = {label: quality_id for label, quality_id in ITEM_QUALITY_OPTIONS}
ITEM_QUALITY_LABELS      = [label for label, _ in ITEM_QUALITY_OPTIONS]

ITEM_QUALITY_COLORS = {
    0: "Poor", 1: "Common", 2: "Uncommon", 3: "Rare",
    4: "Epic", 5: "Legendary", 6: "Artifact", 7: "Heirloom",
}

# ── Loot category exception flags ──────────────────────────────────────────
# Stored as a bitmask in living_world_bot_combat_profile.loot_category_flags.
# Items whose (item_class, item_subclass) fall into a flagged category are
# always looted regardless of loot_quality_min.
# Gold is always collected implicitly and has no flag.
#
# (bit, label, description, set of (item_class, item_subclass or None=any))
LOOT_CATEGORY_DEFS: list[tuple[int, str, str, set]] = [
    (0x01, "Cloth & Leather",
     "Bolts of cloth, hides, leather strips — Trade Goods class 7 sub 5/6",
     {(7, 5), (7, 6)}),
    (0x02, "Ore & Stone",
     "Raw ore and stone — Trade Goods class 7 sub 7",
     {(7, 7)}),
    (0x04, "Herbs",
     "Gathered herbs and plants — Trade Goods class 7 sub 1",
     {(7, 1)}),
    (0x08, "Enchanting Mats",
     "Dusts, essences, shards — Trade Goods class 7 sub 10",
     {(7, 10)}),
    (0x10, "Recipes & Formulas",
     "All recipe/formula/pattern items — Item class 9",
     {(9, None)}),
    (0x20, "Gems",
     "Cut and uncut gems — Item class 3",
     {(3, None)}),
]
# Flat list of (bit, label) for UI use
LOOT_CATEGORY_FLAGS: list[tuple[int, str]] = [
    (bit, label) for bit, label, _, __ in LOOT_CATEGORY_DEFS
]

ITEM_MOD_NAMES = {
    0: "Mana", 1: "Health", 3: "Agility", 4: "Strength",
    5: "Intellect", 6: "Spirit", 7: "Stamina",
    12: "Defense Rating", 13: "Dodge Rating", 14: "Parry Rating",
    15: "Block Rating", 31: "Hit Rating", 32: "Crit Rating",
    35: "Resilience Rating", 36: "Haste Rating", 37: "Expertise Rating",
    38: "Attack Power", 39: "Ranged Attack Power",
    43: "Mana per 5 sec", 44: "Armor Penetration Rating",
    45: "Spell Power", 46: "Health per 5 sec",
    47: "Spell Penetration", 48: "Block Value",
}

INVENTORY_TYPE_NAMES = {
    1: "Head", 2: "Neck", 3: "Shoulder", 4: "Body (Shirt)",
    5: "Chest", 6: "Waist", 7: "Legs", 8: "Feet",
    9: "Wrists", 10: "Hands", 11: "Finger", 12: "Trinket",
    13: "One-Hand", 14: "Off Hand (Shield)", 15: "Ranged",
    16: "Back", 17: "Two-Hand", 19: "Tabard", 20: "Chest (Robe)",
    21: "Main Hand", 22: "Off Hand", 23: "Held In Off-Hand",
    25: "Thrown", 26: "Ranged (Right)",
}

ITEM_TYPE_NAMES = {
    (2, 0): "One-Handed Axe",   (2, 1): "Two-Handed Axe",
    (2, 2): "Bow",              (2, 3): "Gun",
    (2, 4): "One-Handed Mace",  (2, 5): "Two-Handed Mace",
    (2, 6): "Polearm",          (2, 7): "One-Handed Sword",
    (2, 8): "Two-Handed Sword", (2, 10): "Staff",
    (2, 13): "Fist Weapon",     (2, 15): "Dagger",
    (2, 16): "Thrown",          (2, 18): "Crossbow",
    (2, 19): "Wand",
    (4, 0): "Misc Armor",  (4, 1): "Cloth",     (4, 2): "Leather",
    (4, 3): "Mail",        (4, 4): "Plate",      (4, 6): "Shield",
    (4, 11): "Ring",       (4, 12): "Necklace",  (4, 13): "Trinket",
    (4, 16): "Cloak",
}

SLOT_ENCHANT_CATEGORY = {
    0: "head", 2: "shoulder", 4: "chest",  5: "waist",
    6: "legs", 7: "feet",     8: "wrist",  9: "hands",
    14: "cloak", 15: "weapon", 16: "offhand", 17: "ranged",
}

SLOT_SPELL_NAME_KEYWORDS = {
    "head":     ["helmet", "head", "arcanum"],
    "shoulder": ["shoulder", "inscription"],
    "chest":    ["chest", "stats", "exceptional stats"],
    "waist":    ["belt", "waist"],
    "legs":     ["leg", "spellthread", "fur lining"],
    "feet":     ["boots", "feet"],
    "wrist":    ["bracer", "wrist"],
    "hands":    ["gloves", "hand", "gauntlet"],
    "cloak":    ["cloak", "back"],
    "weapon":   ["weapon", " sword", " mace", " axe", " staff", " blade",
                 "2h weapon", "1h weapon"],
    "offhand":  ["shield", "off-hand", "offhand"],
    "ranged":   ["scope", "ranged", "heartseeker", "sun scope", "sniper"],
}

ENCHANT_CATEGORY_KEYWORDS = {
    "head":     ["arcanum", "inscription"],
    "shoulder": ["inscription", "arcanum", "greater"],
    "chest":    ["stats", "spirit", "resilience", "stamina", "greater"],
    "waist":    [],
    "legs":     ["spellthread", "thread", "reinforced"],
    "feet":     [],
    "wrist":    ["assault", "stamina", "spirit", "stats", "intellect"],
    "hands":    [],
    "cloak":    ["cloak", "shadow armor", "speed", "wisdom", "mighty armor",
                 "subtlety", "spell piercing"],
    "weapon":   ["berserking", "mongoose", "executioner", "accuracy", "black magic",
                 "spellpower", "icebreaker", "lifeward", "potency", "agility",
                 "intellect", "spellsurge", "slayer"],
    "offhand":  ["shield", "defense", "intellect", "resilience", "stamina", "plating"],
    "ranged":   ["scope", "heartseeker", "sun scope", "diamond-cut", "sniper"],
}

CLASS_SPELL_FAMILY = {
    1: 2, 2: 8, 3: 7, 4: 6, 5: 4, 6: 10, 7: 9, 8: 1, 9: 3, 11: 5,
}

SPEC_TO_CLASS = {name.lower(): cid for cid, name in WOW_CLASSES.items()}
SPEC_ALIAS_TO_CLASS = {
    "arms": 1, "fury": 1, "protection warrior": 1,
    "holy paladin": 2, "protection paladin": 2, "retribution": 2,
    "beastmastery": 3, "beast mastery": 3, "marksmanship": 3, "survival": 3,
    "assassination": 4, "combat": 4, "subtlety": 4,
    "discipline": 5, "holy priest": 5, "shadow": 5,
    "blood": 6, "frost death knight": 6, "unholy": 6,
    "elemental": 7, "enhancement": 7, "restoration shaman": 7,
    "arcane": 8, "fire": 8, "frost mage": 8,
    "affliction": 9, "demonology": 9, "destruction": 9,
    "balance": 11, "feral": 11, "restoration druid": 11,
}

ROLE_ALIASES = {
    "heal": "HEAL", "healer": "HEAL", "heals": "HEAL",
    "tank": "TANK", "off-tank": "OFF_TANK", "off_tank": "OFF_TANK",
    "offtank": "OFF_TANK", "dps": "DPS",
}

def _normalize_role(raw: str) -> str:
    """Normalise any role string to one of DPS / HEAL / TANK / OFF_TANK."""
    if not raw:
        return "DPS"
    cleaned = raw.strip().upper()
    if cleaned in ("DPS", "HEAL", "TANK", "OFF_TANK"):
        return cleaned
    return ROLE_ALIASES.get(raw.strip().lower(), "DPS")

SRP6_G = 7
SRP6_N = int("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", 16)

# ── Professions ────────────────────────────────────────────────────────────
# Secondary professions — always shown in the professions panel
SECONDARY_PROF_SKILL_LINES: dict[int, str] = {
    129: "First Aid",
    185: "Cooking",
    356: "Fishing",
}

# Primary crafting professions — up to 2 per character
CRAFTING_PROF_SKILL_LINES: dict[int, str] = {
    171: "Alchemy",
    164: "Blacksmithing",
    333: "Enchanting",
    202: "Engineering",
    773: "Inscription",
    755: "Jewelcrafting",
    165: "Leatherworking",
    197: "Tailoring",
}

# Gathering professions — skill level shown in Summary, no recipe list
GATHERING_PROF_SKILL_LINES: dict[int, str] = {
    182: "Herbalism",
    186: "Mining",
    393: "Skinning",
}

# Sub-skill lines that belong to a parent crafting profession
PROF_SPECIALIZATIONS: dict[int, dict[int, str]] = {
    202: {203: "Gnomish Engineering",   204: "Goblin Engineering"},
    165: {751: "Tribal LW",             752: "Elemental LW",      753: "Dragonscale LW"},
    164: {220: "Armorsmith",            221: "Weaponsmith"},
    197: {904: "Mooncloth Tailoring",   905: "Spellfire Tailoring", 906: "Shadoweave Tailoring"},
}

# Quick lookup: every profession skill line (secondary + crafting)
ALL_PLAYER_PROF_SKILL_LINES: dict[int, str] = {
    **SECONDARY_PROF_SKILL_LINES,
    **CRAFTING_PROF_SKILL_LINES,
}

# Profession max skill by expansion tier (used to label Apprentice/Expert/etc.)
PROF_MAX_SKILL = 450   # WotLK cap
