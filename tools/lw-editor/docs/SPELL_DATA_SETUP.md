# Spell Data Setup Guide

The LivingWorld Bot Editor needs spell data to show spell names in dropdowns. There are three ways to get this data:

## Option 1: Extract DBC Files (Recommended)

DBC (DataBaseClient) files contain all WoW client data including spell names.

### Steps:

1. **Locate your WoW 3.3.5a client installation**
   - Example: `C:\Program Files (x86)\World of Warcraft 3.3.5a\`

2. **Run the DBC extractor** (if using AzerothCore build tools):
   ```bash
   # From your server build directory
   cd build/bin/RelWithDebInfo
   ./mapextractor
   ./vmap4extractor
   ./vmap4assembler
   ./mmaps_generator
   ```

3. **Copy DBC files**:
   - DBC files will be extracted to `Buildings/` folder
   - Copy `*.dbc` files to: `var/extractors/dbc/`

### Required DBC Files:
- `Spell.dbc` - Spell names and data
- `SkillLineAbility.dbc` - Class spell associations
- `SkillLine.dbc` - Skill categories
- `spell_ranks.dbc` - Spell rank chains (if available)

## Option 2: Use spell_template Database Table

If your AzerothCore server has the `spell_template` table (custom modification), the editor can use it.

**Check if you have it:**
```sql
USE acore_world;
SHOW TABLES LIKE 'spell_template';
```

**This table is not part of standard AzerothCore** and requires a custom SQL patch.

## Option 3: Manual Spell ID Entry (Current Fallback)

Without DBC files or spell_template table, you can still use the editor by entering spell IDs manually.

### How to find spell IDs:

**WoWHead (easiest):**
1. Go to https://wowhead.com/wotlk
2. Search for your spell (e.g., "Fireball")
3. URL shows ID: `wowhead.com/wotlk/spell=133` → Spell ID is **133**

**WoW Wiki:**
1. Go to https://wowpedia.fandom.com/wiki/
2. Search for spell
3. Check spell ID in article

**In-game addon:**
- Install "IdTip" addon
- Hover over spell in spellbook
- ID shown in tooltip

### Entering spells manually in the editor:

1. In the spell combo box, **type the spell ID** (e.g., `133`)
2. Press Enter or Tab
3. The editor will save the spell ID
4. In game, the bot will cast the spell correctly

**Example:**
```
Action: Spell
Type in combo box: 133
Rank: Best Known
Target: enemy
```

This will work even without spell names showing.

## Current Status of Your Setup

Based on the errors, your setup currently has:
- ✗ DBC files: Missing (folder is empty)
- ✗ spell_template table: Missing (not in your database)
- ✓ Manual entry: **Works!** (You can type spell IDs)

## Recommended Solution

**For now**: Use manual spell ID entry
- Fast to get started
- No additional setup needed
- Fully functional

**For better UX**: Extract DBC files
- Get spell name dropdowns
- Easier to browse spells
- Auto-complete functionality

## Extracting Just DBC Files

If you don't want to extract maps/vmaps/mmaps (takes hours), you can extract just DBC files:

### Using wow.tools (Online, No Download)

1. Go to https://wow.tools/dbc/
2. Select expansion: **3.3.5 (WotLK)**
3. Download individual DBC files:
   - Spell.dbc
   - SkillLineAbility.dbc  
   - SkillLine.dbc

4. Place in: `var/extractors/dbc/`

### Using Client Files Directly

If you have WoW client installed:

1. Navigate to: `<WoW Install>/Data/<locale>/DBFilesClient/`
   - Example: `C:\WoW\Data\enUS\DBFilesClient\`

2. Copy these files to `var/extractors/dbc/`:
   - `Spell.dbc`
   - `SkillLineAbility.dbc`
   - `SkillLine.dbc`

## Verifying DBC Files Work

After adding DBC files, restart the editor. You should see:
```
[lw-editor] Loaded 45,000+ spells from Spell.dbc
```

Instead of:
```
[lw-editor] DBC files not found in var/extractors/dbc
```

## Troubleshooting

### "DBC files not found"
- Check path: `var/extractors/dbc/Spell.dbc` should exist
- Check file permissions (read access needed)
- Restart the editor after adding files

### "spell_template table doesn't exist"
- This is normal - most servers don't have this custom table
- Editor will fall back to DBC files
- If no DBC files, manual entry still works

### Spell names show as "#12345"
- DBC files not loaded or corrupt
- Spell ID is still valid for saving
- Bot will cast spell correctly in-game

### Spell dropdown is empty
- No DBC files AND no spell_template table
- **Solution**: Type spell IDs manually
- This is fully functional, just less convenient

## Quick Reference: Common Spell IDs

Here are some common spell base IDs to get you started:

**Mage:**
- Fireball: 133
- Frostbolt: 116
- Arcane Missiles: 5143
- Polymorph: 118
- Counterspell: 2139

**Priest:**
- Flash Heal: 2061
- Greater Heal: 2060
- Renew: 139
- Power Word: Shield: 17
- Shadow Word: Pain: 589

**Warrior:**
- Mortal Strike: 12294
- Bloodthirst: 23881
- Heroic Strike: 78
- Shield Slam: 23922
- Execute: 5308

**Paladin:**
- Flash of Light: 19750
- Holy Light: 635
- Crusader Strike: 35395
- Hammer of Wrath: 24275
- Consecration: 26573

For more spell IDs, use WoWHead as described above.
