# World Bot Player-Stat Parity Checklist

This checklist tracks how close `Creature`-backed `world bots` are to real
player stat behavior, and what files should be touched for each remaining
parity slice.

It is intentionally implementation-oriented:

- `Status` describes current parity
- `Checklist` describes the concrete next fidelity steps
- `Files to touch` lists the most likely module/core files
- `New helper / hook?` says whether an existing seam is probably enough

## Status Legend

- `Done` = close enough for the current architecture
- `Partial` = a player-like baseline exists, but not full player semantics
- `Missing` = no meaningful player-like runtime implementation yet

## Current Shared Seams

These are the main seams already in use for world-bot stat parity work:

- Spawn/materialization stat seeding:
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Existing world-bot baseline helpers:
  - `modules/mod-living-world/src/service/WorldBotPlayerStatBaseline.h`
  - `modules/mod-living-world/src/service/WorldBotAttackPowerBaseline.h`
  - `modules/mod-living-world/src/service/WorldBotPhysicalDamageBaseline.h`
  - `modules/mod-living-world/src/service/WorldBotDefensiveCombatBaseline.h`
  - `modules/mod-living-world/src/service/WorldBotCriticalStrikeBaseline.h`
  - `modules/mod-living-world/src/service/WorldBotSpellCriticalStrikeBaseline.h`
- Existing combat hook script:
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
- Existing UnitScript hook declarations/dispatch:
  - `src/server/game/Scripting/ScriptDefines/UnitScript.h`
  - `src/server/game/Scripting/ScriptDefines/UnitScript.cpp`
  - `src/server/game/Scripting/ScriptMgr.h`
- Current core combat seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
- Current player reference formulas:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

## 1. Primary Stats

### 1.1 Strength / Agility / Stamina / Intellect / Spirit

Status: `Done` for baseline, `Partial` for full downstream player semantics.

Checklist:

- [x] Seed class/race/level player-like primary stats at spawn
- [x] Layer virtual-loadout and assigned-gear aggregate primary-stat bonuses
- [ ] Add any missing downstream stat consumers not already covered by later slices

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotPlayerStatBaseline.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- No new core hook needed for the baseline itself.
- Future downstream parity should usually be implemented as narrow helper-driven
  runtime slices, not by forcing player fields onto `Creature`.

### 1.2 Health / Mana Baseline

Status: `Done` for base values, `Partial` for full player-side scaling.

Checklist:

- [x] Seed class/level base health and mana
- [x] Layer additive health/mana bonuses from loadout and assigned gear
- [ ] Audit any remaining player-only stamina/intellect bonus edge cases that
  are not already covered by the current baseline approach

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotPlayerStatBaseline.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing spawn-time seeding is enough unless a later slice proves a read-time
  override is needed.

### 1.3 Non-Mana Power Pools

Status: `Partial`.

Current state:

- Power type is set correctly for rage/energy/runic power.
- Spawn fill behavior is only an approximation.

Checklist:

- [x] Set correct non-mana power type
- [x] Match player-authentic spawn defaults and caps for rage/energy/runic power
- [x] Match player regen/update behavior for energy and runes
- [x] Match combat-generation behavior for rage/runic power where relevant

Implementation notes:

- Added a shared power-baseline helper for class power type, player-like caps,
  and spawn fill behavior.
- World bots now explicitly materialize energy at 100, rage at 1000, and runic
  power at 1000 instead of inheriting generic creature defaults.
- Energy uses the existing creature 20-per-tick regeneration path, matching the
  player-style 20 energy per two-second tick used here. Rage and runic power do
  not get passive regen; they remain combat/spell-effect generated.

Files to touch:

