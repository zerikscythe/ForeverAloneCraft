#!/usr/bin/env python3
"""
DBC to CSV Extractor
────────────────────────────────────────────────────────────────────────────
Extracts spell and class data from WoW DBC files into lightweight CSV/JSON
files that the LivingWorld Bot Editor can use for spell lookups.

This only needs to be run once after extracting DBC files from the WoW client.

Usage:
    python extract_dbc_data.py

Requirements:
    - DBC files in ../../var/extractors/dbc/
    - Python 3.10+

Output:
    - spell_names.json - Spell ID to name mapping
    - class_spells.json - Class to spell list mapping
    - spell_ranks.csv - Spell rank chains
"""

import json
import csv
import struct
import pathlib
import sys
from collections import defaultdict

# Paths
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
DBC_DIR    = SCRIPT_DIR.parent.parent / "var" / "extractors" / "dbc"
OUTPUT_DIR = SCRIPT_DIR / "data"

def read_dbc_file(file_name: str, expected_fields: int = None):
    """Read a DBC file and return parsed data."""
    path = DBC_DIR / file_name
    if not path.exists():
        print(f"❌ {file_name} not found in {DBC_DIR}")
        return None

    print(f"📖 Reading {file_name}...")

    try:
        with path.open("rb") as f:
            magic, record_count, field_count, record_size, string_size = struct.unpack(
                "<4s4I", f.read(20))

            if magic != b"WDBC":
                print(f"   ❌ Not a valid DBC file (bad magic)")
                return None

            if expected_fields and field_count != expected_fields:
                print(f"   ⚠️  Field count mismatch: {field_count} != {expected_fields}")

            records = f.read(record_count * record_size)
            string_block = f.read(string_size)

        print(f"   ✅ Loaded {record_count:,} records ({field_count} fields each)")
        return record_count, field_count, record_size, records, string_block

    except Exception as e:
        print(f"   ❌ Error reading file: {e}")
        return None

def get_string(string_block: bytes, offset: int) -> str:
    """Extract a null-terminated string from the string block."""
    if not offset:
        return ""
    end = string_block.find(b"\x00", offset)
    if end == -1:
        end = len(string_block)
    return string_block[offset:end].decode("utf-8", errors="ignore")

def extract_spell_names():
    """Extract spell ID to name mapping from Spell.dbc."""
    print("\n🔮 Extracting spell names...")

    data = read_dbc_file("Spell.dbc", expected_fields=234)
    if not data:
        return {}

    record_count, field_count, record_size, records, strings = data
    spell_names = {}
    name_idx = 136  # Name field index in Spell.dbc

    for i in range(record_count):
        row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        spell_id = row[0]
        name = get_string(strings, row[name_idx]).strip()

        if name and not is_noise_spell(name):
            spell_names[spell_id] = name

    print(f"   ✅ Extracted {len(spell_names):,} spell names")
    return spell_names

def extract_class_spells():
    """Extract class to spell mapping from SkillLineAbility.dbc."""
    print("\n🎯 Extracting class spell associations...")

    # Load SkillLineAbility.dbc
    data = read_dbc_file("SkillLineAbility.dbc", expected_fields=14)
    if not data:
        return {}

    record_count, field_count, record_size, records, _ = data

    # Load SkillLine.dbc to identify class skills
    skillline_data = read_dbc_file("SkillLine.dbc")
    if not skillline_data:
        return {}

    sl_count, sl_fields, sl_size, sl_records, _ = skillline_data
    class_skilllines = set()

    for i in range(sl_count):
        row = struct.unpack_from(f"<{sl_fields}I", sl_records, i * sl_size)
        if row[1] == 7:  # SKILL_CATEGORY_CLASS
            class_skilllines.add(row[0])

    print(f"   ℹ️  Found {len(class_skilllines)} class skill lines")

    # Build class to spell mapping
    class_spells = defaultdict(set)

    for i in range(record_count):
        row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        skillline = row[1]
        spell_id = row[2]
        classmask = row[4]
        excludeclass = row[6]

        # Only process class skills
        if skillline not in class_skilllines:
            continue

        # Check each class (1-11, excluding 10)
        for class_id in [1, 2, 3, 4, 5, 6, 7, 8, 9, 11]:
            mask = 1 << (class_id - 1)

            # Spell is for this class if mask matches and not excluded
            if (classmask & mask) and not (excludeclass & mask):
                class_spells[class_id].add(spell_id)

    # Convert sets to sorted lists
    class_spells = {k: sorted(v) for k, v in class_spells.items()}

    total_spells = sum(len(v) for v in class_spells.values())
    print(f"   ✅ Mapped {total_spells:,} spell associations across {len(class_spells)} classes")

    for class_id in sorted(class_spells.keys()):
        class_name = CLASS_NAMES.get(class_id, f"Class{class_id}")
        print(f"      {class_name}: {len(class_spells[class_id]):,} spells")

    return class_spells

