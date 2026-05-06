# DBC Extraction & JSON Cache Guide

The editor never reads DBC files at runtime. Instead, `extract_dbc_data.py` converts them to
small JSON files once. The editor loads those files on startup in milliseconds.

---

## Quick Start

### Step 1 — Copy DBC files

Copy the following files from your WoW 3.3.5a client (`Data/enUS/DBFilesClient/`)
into `var/extractors/dbc/`:

```
Spell.dbc
SkillLineAbility.dbc
SkillLine.dbc
Faction.dbc
Talent.dbc
TalentTab.dbc
SpellItemEnchantment.dbc
GemProperties.dbc
```

> **Alternative:** Download them from [wow.tools](https://wow.tools/dbc/) → select build `3.3.5`.

### Step 2 — Run the extractor

```bash
cd tools/lw-editor
python extract_dbc_data.py
```

### Step 3 — Validate

```bash
python startup_validate.py
```

All 7 data-file checks should pass before launching the editor.

---

## Output Files

All files are written to `var/extractors/data/` (configured via `DATA_DIR` in `lw_editor/__init__.py`).

| File | Size (approx) | Entries | Used for |
|---|---|---|---|
| `spell_names.json` | ~2 MB | 48,885 | Spell ID → name everywhere |
| `class_spells.json` | ~1 MB | ~2,500 | Per-class spell pickers |
| `spell_lookup.csv` | ~2 MB | 48,885 | Human reference |
| `faction_names.json` | ~30 KB | 87 | Reputation tab grouping |
| `talent_data.json` | ~200 KB | 829 talents | Talent tree display |
| `enchantment_data.json` | ~800 KB | 2,655 | Enchant picker |
| `gem_properties.json` | ~50 KB | 626 | Gem socket picker |
| `skill_line_abilities.json` | ~500 KB | 3,556 recipes | Professions browser |

**Total: ~6 MB** — compare to ~200 MB of raw DBC files.

---

## faction_names.json format

Unlike other files, this is a dict of dicts rather than a simple id→string map:

```json
{
  "1": {
    "name": "Stormwind",
    "parent_id": 1118,
    "parent_name": "Alliance",
    "expansion": "Classic"
  },
  ...
}
```

`expansion` is one of: `"Classic"`, `"The Burning Crusade"`, `"Wrath of the Lich King"`, `"Other"`.

The extractor automatically:
- Walks the full parent chain to determine the expansion root
- Filters out internal/profession-spec pseudo-factions (`Blacksmithing - Armorsmithing`, etc.)
- Filters out `UNUSED`, `REUSE`, `DND`, `DNR` entries

---

## skill_line_abilities.json format

```json
{
  "185": [
    {
      "spell_id": 2550,
      "spell_name": "Cooking",
      "min_skill": 0,
      "superseded_by": 0
    },
    ...
  ]
}
```

The key is the `skill_line_id` (e.g. `185` = Cooking, `202` = Engineering).
Entries superseded by a higher-rank version of the same spell are excluded automatically.

Skill lines extracted:

| Skill Line ID | Profession |
|---|---|
| 129 | First Aid |
| 185 | Cooking |
| 356 | Fishing |
| 171 | Alchemy |
| 164 | Blacksmithing |
| 333 | Enchanting |
| 202 | Engineering |
| 773 | Inscription |
| 755 | Jewelcrafting |
| 165 | Leatherworking |
| 197 | Tailoring |
| 203 | Gnomish Engineering (spec) |
| 204 | Goblin Engineering (spec) |
| 751–753 | LW specs (Tribal / Elemental / Dragonscale) |
| 220, 221 | BS specs (Armorsmith / Weaponsmith) |
| 904–906 | Tailoring specs (Mooncloth / Spellfire / Shadoweave) |

---

## Updating the Cache

Re-run extraction any time you:
- Update DBC files
- Add custom spells or factions to your server
- Change the extraction logic in `extract_dbc_data.py`

```bash
python extract_dbc_data.py   # overwrites existing JSON files
python startup_validate.py   # verify the new files pass all checks
```

---

## Git Considerations

The JSON files are **not** source code — they're build artefacts derived from DBC files.

**Option A — gitignore them (recommended)**

Add to `.gitignore`:
```
var/extractors/data/*.json
var/extractors/data/*.csv
```

Each developer runs `extract_dbc_data.py` once after clone.

**Option B — commit them**

If your team doesn't have DBC files readily available, committing the JSON files makes
setup one step shorter for everyone:

```bash
git add var/extractors/data/*.json
git commit -m "chore: add pre-extracted DBC data cache"
```

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `❌ DBC directory not found` | Copy DBC files to `var/extractors/dbc/` |
| `⚠ Faction.dbc not found — skipping` | Only faction grouping is affected; editor still works |
| Profession tab shows "No recipe data" | Run `extract_dbc_data.py`; check `skill_line_abilities.json` exists |
| Reputation tab shows unknown factions | Faction IDs not in `faction_names.json`; they appear in **Other** tab |
| `startup_validate.py` reports missing data files | Run `extract_dbc_data.py` first |
| Talent trees empty | Requires `Talent.dbc` and `TalentTab.dbc`; run extractor again |


## Overview

Instead of keeping large binary DBC files around, you can extract the spell data once into small JSON files that the editor can load instantly.

**Benefits:**
- ✅ **Fast loading** - JSON is much faster to parse than DBC
- ✅ **Small files** - ~2-5 MB total vs hundreds of MB of DBC files  
- ✅ **Human readable** - Can view/edit in any text editor
- ✅ **Properly filtered** - Only combat-usable spells per class
- ✅ **One-time setup** - Extract once, use forever
- ✅ **No database needed** - Works offline

## Quick Start

### Step 1: Extract DBC Files (One Time)

You need WoW 3.3.5a client DBC files. Get them from:

**Option A: Your WoW Client**
```
<WoW Install>/Data/enUS/DBFilesClient/
```
Copy these 3 files to `var/extractors/dbc/`:
- `Spell.dbc`
- `SkillLineAbility.dbc`
- `SkillLine.dbc`

**Option B: wow.tools Online**
1. Go to https://wow.tools/dbc/
2. Select "3.3.5 (WotLK)"
3. Download the 3 files above
4. Place in `var/extractors/dbc/`

### Step 2: Run Extraction Script

```bash
cd tools/lw-editor
python extract_dbc_data.py
```

### Step 3: Done!

The editor will now use the JSON files automatically. You can delete the DBC files if you want.

## What Gets Created

After running the extraction script:

```
tools/lw-editor/
├── spell_names.json      (~2 MB) - All spell IDs → names
├── class_spells.json     (~1 MB) - Class → spell list mapping
└── spell_lookup.csv      (~2 MB) - Simple CSV for reference
```

### spell_names.json
```json
{
  "133": "Fireball",
  "116": "Frostbolt",
  "5143": "Arcane Missiles"
  ...
}
```

### class_spells.json
```json
{
  "Mage": [
    {"id": 133, "name": "Fireball"},
    {"id": 116, "name": "Frostbolt"},
    ...
  ],
  "Warrior": [
    {"id": 78, "name": "Heroic Strike"},
    ...
  ]
}
```

### spell_lookup.csv
```csv
spell_id,spell_name
133,Fireball
116,Frostbolt
...
```

## File Sizes

| File | Size | Records | Purpose |
|------|------|---------|---------|
| `spell_names.json` | ~2 MB | 45,000+ | ID → name lookup |
| `class_spells.json` | ~1 MB | ~2,500 | Class spell lists |
| `spell_lookup.csv` | ~2 MB | 45,000+ | Human reference |
| **Total** | **~5 MB** | | |

Compare to DBC files: ~200+ MB

## Loading Priority

The bot editor checks spell data sources in this order:

1. **JSON cache** (if files exist) ← **Fastest**
2. DBC files (if extracted)
3. Database `spell_dbc` table
4. Manual entry (always works)

## Updating the Cache

If WoW gets patched or you add custom spells:

1. Update your DBC files
2. Run `python extract_dbc_data.py` again
3. Overwrites old JSON files

## Git Integration

**Add to `.gitignore`:**
```
# Spell data cache (regenerate from DBC files)
tools/lw-editor/spell_names.json
tools/lw-editor/class_spells.json
tools/lw-editor/spell_lookup.csv
```

**Or commit them** if you want to share spell data with your team:
```bash
git add tools/lw-editor/*.json
git commit -m "Add spell data cache for editor"
```

## Customizing the Extraction

Edit `extract_dbc_data.py` to customize:

### Filter Out More Spells

```python
def is_noise_spell(name: str) -> bool:
    lowered = name.lower()
    return (
        lowered.startswith("zzold") or
        # Add more filters:
        lowered.startswith("qa") or
        "internal" in lowered or
        ...
    )
```

### Add More Data

```python
# In extract_spell_names():
spell_data[spell_id] = {
    "name": name,
    "icon": row[icon_idx],  # Add icon ID
    "rank": get_rank(spell_id),  # Add rank info
    # etc.
}
```

### Change Output Format

```python
# Use YAML instead of JSON:
import yaml
with open("spell_data.yaml", "w") as f:
    yaml.dump(data, f)
```

## Troubleshooting

### "DBC files not found"
```
❌ DBC directory not found: var/extractors/dbc
```
**Solution**: Extract DBC files from WoW client first (see Step 1)

### "No spell data available"
The editor works in this order:
1. Check JSON cache → Not found
2. Check DBC files → Not found  
3. Check database → Not found
4. Allow manual entry

**Solution**: Run `python extract_dbc_data.py` to create JSON cache

### Empty Spell Lists
If class spell lists are empty after extraction:

**Check:**
- `SkillLineAbility.dbc` exists and is valid
- `SkillLine.dbc` exists and is valid
- No errors during extraction

**Re-extract:**
```bash
rm spell_names.json class_spells.json
python extract_dbc_data.py
```

### JSON Parse Error
```
Failed to load JSON cache: ...
```

**Solution**: Re-run extraction or delete corrupted JSON:
```bash
rm *.json
python extract_dbc_data.py
```

## Performance

| Method | Load Time | Memory | Disk Space |
|--------|-----------|--------|------------|
| JSON cache | ~50ms | ~5 MB | ~5 MB |
| DBC files | ~500ms | ~20 MB | ~200 MB |
| Database | ~2000ms | ~10 MB | Database size |

JSON cache is **10x faster** than DBC and **40x faster** than database.

## Advanced: Minimal Setup

If you only want Mage spells:

```python
# Edit extract_dbc_data.py
def main():
    # ... existing code ...

    # Only extract Mage (class 8)
    class_spells = {8: extract_class_spells()[8]}

    # Save...
```

This creates tiny JSON files with just Mage spells.

## Reference: Extraction Script Output

```
╔════════════════════════════════════════════════════════════════╗
║         DBC to CSV/JSON Extractor for LW Bot Editor           ║
╚════════════════════════════════════════════════════════════════╝

DBC Source: D:\...\var\extractors\dbc
Output Dir: D:\...\tools\lw-editor

🔮 Extracting spell names...
📖 Reading Spell.dbc...
   ✅ Loaded 45,854 records (234 fields each)
   ✅ Extracted 41,237 spell names

🎯 Extracting class spell associations...
📖 Reading SkillLineAbility.dbc...
   ✅ Loaded 25,478 records (14 fields each)
📖 Reading SkillLine.dbc...
   ✅ Loaded 752 records (18 fields each)
   ℹ️  Found 11 class skill lines
   ✅ Mapped 2,547 spell associations across 10 classes
      Warrior: 158 spells
      Paladin: 142 spells
      Hunter: 189 spells
      Rogue: 124 spells
      Priest: 167 spells
      Death Knight: 201 spells
      Shaman: 178 spells
      Mage: 198 spells
      Warlock: 176 spells
      Druid: 214 spells

💾 Saving spell_names.json...
   ✅ Saved 41,237 spell names to spell_names.json
💾 Saving class_spells.json...
   ✅ Saved class spell mappings to class_spells.json
💾 Saving spell_lookup.csv...
   ✅ Saved spell lookup table to spell_lookup.csv

══════════════════════════════════════════════════════════════
✨ Extraction complete!

Generated files:
  • spell_names.json - 41,237 spell names
  • class_spells.json - Spells grouped by class
  • spell_lookup.csv - Simple CSV lookup table

The bot editor will now use these files instead of DBC files.
══════════════════════════════════════════════════════════════
```

## Need Help?

1. **Missing DBC files?** See `SPELL_DATA_SETUP.md`
2. **Want spell IDs?** Check `spell_lookup.csv` or wowhead.com
3. **Custom server?** Extract from your server's DBC files
4. **Still stuck?** You can always type spell IDs manually