- Existing:
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Likely core references:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotPowerBaseline.h`
- May need new runtime hook if passive spawn seeding is not enough for energy /
  rune parity.

## 2. Armor And Resistances

### 2.1 Armor Baseline

Status: `Done` for baseline, `Partial` for full player ecosystem.

Checklist:

- [x] Seed armor from player-like agility baseline
- [x] Layer virtual-loadout and assigned-gear armor bonuses
- [ ] Audit armor-dependent downstream systems if later slices introduce armor
  penetration or deeper item semantics

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotPlayerStatBaseline.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing helper path is enough for the current baseline.

### 2.2 Non-Physical Resistances

Status: `Partial`.

Current state:

- World bots are reset to a fresh player-like non-physical resistance baseline
  of `0`.
- There is no meaningful player-like resistance scaling pipeline yet.

Checklist:

- [x] Reset non-physical resistances to clean player baseline
- [x] Add optional stat/loadout/gear-driven resistance bonuses if the design
  wants true resistance-bearing world bots
- [x] Audit spell resist behavior against `GetEffectiveResistChance(...)`
- [x] Decide whether spell penetration parity belongs in the same slice

Implementation notes:

- Assigned gear now carries item-template non-physical resistance fields for
  holy, fire, nature, frost, shadow, and arcane.
- Materialization applies those resistance bonuses after the clean player-like
  zero baseline reset, so resistance-bearing gear now affects spell resistance.
- Spell penetration is implemented separately in `GetEffectiveResistChance`,
  where it reduces the final non-physical resistance before the existing clamp.

Files to touch:

- Existing:
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
  - `modules/mod-living-world/src/model/WorldBotVirtualLoadout.h`
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core references:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotResistanceBaseline.h`
- May not need a new core hook if resistance values can be seeded cleanly.
- If spell-penetration parity is included, a new read-time hook around spell hit
  / resist calculation is likely.

## 3. Physical Throughput

### 3.1 Attack Power / Ranged Attack Power

Status: `Done` for baseline, `Partial` for full player-item semantics.

Checklist:

- [x] Seed player-like melee and ranged attack power
- [x] Layer virtual-loadout and assigned-gear AP bonuses
- [ ] Audit later slices that add expertise, haste, armor pen, or visible weapon
  fidelity

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotAttackPowerBaseline.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing helper path is enough for the current baseline.

### 3.2 Physical Weapon Damage

Status: `Partial`.

Current state:

- World bots use a player-like physical damage baseline seed.
- This is not full true-equipped-weapon fidelity.

Checklist:

- [x] Replace generic creature damage seed with player-like baseline
- [ ] Decide whether to model weapon subclass-specific formulas more deeply
- [ ] Decide whether to model dual-wield asymmetry and true equipped weapon
  ranges
- [ ] Decide whether normalized-vs-true weapon speed parity matters enough for
  world bots

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotPhysicalDamageBaseline.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Likely core references:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Unit/Unit.cpp`

New helper / hook?

- Existing helper path is enough for the current slice.
- A deeper weapon-fidelity slice may need a new helper:
  - `modules/mod-living-world/src/service/WorldBotWeaponDamageModel.h`

## 4. Avoidance And Melee Defense

### 4.1 Dodge

Status: `Partial`.

Checklist:

- [x] Replace generic creature dodge baseline with class/level/agility baseline
- [x] Add defense-skill and defense-rating contributions
- [x] Add diminishing returns parity
- [ ] Audit stance/form/weapon-state edge cases if needed

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotDefensiveCombatBaseline.h`
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
- Current core seam:
  - `src/server/game/Entities/Unit/Unit.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing `OnBeforeRollMeleeOutcomeAgainst(...)` hook is enough for baseline and
  likely enough for defense/rating/DR parity too.
- Likely extend existing helper rather than add a new hook.

### 4.2 Parry

Status: `Partial`.

Checklist:

- [x] Replace generic creature parry behavior with class-eligibility baseline
- [x] Add defense-skill and defense-rating contributions
- [x] Add diminishing returns parity
- [ ] Tighten true weapon/offhand/polymorph edge cases if needed

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotDefensiveCombatBaseline.h`
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
- Current core seam:
  - `src/server/game/Entities/Unit/Unit.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing melee-outcome hook is probably enough.

### 4.3 Block Chance

Status: `Partial`.

Checklist:

- [x] Replace generic creature block behavior with shield/class-eligibility logic
- [x] Add defense-skill and defense-rating contributions
- [ ] Audit full player block caps/limits

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotDefensiveCombatBaseline.h`
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
- Current core seam:
  - `src/server/game/Entities/Unit/Unit.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing melee-outcome hook is probably enough.

### 4.4 Block Value

Status: `Partial`.

Checklist:

- [x] Add player-like shield block value baseline
- [x] Decide whether strength and shield itemization should contribute
- [x] Apply value in the actual blocked-damage path, not only in UI-like fields

Implementation notes:

- Added a world-bot-only shield block value runtime hook at combat read time.
- Uses the player block-value shape: `strength * 0.5 + flat block value - 10`,
  floored at zero and multiplied by shield-block-value percent auras.
- Assigned shield/item block value now includes both `ITEM_MOD_BLOCK_VALUE` stats
  and the shield template `Block` field, with flat/pct block-value auras read at
  runtime for creatures.
- The hook feeds both ordinary blocked melee damage and weapon-based spell damage
  that can be blocked; non-world-bot creature behavior is unchanged.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core seam:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotBlockValueBaseline.h`