def extract_spell_ranks():
    """Extract spell rank information (if available)."""
    print("\n📊 Extracting spell ranks...")

    # Note: spell_ranks is usually in the database, not DBC
    # We'll create a placeholder for now
    print("   ℹ️  Spell ranks are typically in database (spell_ranks table)")
    print("   ℹ️  Skipping DBC extraction for ranks")
    return []

def is_noise_spell(name: str) -> bool:
    """Filter out test/debug spells."""
    if not name:
        return True
    lowered = name.lower()
    return (
        lowered.startswith("zzold") or
        lowered.startswith("deprecated") or
        lowered.startswith("test") or
        lowered.endswith("(dnd)") or
        "test" in lowered or
        "deprecated" in lowered or
        "old" in lowered and "old god" not in lowered
    )

CLASS_NAMES = {
    1: "Warrior",
    2: "Paladin",
    3: "Hunter",
    4: "Rogue",
    5: "Priest",
    6: "Death Knight",
    7: "Shaman",
    8: "Mage",
    9: "Warlock",
    11: "Druid",
}

def build_enchant_spell_map(spell_names: dict) -> dict[int, str]:
    """
    Scan Spell.dbc for spells whose effects apply an item enchantment and
    return a mapping of {SpellItemEnchantment_ID -> spell_name}.

    Relevant Spell.dbc binary field indices (each field = 4 bytes):
      Effect[0,1,2]        = fields 71, 72, 73
      EffectMiscValue[0,1,2] = fields 110, 111, 112
      SpellName[0] (enUS)  = field 136  (string offset)

    SPELL_EFFECT_ENCHANT_ITEM            = 53
    SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY  = 54
    SPELL_EFFECT_ENCHANT_HELD_ITEM       = 156 (rare, handled too)
    """
    ENCHANT_EFFECTS = {53, 54, 156}
    EFFECT_FIELDS     = [71, 72, 73]
    MISC_VALUE_FIELDS = [110, 111, 112]
    NAME_FIELD        = 136

    parsed = read_dbc_file("Spell.dbc", expected_fields=234)
    if not parsed:
        return {}

    record_count, field_count, record_size, records, string_block = parsed

    result: dict[int, str] = {}

    for i in range(record_count):
        base   = i * record_size
        fields = struct.unpack_from(f"<{field_count}I", records, base)

        # Check each of the three effect slots.
        for slot in range(3):
            effect_type = fields[EFFECT_FIELDS[slot]]
            if effect_type not in ENCHANT_EFFECTS:
                continue

            enchant_id = fields[MISC_VALUE_FIELDS[slot]]
            if not enchant_id:
                continue

            # Only store if we have a spell name and haven't already seen a
            # better (non-empty) name for this enchant ID.
            if enchant_id in result:
                continue

            name_off = fields[NAME_FIELD]
            name = ""
            if name_off and name_off < len(string_block):
                end  = string_block.index(b'\x00', name_off)
                name = string_block[name_off:end].decode("utf-8", errors="replace").strip()

            if name:
                result[enchant_id] = name

    return result


