# Quick Reference

## Combat Profile Conditions

### Stat Keys
| Stat Key | Type | Range | Description |
|---|---|---|---|
| `hp_pct` | Number | 0–100 | Health percentage |
| `mana_pct` | Number | 0–100 | Mana percentage |
| `rage` | Number | 0–100 | Rage |
| `energy` | Number | 0–100 | Energy |
| `runic_power` | Number | 0–100 | Runic Power |
| `combo_points` | Number | 0–5 | Combo points |
| `distance` | Number | yards | Distance to subject |
| `in_melee` | Bool | 0/1 | In melee range |
| `is_moving` | Bool | 0/1 | Subject is moving |
| `is_casting` | Bool | 0/1 | Subject is casting |
| `aura` | Bool | 0/1 | Aura present (by spell ID) |
| `aura_stacks` | Number | 0–255 | Stack count of aura |
| `threat_pct` | Number | 0–200+ | Threat as % of top holder |
| `is_aggro_holder` | Bool | 0/1 | Subject holds aggro |

### Typical Threat Thresholds
| Role | Safe | Warning | Danger |
|---|---|---|---|
| Tank | 120%+ | 100–120% | < 100% |
| DPS | < 70% | 70–90% | > 90% |
| Healer | < 60% | 60–80% | > 80% |

### Common Examples
```
# Tank — taunt when losing aggro
bot.is_aggro_holder == False  →  Taunt → enemy_primary

# DPS — threat dump
bot.threat_pct >= 90          →  Feint → self

# DPS — hold fire
bot.threat_pct >= 85  AND  owner.is_aggro_holder == False  →  (wait)

# Healer — smart heal
lowest_hp_party.hp_pct < 60   →  Flash Heal → lowest_hp_party

# Aura stack tracker
target.aura_stacks >= 5  (aura: Deep Wounds 12721)  →  Execute → enemy_primary
```

---

## Character Editor — Common Tasks

### Edit character stats
1. Select account → select character
2. **Summary** tab → edit Level / XP / Gold / Bank Slots
3. Click **💾 Save Basic Stats**

### Change gear on a slot
1. **Equipped Gear** tab → find the slot
2. Change quality filter if needed (filters the item dropdown)
3. Pick item → pick enchant → pick gems
4. Click **Apply Gear Changes**
5. Hover over any item to see the full WoW-style tooltip

### Add / remove a talent point
1. **Talents** tab → find the talent in the tree
2. Select it → click **Add Point** or **Remove Point**
3. To wipe all: **Reset Talents**

### Check profession recipes
1. **Professions** tab → pick the profession sub-tab
2. ✓ = known, — = not known
3. Counter shows `23 / 74 known`
4. Click **Recipe** or **Min Skill** header to sort

### Sort any data grid
Click any column header — ▲ ascending, click again ▼ descending.  
Works on: Character list, all Reputation tabs, Inventory, Achievements, all Profession recipe lists.

### Refresh from DB (bypass cache)
Click **🔄 Refresh Character** at the top of the character editor.

---

## Reputation Tab Layout
```
[ Classic | TBC | WotLK | Other ]
  Faction           Group          Standing   Rank       Flags
  Stormwind         Alliance       42000      Exalted    ✓
  Ironforge         Alliance       42000      Exalted    ✓
  Argent Dawn       (root)         8500       Honored
```
Color-coded by rank. Profession entries are filtered out.

---

## Gear Tooltip Contents
```
Thunderfury, Blessed Blade of the Windseeker  [Legendary]
Item Level 80
One-Handed Sword          One-Hand
44 - 115 Damage
+16 - 30 Nature Damage
Speed 1.90  (41.8 damage per second)
+5 Agility
+8 Stamina
Chance on Hit: Thunderfury
Requires Level 60
```

---

## Validation
```bash
python startup_validate.py   # must pass before reporting issues
```

---

## DBC Extraction
```bash
python extract_dbc_data.py   # one-time setup, re-run after DBC changes
```

Requires in `var/extractors/dbc/`:  
`Spell.dbc` · `SkillLineAbility.dbc` · `SkillLine.dbc` · `Faction.dbc`  
`Talent.dbc` · `TalentTab.dbc` · `SpellItemEnchantment.dbc` · `GemProperties.dbc`