- May need a new UnitScript hook if a clean world-bot-only override is needed at
  blocked-damage read time.

### 4.5 Defense Skill / Defense Rating / Crit Suppression

Status: `Partial`.

Checklist:

- [x] Model defense-skill-derived avoidance/miss/crit suppression
- [x] Model defense-rating contribution
- [x] Decide whether to keep this as pure runtime adjustment instead of true
  player stat-field emulation

Notes:

- Defense rating is now consumed from assigned gear at melee-outcome roll time.
  It contributes to dodge/parry/block, miss chance, victim defense skill, and
  incoming melee crit suppression through the existing world-bot UnitScript
  hook. Dodge/parry/miss use the same class caps and diminishing constants as
  the player formulas; block remains the player-style direct rating path.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/service/WorldBotDefensiveCombatBaseline.h`
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotDefenseBaseline.h`
- Existing melee-outcome hook may be enough for avoidance and crit suppression.
- If spell-side suppression is included, an additional spell-hit / spell-crit
  hook may be needed.

## 5. Critical Strike

### 5.1 Melee Crit

Status: `Partial`.

Checklist:

- [x] Replace generic creature crit baseline with class/level/agility baseline
- [x] Add item/rating-driven crit contribution beyond current runtime modifiers
- [ ] Audit weapon-skill / defense interactions if we want closer player parity
- [ ] Audit special per-weapon / per-spec edge cases

Implementation notes:

- Assigned melee and ranged crit rating now convert through the appropriate
  combat-rating DBC bucket and add to the world-bot melee outcome crit chance
  after the player-like agility/base crit replacement.

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotCriticalStrikeBaseline.h`
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
- Current core seam:
  - `src/server/game/Entities/Unit/Unit.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing melee-outcome hook is enough for the current direction.

### 5.2 Spell Crit

Status: `Partial`.

Checklist:

- [x] Bypass ordinary creature no-spell-crit behavior for world bots only
- [x] Replace generic creature spell-crit base with class/level/intellect base
- [x] Add item/rating-driven spell crit contribution beyond current runtime
  modifiers
- [ ] Audit full caster-sheet parity and spell-school edge cases

Implementation notes:

- Assigned spell crit rating now converts through `CR_CRIT_SPELL` and is added
  in the world-bot spell crit hook after intellect/base crit and school crit
  auras.

Files to touch:

- Existing:
  - `modules/mod-living-world/src/service/WorldBotSpellCriticalStrikeBaseline.h`
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
  - `src/server/game/Scripting/ScriptDefines/UnitScript.h`
  - `src/server/game/Scripting/ScriptDefines/UnitScript.cpp`
  - `src/server/game/Scripting/ScriptMgr.h`
  - `src/server/game/Entities/Unit/Unit.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Existing `OnCalculateSpellDoneCritChance(...)` hook is enough for this family
  unless future caster parity expands into spell power / throughput / hit.

## 6. Hit, Expertise, And Miss Logic

### 6.1 Melee Hit

Status: `Partial`.

Checklist:

- [x] Model player-like melee hit contribution from ratings/auras
- [ ] Audit dual-wield / ranged / boss-level miss logic expectations
- [ ] Keep world-bot-only changes narrow and avoid global creature side effects

Notes:

- Assigned melee/ranged hit rating now reduces world-bot attacker miss chance
  through the existing melee-outcome hook. The adjustment is world-bot-only and
  leaves ordinary creature miss behavior untouched.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotMeleeHitBaseline.h`
- Existing `OnBeforeRollMeleeOutcomeAgainst(...)` hook may be enough for melee
  miss adjustments if we want to stay B-style.

### 6.2 Spell Hit

Status: `Partial`.

Checklist:

- [x] Model player-like spell hit contribution from ratings/auras
- [ ] Audit binary-spell resist / spell-pen / victim avoidance interactions
- [ ] Keep ordinary creature spell-hit behavior unchanged for non-world-bots

Notes:

- A narrow core `UnitScript` hook now exposes magic spell hit chance before the
  existing clamp and roll. The living-world script uses it only for world-bot
  attackers and adds assigned spell-hit rating through the same combat-rating
  DBC conversion used by player stats.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
  - `src/server/game/Scripting/ScriptDefines/UnitScript.h`
  - `src/server/game/Scripting/ScriptDefines/UnitScript.cpp`
  - `src/server/game/Scripting/ScriptMgr.h`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- New helper:
  - `modules/mod-living-world/src/service/WorldBotSpellHitBaseline.h`