def extract_faction_names() -> dict | None:
    """
    Extract faction data from Faction.dbc including parent hierarchy.

    Field layout (57 uint32 fields):
      [0]     ID
      [1]     reputationListID  (0xFFFFFFFF = header / no player rep)
      [2-5]   BaseRepRaceMask[4]
      [6-9]   BaseRepClassMask[4]
      [10-13] BaseRepValue[4]   (int32)
      [14-17] ReputationFlags[4]
      [18]    parentFactionID
      [19]    spilloverRateIn
      [20]    spilloverRateOut
      [21]    spilloverMaxRankIn
      [22]    parentFactionCap
      [23]    name enUS string offset
      [24-38] name other locales
      [39]    nameFlags
      [40-55] description[16]
      [56]    descriptionFlags

    Expansion root faction IDs (WotLK 3.3.5a):
      1118 = Classic
       980 = The Burning Crusade
      1097 = Wrath of the Lich King
    """
    EXPANSION_ROOTS = {
        1118: "Classic",
        980:  "The Burning Crusade",
        1097: "Wrath of the Lich King",
    }
    # Faction names that are professions / internal and not player rep groups
    INTERNAL_KEYWORDS = (
        'Blacksmithing -', 'Leatherworking -', 'Engineering -',
        'Test Faction', 'Conversion', 'UNUSED', 'REUSE', 'DND', 'DNR',
    )
    IS_HDR = 0xFFFFFFFF

    print("\n🏳️  Extracting faction names...")
    parsed = read_dbc_file("Faction.dbc", expected_fields=57)
    if not parsed:
        print("   ⚠️  Faction.dbc not found — skipping.")
        return None

    record_count, field_count, record_size, records, string_block = parsed

    # First pass — build full map
    raw: dict[int, dict] = {}
    for i in range(record_count):
        fields    = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        faction_id = int(fields[0])
        if not faction_id:
            continue
        rep_idx   = int(fields[1])
        parent_id = int(fields[18])
        name_off  = int(fields[23])
        name = ""
        if name_off and name_off < len(string_block):
            end  = string_block.index(b'\x00', name_off)
            name = string_block[name_off:end].decode("utf-8", errors="replace").strip()
        raw[faction_id] = {"name": name, "parent_id": parent_id, "rep_idx": rep_idx}

    def get_root(fid: int, depth: int = 0) -> int:
        d = raw.get(fid)
        if d is None or d["parent_id"] == 0 or depth > 8:
            return fid
        return get_root(d["parent_id"], depth + 1)

    # Second pass — build output with expansion group tag
    result: dict[int, dict] = {}
    for faction_id, d in raw.items():
        name = d["name"]
        if not name:
            continue
        if d["rep_idx"] == IS_HDR:
            continue
        if any(kw in name for kw in INTERNAL_KEYWORDS):
            continue

        root_id    = get_root(faction_id)
        expansion  = EXPANSION_ROOTS.get(root_id, "Other")
        parent_id  = d["parent_id"]
        parent_name = raw.get(parent_id, {}).get("name", "") if parent_id else ""

        result[faction_id] = {
            "name":        name,
            "parent_id":   parent_id,
            "parent_name": parent_name,
            "expansion":   expansion,
        }

    print(f"   ✅ Extracted {len(result):,} faction entries")
    by_xpac = {}
    for d in result.values():
        by_xpac[d["expansion"]] = by_xpac.get(d["expansion"], 0) + 1
    for xpac, count in sorted(by_xpac.items()):
        print(f"      {xpac:<28} {count}")
    return result


