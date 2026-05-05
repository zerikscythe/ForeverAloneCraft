# Quick Reference: New Bot Editor Features

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
