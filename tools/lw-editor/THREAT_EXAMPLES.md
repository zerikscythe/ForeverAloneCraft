# LivingWorld Bot Threat-Aware Rotation Examples

This guide provides practical examples of using the new threat-aware features in bot rotations.

## Understanding Threat

**Threat Percentage (`threat_pct`)**: 
- Measures your threat relative to the person with the highest threat
- 100% = You have the same threat as the top threat holder
- 50% = You have half the threat of the top threat holder
- 110% = You have 10% more threat than the current top (about to pull aggro!)

**Aggro Holder (`is_aggro_holder`)**:
- Boolean value (True/False or 1/0)
- True when the subject is currently being attacked by the enemy
- Useful for tanks to know when they have aggro, or DPS to know when they pulled it

## Tank Rotations

### Basic Taunt Macro (All Tanks)
```
Priority: 10
Label: Emergency Taunt
Conditions: bot.is_aggro_holder == False
Action: Cast Taunt → enemy_primary
```
**Use Case**: Automatically taunt when you lose aggro

### Threat Generation (Warrior Tank)
```
Priority: 5
Label: Heroic Strike (High Threat)
Conditions: bot.threat_pct < 150
Condition Logic: All (AND)
Action: Cast Heroic Strike → enemy_primary
```
**Use Case**: Build threat when you're below 150% of DPS threat

### AoE Threat (Paladin Tank)
```
Priority: 8
Label: Consecration
Conditions: bot.threat_pct < 120
Condition Logic: All (AND)
Action: Cast Consecration → self
```
**Use Case**: Generate AoE threat when losing aggro lead

## DPS Rotations

### Threat Dump (Rogue)
```
Priority: 3
Label: Feint
Conditions: 
  - bot.threat_pct >= 90
  - bot.is_aggro_holder == False
Condition Logic: Any (OR)
Action: Cast Feint → self
```
**Use Case**: Use Feint when threat gets too high (90%+) or when you already pulled aggro

### Threat Dump (Hunter)
```
Priority: 3
Label: Feign Death
Conditions: bot.threat_pct >= 95
Action: Cast Feign Death → self
```
**Use Case**: Drop all threat when approaching aggro threshold

### Pause DPS When Tank Lost Aggro
```
Priority: 1
Label: Wait for Tank
Conditions: 
  - owner.is_aggro_holder == False
  - bot.threat_pct >= 80
Condition Logic: All (AND)
Action: (No secondary action - just wait)
```
**Use Case**: Stop attacking when the tank doesn't have aggro and you're getting close

### Mage - Aggressive DPS with Safety
```
Priority: 5
Label: Pyroblast (Safe)
Conditions:
  - bot.threat_pct < 70
  - owner.is_aggro_holder == True
Condition Logic: All (AND)
Action: Cast Pyroblast → enemy_primary
```
**Use Case**: Only use high-threat abilities when tank has solid aggro

### Tricks of the Trade (Rogue) - Help Tank
```
Priority: 2
Label: Tricks on Tank
Conditions:
  - owner.threat_pct < 100
  - bot.threat_pct >= 60
Condition Logic: All (AND)
Action: Cast Tricks of the Trade → owner
```
**Use Case**: Give threat to tank when they're falling behind

## Healer Rotations

### Fade (Priest)
```
Priority: 8
Label: Fade
Conditions: bot.is_aggro_holder == True
Action: Cast Fade → self
```
**Use Case**: Drop threat when you pull aggro from healing

### Pre-emptive Threat Management (Priest)
```
Priority: 7
Label: Fade Early
Conditions: bot.threat_pct >= 85
Action: Cast Fade → self
```
**Use Case**: Use Fade before you pull aggro

### Hand of Salvation (Paladin Healer)
```
Priority: 6
Label: Save DPS from Aggro
Conditions: lowest_hp_party.threat_pct >= 95
Action: Cast Hand of Salvation → lowest_hp_party
```
**Use Case**: Help party members who are about to pull aggro