def extract_enchantment_data() -> list | None:
    """
    Extract item enchantment data from SpellItemEnchantment.dbc.

    Field layout (38 uint32 fields):
      [0]    ID
      [1-3]  Effect[3]          (SpellItemEnchantmentEffect enum)
      [4-6]  EffectPointsMin[3]
      [7-9]  EffectPointsMax[3]
      [10-12] EffectArg[3]      (stat type for Effect=5, spell ID for Effect=1/3)
      [13]   unused
      [14]   Name_Lang_enUS (string offset)
      ...
      [37]   MinLevel
    """
    # SpellItemEnchantmentEffect enum
    EFFECT_NONE   = 0
    EFFECT_PROC   = 1   # procs a spell on hit
    EFFECT_DAMAGE = 2   # flat weapon damage
    EFFECT_EQUIP  = 3   # casts a spell on equip
    EFFECT_RESIST = 4   # resistance bonus
    EFFECT_STAT   = 5   # direct stat bonus
    EFFECT_TOTEM  = 6
    EFFECT_USE    = 7

    # ItemMod enum → display label (for EFFECT_STAT)
    ITEM_MOD = {
        0: "Mana", 1: "Health", 3: "Agility", 4: "Strength",
        5: "Intellect", 6: "Spirit", 7: "Stamina",
        12: "Defense Rating", 13: "Dodge Rating", 14: "Parry Rating",
        15: "Block Rating", 16: "Hit Melee Rating", 17: "Hit Ranged Rating",
        18: "Hit Spell Rating", 19: "Crit Melee Rating", 20: "Crit Ranged Rating",
        21: "Crit Spell Rating", 28: "Haste Melee Rating", 29: "Haste Ranged Rating",
        30: "Haste Spell Rating", 31: "Hit Rating", 32: "Crit Rating",
        35: "Resilience Rating", 36: "Haste Rating", 37: "Expertise Rating",
        38: "Attack Power", 39: "Ranged Attack Power",
        43: "Mana per 5 sec", 44: "Armor Penetration Rating",
        45: "Spell Power", 46: "Health per 5 sec",
        47: "Spell Penetration", 48: "Block Value",
    }

    print("\n✨ Extracting enchantment data...")

    parsed = read_dbc_file("SpellItemEnchantment.dbc", expected_fields=38)
    if not parsed:
        print("   ⚠️  SpellItemEnchantment.dbc not found — skipping.")
        return None

    # Pre-load spell names to resolve proc/equip spell IDs
    spell_parsed = read_dbc_file("Spell.dbc", expected_fields=234)
    spell_names_map: dict[int, str] = {}
    if spell_parsed:
        sc, sf, ss, srecs, sstr = spell_parsed
        for i in range(sc):
            sf2 = struct.unpack_from(f"<{sf}I", srecs, i * ss)
            off = sf2[136]
            if off and off < len(sstr):
                end = sstr.index(b'\x00', off)
                name = sstr[off:end].decode("utf-8", errors="replace").strip()
                if name:
                    spell_names_map[sf2[0]] = name

    record_count, field_count, record_size, records, string_block = parsed
    result = []
    for i in range(record_count):
        fields     = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        enchant_id = int(fields[0])
        name_off   = int(fields[14])
        min_level  = int(fields[37])
        if not enchant_id:
            continue

        name = ""
        if name_off and name_off < len(string_block):
            end  = string_block.index(b'\x00', name_off)
            name = string_block[name_off:end].decode("utf-8", errors="replace").strip()
        if not name:
            continue

        # Decode effect lines
        effect_lines = []
        for slot in range(3):
            etype = int(fields[1 + slot])
            emin  = int(fields[4 + slot])
            earg  = int(fields[10 + slot])
            if etype == EFFECT_NONE:
                continue
            if etype == EFFECT_STAT:
                stat_name = ITEM_MOD.get(earg, f"Stat({earg})")
                effect_lines.append(f"+{emin} {stat_name}")
            elif etype in (EFFECT_PROC, EFFECT_EQUIP, EFFECT_USE):
                prefix = {EFFECT_PROC: "Proc", EFFECT_EQUIP: "Equip", EFFECT_USE: "Use"}[etype]
                spell_name = spell_names_map.get(earg, f"Spell {earg}" if earg else "")
                if spell_name:
                    effect_lines.append(f"{prefix}: {spell_name}")
            elif etype == EFFECT_DAMAGE:
                effect_lines.append(f"+{emin} Weapon Damage")
            elif etype == EFFECT_RESIST:
                effect_lines.append(f"+{emin} Resistance")

        result.append({
            "ID":             enchant_id,
            "Name_Lang_enUS": name,
            "Effects":        effect_lines,
            "MinLevel":       min_level,
        })

    # Attach enchanting spell names
    print("   🔗 Mapping enchantment IDs to enchanting spell names...")
    enchant_spell_map = build_enchant_spell_map({})
    linked = 0
    for entry in result:
        spell_name = enchant_spell_map.get(entry["ID"], "")
        entry["SpellName"] = spell_name
        if spell_name:
            linked += 1
    print(f"   ✅ Linked {linked}/{len(result)} enchantments to their source spell")
    print(f"   ✅ Extracted {len(result):,} enchantments")
    return result