- New core hook:
  - `OnCalculateMagicSpellHitChance(...)`

### 6.3 Expertise

Status: `Partial`.

Checklist:

- [x] Model expertise contribution against dodge/parry for world-bot attackers
- [x] Decide whether to express this as a direct runtime reduction instead of a
  stored stat field
- [ ] Audit one-hand / off-hand specialization edge cases only if needed

Notes:

- Assigned expertise rating now converts through the player combat-rating DBC
  tables and directly reduces dodge/parry chances for world-bot melee attackers
  in the melee-outcome hook. Ranged attacks intentionally skip expertise.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/script/WorldBotDefensiveCombatUnitScript.cpp`
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Current core seam:
  - `src/server/game/Entities/Unit/Unit.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotExpertiseBaseline.h`
- Existing melee-outcome hook is likely enough.

## 7. Speed And Tempo

### 7.1 Haste

Status: `Partial`.

Checklist:

- [x] Model melee haste into swing timers / attack speed
- [x] Model spell haste into cast time and possibly GCD behavior
- [x] Audit aura-based haste vs item/rating-derived haste

Notes:

- Assigned melee/ranged/spell haste ratings now convert through the shared
  player combat-rating DBC helper and apply through existing Unit haste methods
  after world-bot stat recalculation. This keeps assigned item haste separate
  from aura haste while using the same attack-time and cast-speed mechanics.
  GCD-specific behavior is left to the existing cast system.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Spells/SpellInfo.cpp`
  - `src/server/game/Entities/Unit/StatSystem.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

New helper / hook?

- New helper:
  - `modules/mod-living-world/src/service/WorldBotHasteBaseline.h`
- No new core hook was needed; existing `Unit::ApplyAttackTimePercentMod(...)`
  and `Unit::ApplyCastTimePercentMod(...)` are used at world-bot spawn time.

### 7.2 Mana Regen / MP5 / Spirit Regen

Status: `Partial`.

Checklist:

- [x] Model player-like mana regen baseline
- [x] Add item/aura contributions such as MP5 and spirit-driven regen
- [x] Audit five-second-rule / casting-regen expectations if healer bots need it

Notes:

- Creature mana regeneration now exposes a narrow power-regen hook. World bots
  use it for mana only, replacing the generic creature mana tick with a
  player-like spirit/intellect calculation plus assigned MP5, power-regen auras,
  stat-to-mana-regen auras, and interrupted-regeneration percent while under
  the last-mana-use effect.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Likely core references:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Spells/Auras/SpellAuraEffects.cpp`

New helper / hook?

- New helper:
  - `modules/mod-living-world/src/service/WorldBotManaRegenBaseline.h`
- New runtime hook:
  - `OnCalculatePowerRegen(...)`

## 8. Caster Throughput

### 8.1 Spell Power / Healing Done

Status: `Partial`.

Checklist:

- [x] Model player-like spell power contribution
- [x] Model healing-done contribution
- [x] Decide whether these are stored as aggregate prepared values or computed
  purely at read time
- [ ] Keep the implementation Creature-safe and avoid fake player inventory

Notes:

- Assigned gear summary now feeds spell power and healing power through
  read-time UnitScript hooks in `SpellBaseDamageBonusDone(...)` and
  `SpellBaseHealingBonusDone(...)`. These use aggregate prepared values and stay
  Creature-safe; no player inventory fields are faked.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotVirtualLoadout.h`
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
  - `src/server/game/Scripting/ScriptMgr.h`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- New helper:
  - `modules/mod-living-world/src/service/WorldBotSpellPowerBaseline.h`
- New core hooks:
  - `OnCalculateSpellBaseDamageBonusDone(...)`
  - `OnCalculateSpellBaseHealingBonusDone(...)`
  - implemented as narrow read-time seams around `SpellBaseDamageBonusDone(...)`
    and `SpellBaseHealingBonusDone(...)`

### 8.2 Spell Penetration

Status: `Partial`.

Checklist:

- [x] Decide whether spell penetration is needed for world-bot parity
- [x] If yes, model it against spell resist calculations, not just item scoring

Implementation notes:

- Added a world-bot-only effective-resistance hook in `GetEffectiveResistChance`.
- Assigned `ITEM_MOD_SPELL_PENETRATION` now reduces non-physical victim
  resistance before the existing clamp and level-difference resistance are
  applied. Physical armor remains handled by the armor penetration slice.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
  - `src/server/game/Spells/Auras/SpellAuraEffects.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotSpellPenetrationBaseline.h`
- Likely bundled with a spell-hit / resist hook rather than built alone.

## 9. Armor Penetration

Status: `Partial`.

Checklist:

- [x] Decide whether armor penetration belongs in the world-bot combat target
  fidelity envelope
- [x] If yes, model it in physical damage reduction, not only in item scoring

Implementation notes:

- Added a world-bot-only armor-reduction hook inside `CalcArmorReducedDamage`.
- Assigned armor penetration rating now converts through the same combat-rating
  DBC path used by other bot rating slices, then applies against the WotLK armor
  penetration cap before physical damage reduction is computed.
- `SPELL_AURA_MOD_ARMOR_PENETRATION_PCT` is included for ordinary spell/class
  auras. Auras with explicit equipped-item requirements remain skipped for now
  because world bots still do not expose real player inventory checks.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/StatSystem.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`
  - `src/server/game/Entities/Player/Player.h`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotArmorPenBaseline.h`
- May need a new damage-side hook if we keep this B-style and world-bot-only.

## 10. PvP Durability

### 10.1 Resilience

Status: `Partial`.

Checklist:

- [x] Decide whether world bots should have PvP resilience at all
- [x] If yes, model crit suppression and damage reduction explicitly
- [x] Audit melee, ranged, spell, and mana-drain resilience paths

Implementation notes:

- Added a world-bot-only resilience hook beside the existing player/pet
  resilience application path.
- Assigned resilience and crit-taken ratings now convert through the matching
  `CR_CRIT_TAKEN_*` combat-rating bucket for melee, ranged, and spell paths.
- The runtime adjustment mirrors player ordering: crit chance reduction first,
  then crit damage reduction for critical hits, then generic damage reduction.
- Existing mana-drain resilience calls route through `CR_CRIT_TAKEN_SPELL`, so
  the world-bot hook applies there as well.

Files to touch:

- Existing module side:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
- Likely core seams:
  - `src/server/game/Entities/Unit/Unit.cpp`
  - `src/server/game/Entities/Unit/Unit.h`
  - `src/server/game/Spells/Auras/SpellAuraEffects.cpp`
- Player reference:
  - `src/server/game/Entities/Unit/StatSystem.cpp`

New helper / hook?

- Likely new helper:
  - `modules/mod-living-world/src/service/WorldBotResilienceBaseline.h`
- Existing crit hooks may cover part of the work, but full resilience parity
  probably needs new targeted hooks for damage-side calculations too.

## 11. Gear-System Gap That Blocks Several Stats

### 11.1 Assigned-Gear Summary Coverage

Status: `Missing` for most secondary stats.

Current state:

- Item scoring already notices many secondaries such as dodge/parry/block
  rating, spell power, hit, haste, expertise, armor penetration, and spell crit.
- The aggregate assigned-gear summary still only carries:
  - primary stats
  - health
  - mana
  - armor
  - attack power
  - ranged attack power

Checklist:

- [x] Extend assigned-gear summary to carry the secondaries we actually plan to
  use at runtime
- [ ] Extend virtual-loadout shape only where the runtime can honestly support it
- [ ] Keep scoring-only stats separate from runtime-applied stats until the
  relevant slice exists

Notes:

- Assigned gear now carries runtime-visible buckets for defense/dodge/parry/
  block, hit/crit/haste, expertise, armor penetration, resilience, spell power,
  healing power, mana/health regen, spell penetration, and block value. These
  are summarized and logged, but most are intentionally not applied to combat
  until their dedicated runtime slices below wire them into narrow hooks.

Files to touch:

- Existing:
  - `modules/mod-living-world/src/model/WorldBotAssignedGear.h`
  - `modules/mod-living-world/src/model/WorldBotVirtualLoadout.h`
  - `modules/mod-living-world/src/service/WorldBotAssignedGearService.cpp`
  - `modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp`

New helper / hook?

- No new core hook by itself.
- This is the prerequisite data-model work for many unchecked slices above.

## Recommended Order From Here

If the goal is "feel as close to a real player sheet as possible" while staying
Creature-safe and B-style, the best next order is:

1. `Defense skill / defense rating / crit suppression`
2. `Melee hit`
3. `Expertise`
4. `Spell hit`
5. `Spell power / healing done`
6. `Haste`
7. `Mana regen`
8. `Block value`
9. `Armor penetration`
10. `Resilience`

That order keeps the biggest combat-authenticity gaps ahead of the more niche or
PvP-specific ones.