## Hybrid DPS/Heal Rotations

### Shaman - Smart Healing
```
Priority: 4
Label: Emergency Heal
Conditions:
  - lowest_hp_party.hp_pct < 50
  - bot.is_aggro_holder == False
Condition Logic: All (AND)
Action: Cast Healing Wave → lowest_hp_party
```
**Use Case**: Only heal when not tanking adds

### Paladin - DPS or Heal Based on Threat
```
Priority: 5
Label: Flash of Light
Conditions:
  - lowest_hp_party.hp_pct < 70
  - bot.threat_pct < 60
Condition Logic: All (AND)
Action: Cast Flash of Light → lowest_hp_party

Priority: 6
Label: Crusader Strike
Conditions:
  - owner.hp_pct > 80
  - bot.threat_pct < 75
Condition Logic: All (AND)
Action: Cast Crusader Strike → enemy_primary
```
**Use Case**: Prioritize healing when safe, DPS when low threat

## Advanced Examples

### Execute Phase with Threat Awareness (Warrior DPS)
```
Priority: 2
Label: Execute (Safe)
Conditions:
  - enemy_primary.hp_pct < 20
  - bot.threat_pct < 85
  - owner.is_aggro_holder == True
Condition Logic: All (AND)
Action: Cast Execute → enemy_primary
```
**Use Case**: Spam Execute during execute phase but only when tank has solid aggro

### Multi-Condition Threat Check (Any DPS)
```
Priority: 3
Label: Big Damage Combo
Conditions:
  - bot.threat_pct < 60
  - owner.threat_pct >= 120
  - enemy_primary.hp_pct > 30
Condition Logic: All (AND)
Action: Cast [High Damage Ability] → enemy_primary
```
**Use Case**: Only use big abilities when tank has strong threat lead

### Targeting Boss's Victim (Off-Tank)
```
Priority: 9
Label: Protect Main Tank
Conditions:
  - enemy_primary_victim.hp_pct < 80
  - bot.is_aggro_holder == False
Condition Logic: All (AND)
Action: Cast Taunt → enemy_primary
```
**Use Case**: Taunt boss when it's attacking the main tank and they're getting low

## Tips for Threat-Aware Rotations

1. **Tank Priority 1-15**: Use low priorities (1-15) for threat generation
2. **DPS Priority 20-40**: Use medium priorities for normal rotation
3. **Threat Dumps Priority 1-10**: High priority to prevent aggro pulls
4. **Typical Threat Thresholds**:
   - DPS should stay below 80-90% threat
   - Tanks should maintain 120%+ threat
   - Healers should use threat reduction around 70-80%

5. **Combine with Other Conditions**: Mix threat conditions with HP, mana, auras, etc.
6. **Use OR Logic for Safety**: When using threat dumps, use OR logic to catch multiple scenarios
7. **Test Incrementally**: Start with simple conditions and add complexity as needed

## Debugging Tips

- Check the server logs for `[LivingWorldDebug]` entries to see what conditions are evaluating
- The bot sends threat percentage to the owner via addon messages
- Use the `.lwbot attack` command to test specific rotations in controlled scenarios
- Start with conservative threat thresholds (lower for DPS, higher for tanks) and adjust

## Integration with Existing Rotations

All threat conditions work alongside existing conditions:
- Combine with `hp_pct`, `mana_pct` for resource management
- Combine with `aura` for buff/debuff checks
- Combine with `aura_stacks` for stacking debuff management
- Combine with `distance` for range checks

Example:
```
Priority: 10
Label: Safe Big Nuke
Conditions:
  - bot.mana_pct > 60
  - bot.threat_pct < 70
  - enemy_primary.hp_pct > 20
  - target.aura == 12345 (Has debuff)
Condition Logic: All (AND)
Action: Cast [Big Spell] → enemy_primary
```

This allows you to create sophisticated, context-aware rotations that react to the full combat situation.