def extract_gem_properties() -> dict | None:
    """
    Extract gem property data from GemProperties.dbc.

    Field layout (5 uint32 fields):
      [0] ID
      [1] Enchant_Id   (SpellItemEnchantment ID applied when socketed)
      [2] MaxCountInv  (unused here)
      [3] MaxCountItem (unused here)
      [4] Type         (socket colour bitmask: 1=Meta 2=Red 4=Yellow 8=Blue)
    """
    print("\n💎 Extracting gem property data...")

    parsed = read_dbc_file("GemProperties.dbc", expected_fields=5)
    if not parsed:
        print("   ⚠️  GemProperties.dbc not found — skipping.")
        return None

    record_count, field_count, record_size, records, _ = parsed
    result = {}
    for i in range(record_count):
        fields = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        gemprop_id = int(fields[0])
        enchant_id = int(fields[1])
        socket_mask = int(fields[4])
        if gemprop_id:
            result[gemprop_id] = {
                "ID":         gemprop_id,
                "Enchant_Id": enchant_id,
                "Type":       socket_mask,
            }

    print(f"   ✅ Extracted {len(result):,} gem properties")
    return result


def extract_talent_data(spell_names: dict) -> dict | None:
    """
    Extract talent tree data from Talent.dbc and TalentTab.dbc.

    TalentTab.dbc field layout (24 uint32 fields):
      [0]    TalentTabID
      [1-16] Name string offsets (16 locales)
      [17]   nameFlags
      [18]   spellicon
      [19]   unused
      [20]   ClassMask
      [21]   petTalentMask
      [22]   tabpage  (0/1/2 = tree order within the class)
      [23]   internalname string offset

    Talent.dbc field layout (23 uint32 fields):
      [0]   TalentID
      [1]   TalentTab  (TalentTabID)
      [2]   Row
      [3]   Col
      [4-8] RankID[5]  (spell IDs for each rank; 0 = unused rank)
      [9-12] unused (4 fields)
      [13]  DependsOn  (TalentID of prerequisite talent, 0 = none)
      [14-15] unused
      [16]  DependsOnRank
      [17-18] unused
      [19]  addToSpellBook
      [20]  requiredSpellID
      [21-22] categoryMask[2]
    """
    print("\n🌟 Extracting talent data...")

    tab_parsed = read_dbc_file("TalentTab.dbc", expected_fields=24)
    if not tab_parsed:
        print("   ⚠️  TalentTab.dbc not found — skipping talent extraction.")
        return None

    tal_parsed = read_dbc_file("Talent.dbc", expected_fields=23)
    if not tal_parsed:
        print("   ⚠️  Talent.dbc not found — skipping talent extraction.")
        return None

    # --- Parse TalentTab.dbc ---
    tab_count, tab_fields, tab_rec_size, tab_records, tab_strings = tab_parsed
    tabs: dict[int, dict] = {}  # TalentTabID → {class_mask, tabpage, name}

    for i in range(tab_count):
        offset = i * tab_rec_size
        fields = struct.unpack_from(f"<{tab_fields}I", tab_records, offset)

        tab_id     = fields[0]
        # fields[1] is the English name string offset
        name_off   = fields[1]
        class_mask = fields[20]
        tabpage    = fields[22]

        name = ""
        if name_off < len(tab_strings):
            end = tab_strings.index(b'\x00', name_off)
            name = tab_strings[name_off:end].decode("utf-8", errors="replace")

        tabs[tab_id] = {
            "class_mask": class_mask,
            "tabpage":    tabpage,
            "name":       name,
        }

    # Build ClassMask → (tabpage → tab_id) so we can name trees per class.
    # ClassMask is a bitmask where bit (classId-1) is set.
    CLASS_IDS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 11]  # no class 10
    class_trees: dict[int, dict[int, int]] = {}   # class_id → {tabpage: tab_id}
    class_tree_names: dict[int, dict[int, str]] = {}

    for tab_id, tab in tabs.items():
        if tab["class_mask"] == 0:
            continue  # pet talents
        for cls in CLASS_IDS:
            if tab["class_mask"] & (1 << (cls - 1)):
                class_trees.setdefault(cls, {})[tab["tabpage"]] = tab_id
                class_tree_names.setdefault(cls, {})[tab["tabpage"]] = tab["name"]

    # --- Parse Talent.dbc ---
    tal_count, tal_fields, tal_rec_size, tal_records, _ = tal_parsed

    # Output structure:
    #   { "class_id": { "tree_index": { "tree_name", "talents": [...] } } }
    result: dict[str, dict] = {}

    for i in range(tal_count):
        offset = i * tal_rec_size
        fields = struct.unpack_from(f"<{tal_fields}I", tal_records, offset)

        talent_id  = fields[0]
        tab_id     = fields[1]
        row        = fields[2]
        col        = fields[3]
        rank_ids   = [fields[4 + r] for r in range(5)]
        depends_on = fields[13]
        depends_on_rank = fields[16]

        if tab_id not in tabs:
            continue

        tab      = tabs[tab_id]
        max_rank = sum(1 for r in rank_ids if r != 0)
        if max_rank == 0:
            continue

        # Talent name comes from its first-rank spell.
        name = spell_names.get(rank_ids[0], f"Talent {talent_id}")

        # Which class owns this talent?
        owning_class = None
        for cls in CLASS_IDS:
            if tab["class_mask"] & (1 << (cls - 1)):
                owning_class = cls
                break

        if owning_class is None:
            continue

        tabpage = tab["tabpage"]
        cls_key = str(owning_class)

        result.setdefault(cls_key, {})
        result[cls_key].setdefault(str(tabpage), {
            "tree_name": class_tree_names.get(owning_class, {}).get(tabpage, f"Tree {tabpage}"),
            "talents":   [],
        })

        result[cls_key][str(tabpage)]["talents"].append({
            "talent_id":       talent_id,
            "name":            name,
            "row":             row,
            "col":             col,
            "max_rank":        max_rank,
            "rank_spell_ids":  [r for r in rank_ids if r != 0],
            "depends_on":      depends_on if depends_on != 0 else None,
            "depends_on_rank": depends_on_rank,
        })

    # Sort talents within each tree by row then col for deterministic output.
    for cls_data in result.values():
        for tree in cls_data.values():
            tree["talents"].sort(key=lambda t: (t["row"], t["col"]))

    total = sum(
        len(tree["talents"])
        for cls_data in result.values()
        for tree in cls_data.values()
    )
    print(f"   ✅ Extracted {total} talents across {len(result)} classes")
    return result


