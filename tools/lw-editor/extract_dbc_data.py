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
DBC_DIR = SCRIPT_DIR.parent.parent / "var" / "extractors" / "dbc"
OUTPUT_DIR = SCRIPT_DIR

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

    print("\n" + "="*66)
    print("✨ Extraction complete!")
    print()
    print("Generated files:")
    print(f"  • {spell_names_file.name} - {len(spell_names):,} spell names")
    print(f"  • {class_spells_file.name} - Spells grouped by class")
    print(f"  • {spell_csv_file.name} - Simple CSV lookup table")
    print()
    print("The bot editor will now use these files instead of DBC files.")
    print("="*66)

    return 0

if __name__ == "__main__":
    sys.exit(main())