---

## Files Reference
| File | Purpose |
|---|---|
| `lw_bot_editor.py` | Entry point |
| `startup_validate.py` | Pre-flight validation script |
| `extract_dbc_data.py` | DBC → JSON extractor |
| `config.ini` | Saved connection settings |
| `lw_editor/tab_character.py` | Character editor tab |
| `lw_editor/tab_professions.py` | Professions panel |
| `lw_editor/tab_bots.py` | Bot profile editor tab |
| `lw_editor/tab_accounts.py` | Account management tab |
| `lw_editor/db.py` | All database access |
| `lw_editor/helpers.py` | UI helpers incl. `make_tv_sortable` |
| `lw_editor/constants.py` | All constants and DBC data |
| `docs/UI_GUIDE.md` | Full UI reference |
| `docs/SSH_SETUP.md` | SSH tunnel setup |
| `docs/JSON_CACHE_GUIDE.md` | DBC extraction deep-dive |
| `docs/THREAT_EXAMPLES.md` | 20+ rotation examples |


## New Condition Stats

### 🎯 Threat Management
| Stat Key | Type | Range | Description | Example Use |
|----------|------|-------|-------------|-------------|
| `threat_pct` | Number | 0-200+ | Your threat % vs top threat holder | DPS threat dump at 90% |
| `is_aggro_holder` | Boolean | 0 or 1 | Whether you have aggro | Tank taunt when False |
| `aura_stacks` | Number | 0-255 | Stacks of a specific aura | DoT at 5 stacks |

### 🎯 New Target Keys
| Target Key | Description | Best For |
|------------|-------------|----------|
| `lowest_hp_party` | Party member with lowest HP% | Healer smart targeting |
| `enemy_primary_victim` | Who the boss is attacking | Off-tank awareness |

---

## Quick Examples

### Tank: Auto-Taunt
```
Condition: bot.is_aggro_holder == False
Action: Taunt → enemy_primary
```

### DPS: Threat Dump
```
Condition: bot.threat_pct >= 90
Action: Feint → self
```

### Healer: Smart Healing
```
Condition: lowest_hp_party.hp_pct < 60
Action: Flash Heal → lowest_hp_party
```

### All: Stack Tracker
```
Condition: target.aura_stacks >= 5
   Aura: Deep Wounds [12721]
   Value: 5
Action: Execute → enemy_primary
```

---

## Typical Threat Thresholds

| Role | Safe Zone | Warning Zone | Danger Zone |
|------|-----------|--------------|-------------|
| **Tank** | 120%+ | 100-120% | <100% |
| **DPS** | <70% | 70-90% | >90% |
| **Healer** | <60% | 60-80% | >80% |

---

## Editor UI Changes

### Condition Stat Dropdown
```
[Subject: bot ▼] [Stat: threat_pct ▼] [Op: >= ▼] [Value: 90]
```

### Aura Stacks UI
```
[Subject: target ▼] [Stat: aura_stacks ▼] [Op: >= ▼]
[Aura: Deep Wounds [12721] ▼] [Stacks: 5]
```

### Boolean Stats
```
[Subject: bot ▼] [Stat: is_aggro_holder ▼] [Op: == ▼] [Value: True ▼]
```

---

## Files Updated

✅ `tools/lw-editor/lw_bot_editor.py` - Main editor code
📄 `tools/lw-editor/README.md` - Complete documentation
📘 `tools/lw-editor/THREAT_EXAMPLES.md` - Practical examples
📋 `tools/lw-editor/CHANGELOG_THREAT.md` - Technical details

---

## How to Use

1. Open the bot editor: `python lw_bot_editor.py`
2. Select or create a combat profile
3. Add a new rotation entry
4. Add a condition using the new stat keys
5. Set the threshold value
6. Save and test in-game!

---

## Need Help?

- Check `THREAT_EXAMPLES.md` for 20+ real rotation examples
- Check `README.md` for complete stat/target/operator reference
- Server logs show `[LivingWorldDebug]` entries for condition evaluation
- Test with `.lwbot attack` command to verify rotations