def extract_skill_line_abilities(spell_names: dict) -> dict | None:
    """
    Extract SkillLineAbility.dbc and group entries by profession skill line.

    Field layout (14 uint32 fields):
      [0]  ID
      [1]  SkillLine       — skill line ID (profession)
      [2]  Spell           — spell ID of the recipe/ability
      [3]  RaceMask
      [4]  ClassMask
      [5]  ExcludeRaceMask
      [6]  ExcludeClassMask
      [7]  MinSkillLineRank — skill required to learn
      [8]  SupercededBy    — 0 or spell ID this replaces (old rank)
      [9]  AcquireMethod
      [10] NumSkillUps
      [11] UniqueBit
      [12] TradeSkillCategoryID
      [13] SkillupSkillLineID

    Only extracts skill lines relevant to player professions.
    """
    # Primary crafting + secondaries + specialisation sub-lines
    WANTED_SKILL_LINES = {
        # Secondary professions
        129: "First Aid",
        185: "Cooking",
        356: "Fishing",
        # Primary crafting professions
        171: "Alchemy",
        164: "Blacksmithing",
        333: "Enchanting",
        202: "Engineering",
        773: "Inscription",
        755: "Jewelcrafting",
        165: "Leatherworking",
        197: "Tailoring",
        # Engineering specialisations
        203: "Gnomish Engineering",
        204: "Goblin Engineering",
        # Leatherworking specialisations
        751: "Tribal Leatherworking",
        752: "Elemental Leatherworking",
        753: "Dragonscale Leatherworking",
        # Blacksmithing specialisations
        220: "Armorsmith",
        221: "Weaponsmith",
        # Tailoring specialisations
        904: "Mooncloth Tailoring",
        905: "Spellfire Tailoring",
        906: "Shadoweave Tailoring",
    }

    print("\n🔧 Extracting skill line ability data...")
    parsed = read_dbc_file("SkillLineAbility.dbc", expected_fields=14)
    if not parsed:
        print("   ⚠️  SkillLineAbility.dbc not found — skipping.")
        return None

    record_count, field_count, record_size, records, _ = parsed

    # Build a set of ALL spell IDs per skill line first so we can identify
    # superseded spells (old ranks) and exclude them from the output.
    skill_all_spells: dict[int, set] = {}
    skill_superseded: dict[int, set] = {}  # spells that are replaced by something else
    for i in range(record_count):
        f = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        sl = f[1]
        if sl not in WANTED_SKILL_LINES:
            continue
        spell_id = f[2]
        superseded_by = f[8]
        if sl not in skill_all_spells:
            skill_all_spells[sl] = set()
            skill_superseded[sl] = set()
        if spell_id:
            skill_all_spells[sl].add(spell_id)
        if superseded_by:
            skill_superseded[sl].add(f[2])  # the OLD spell is the one being superseded

    result: dict[str, list] = {}
    seen: set[tuple] = set()  # (skill_line, spell_id) dedup

    for i in range(record_count):
        f = struct.unpack_from(f"<{field_count}I", records, i * record_size)
        skill_line = f[1]
        if skill_line not in WANTED_SKILL_LINES:
            continue
        spell_id      = int(f[2])
        min_skill     = int(f[7])
        superseded_by = int(f[8])
        if not spell_id:
            continue
        key = (skill_line, spell_id)
        if key in seen:
            continue
        seen.add(key)

        spell_name = spell_names.get(spell_id, "").strip()
        if not spell_name:
            continue

        # Skip pure rank-up "Train <Profession>" spells that auto-upgrade.
        # These are spells superseded by another spell in the same skill line.
        if spell_id in skill_superseded.get(skill_line, set()):
            continue

        sl_key = str(skill_line)
        if sl_key not in result:
            result[sl_key] = []
        result[sl_key].append({
            "spell_id":      spell_id,
            "spell_name":    spell_name,
            "min_skill":     min_skill,
            "superseded_by": superseded_by,
        })

    # Sort each skill line's entries by min_skill then spell_name
    for sl_key in result:
        result[sl_key].sort(key=lambda r: (r["min_skill"], r["spell_name"]))

    total = sum(len(v) for v in result.values())
    found_lines = sorted(int(k) for k in result)
    print(f"   ✅ Extracted {total:,} recipes across {len(found_lines)} skill lines")
    for sl in found_lines:
        print(f"      {WANTED_SKILL_LINES[sl]:30} ({sl})  →  {len(result[str(sl)])} entries")
    return result


def main():
    """Main extraction process."""
    print("╔════════════════════════════════════════════════════════════════╗")
    print("║         DBC to CSV/JSON Extractor for LW Bot Editor           ║")
    print("╚════════════════════════════════════════════════════════════════╝")
    print()
    print(f"DBC Source: {DBC_DIR}")
    print(f"Output Dir: {OUTPUT_DIR}")
    print()

    # Check if DBC directory exists
    if not DBC_DIR.exists():
        print(f"❌ DBC directory not found: {DBC_DIR}")
        print()
        print("Please extract DBC files from your WoW client first:")
        print("  1. Run map extractor tools")
        print("  2. Copy *.dbc files to var/extractors/dbc/")
        print()
        return 1

    # Extract spell names
    spell_names = extract_spell_names()
    if not spell_names:
        print("\n❌ Failed to extract spell names")
        return 1

    # Extract class spell mappings
    class_spells = extract_class_spells()
    if not class_spells:
        print("\n❌ Failed to extract class spell mappings")
        return 1

    # Save spell names
    print("\n💾 Saving spell_names.json...")
    spell_names_file = OUTPUT_DIR / "spell_names.json"
    with spell_names_file.open("w", encoding="utf-8") as f:
        json.dump(spell_names, f, indent=2, ensure_ascii=False)
    print(f"   ✅ Saved {len(spell_names):,} spell names to {spell_names_file.name}")

    # Save class spells with names
    print("\n💾 Saving class_spells.json...")
    class_spells_with_names = {}
    for class_id, spell_ids in class_spells.items():
        class_name = CLASS_NAMES.get(class_id, f"Class{class_id}")
        class_spells_with_names[class_name] = [
            {
                "id": spell_id,
                "name": spell_names.get(spell_id, f"Spell {spell_id}")
            }
            for spell_id in spell_ids
            if spell_id in spell_names  # Only include if we have the name
        ]

    class_spells_file = OUTPUT_DIR / "class_spells.json"
    with class_spells_file.open("w", encoding="utf-8") as f:
        json.dump(class_spells_with_names, f, indent=2, ensure_ascii=False)
    print(f"   ✅ Saved class spell mappings to {class_spells_file.name}")

    # Save simple spell lookup CSV
    print("\n💾 Saving spell_lookup.csv...")
    spell_csv_file = OUTPUT_DIR / "spell_lookup.csv"
    with spell_csv_file.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["spell_id", "spell_name"])
        for spell_id, name in sorted(spell_names.items()):
            writer.writerow([spell_id, name])
    print(f"   ✅ Saved spell lookup table to {spell_csv_file.name}")

    # Extract and save faction names (optional — requires Faction.dbc)
    faction_data = extract_faction_names()
    faction_file = OUTPUT_DIR / "faction_names.json"
    if faction_data:
        print("\n💾 Saving faction_names.json...")
        with faction_file.open("w", encoding="utf-8") as f:
            json.dump(faction_data, f, indent=2, ensure_ascii=False)
        print(f"   ✅ Saved {len(faction_data):,} faction names to {faction_file.name}")

    # Extract and save talent data (optional — requires Talent.dbc + TalentTab.dbc)
    talent_data = extract_talent_data(spell_names)
    talent_file = OUTPUT_DIR / "talent_data.json"
    if talent_data:
        print("\n💾 Saving talent_data.json...")
        with talent_file.open("w", encoding="utf-8") as f:
            json.dump(talent_data, f, indent=2, ensure_ascii=False)
        total_talents = sum(
            len(tree["talents"])
            for cls_data in talent_data.values()
            for tree in cls_data.values()
        )
        print(f"   ✅ Saved {total_talents} talents to {talent_file.name}")

    # Extract and save enchantment data (optional — requires SpellItemEnchantment.dbc)
    enchant_data = extract_enchantment_data()
    enchant_file = OUTPUT_DIR / "enchantment_data.json"
    if enchant_data:
        print("\n💾 Saving enchantment_data.json...")
        with enchant_file.open("w", encoding="utf-8") as f:
            json.dump(enchant_data, f, indent=2, ensure_ascii=False)
        print(f"   ✅ Saved {len(enchant_data):,} enchantments to {enchant_file.name}")

    # Extract and save skill line ability data (profession recipes)
    skill_data = extract_skill_line_abilities(spell_names)
    skill_file = OUTPUT_DIR / "skill_line_abilities.json"
    if skill_data:
        print("\n💾 Saving skill_line_abilities.json...")
        with skill_file.open("w", encoding="utf-8") as f:
            json.dump(skill_data, f, indent=2, ensure_ascii=False)
        total_recipes = sum(len(v) for v in skill_data.values())
        print(f"   ✅ Saved {total_recipes:,} profession recipes to {skill_file.name}")

    # Extract and save gem property data (optional — requires GemProperties.dbc)
    gem_data = extract_gem_properties()
    gem_file = OUTPUT_DIR / "gem_properties.json"
    if gem_data:
        print("\n💾 Saving gem_properties.json...")
        with gem_file.open("w", encoding="utf-8") as f:
            json.dump(gem_data, f, indent=2, ensure_ascii=False)
        print(f"   ✅ Saved {len(gem_data):,} gem properties to {gem_file.name}")

    print("\n" + "="*66)
    print("✨ Extraction complete!")
    print()
    print("Generated files:")
    print(f"  • {spell_names_file.name} - {len(spell_names):,} spell names")
    print(f"  • {class_spells_file.name} - Spells grouped by class")
    print(f"  • {spell_csv_file.name} - Simple CSV lookup table")
    print(f"  • {faction_file.name}     - " + (f"{len(faction_data):,} faction names" if faction_data else "SKIPPED (Faction.dbc not found)"))
    print(f"  • {talent_file.name}     - " + ("Talent trees for all classes" if talent_data else "SKIPPED (Talent.dbc / TalentTab.dbc not found)"))
    print(f"  • {enchant_file.name}    - " + (f"{len(enchant_data):,} enchantments" if enchant_data else "SKIPPED (SpellItemEnchantment.dbc not found)"))
    print(f"  • {gem_file.name}     - " + (f"{len(gem_data):,} gem properties" if gem_data else "SKIPPED (GemProperties.dbc not found)"))
    print(f"  • {skill_file.name} - " + (f"{sum(len(v) for v in skill_data.values()):,} profession recipes" if skill_data else "SKIPPED (SkillLineAbility.dbc not found)"))
    print()
    print("The bot editor will now use these files instead of DBC files.")
    print("="*66)

    return 0

if __name__ == "__main__":
    sys.exit(main())
