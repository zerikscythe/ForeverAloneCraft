# World Bot Virtual Loadouts - Evolved Design Notes

## Purpose

This document started as the agreed first slice for world-bot gear support. It
now also records how that first slice evolved into the current staged,
bagless-but-item-grounded world-bot gearing model.

It also now needs to be read with a newer pivot in mind:

- abstract/offscreen world bots still use the ledger + assigned-gear model as
  canonical truth
- visible/materialized ambient bots are pivoting toward leased `Player` shells
  rebuilt from that ledger truth
- the older `Creature`-backed runtime described throughout this document is
  still relevant historically and as a compatibility lane, but it is no longer
  the intended long-term visible body for ambient bots

World bots are `Creature`-backed actors, not real `Player` inventory owners. They
should still be able to scale in survivability and power like geared players,
but the first slice should not fake more of the item system than the runtime can
honestly support today.

## Decision Summary

Historical note: the original decision summary below explains why virtual
loadouts and invisible assigned gear were the right answer for the
`Creature`-backed phase. That work still matters because the ledger and
assigned-gear tables now feed player-shell rebuilds too.

We still are **not** implementing real inventory ownership or true visible
equipped item instances for world bots.

Instead, the live model is now:

- DB-backed virtual loadout packages
- persistent invisible assigned gear
- curated staged level-80 templates
- materialized stat/rating/passive item effects
- common reactive proc support where the current runtime can honestly carry it

## Cross-family implication

This slice starts in the world-bot path because world bots currently have the
largest survivability gap, but the broader design goal is not world-only.

Account bots, guild bots, and world bots may all eventually be assigned into
shared dungeon/raid/PUG contexts. That means survivability and throughput tuning
must ultimately converge on a shared build/loadout direction even if the runtime
delivery differs by bot family.

Current interpretation:

- **ledger-shell ambient bots** use the same assigned-gear truth, but project it
  into real player inventory/equipment at rebuild time
- **older creature-backed ambient bots** still use virtual-loadout +
  aggregate-stat projection because they are bagless and compatibility-oriented
- **account bots** and **guild bots** already have a real inventory/equipment
  path, but should eventually share the same higher-level build/loadout
  selection vocabulary for PUG readiness
- later slices should move toward **context-aware loadout resolution** so the
  same bot identity can prepare differently for solo world play vs dungeon/raid
  PUG participation

## World-bot progression implication

Virtual loadouts are also the intended mechanism for post-80 world-bot
progression.

- level progression should stop at `80`
- endgame **gear progression should continue after 80**
- a newly capped bot now starts at the curated stage-0 **pre-raid** rung when a
  staged template exists
- continuing counted world-online time should move that bot through stronger
  endgame survivability / throughput packages as later stages are seeded
- the final hours before retirement are still intended to represent the bot's
  near-BiS / fully maxed phase once stages `1-4` are fully authored

This keeps long-lived level-80 bots from freezing at one power band and better
matches the way a real endgame player keeps progressing after the leveling game
ends.

## Why this is the right first slice

- keeps world bots bagless
- reuses the existing `loadoutKey` / `gearTier` identity concepts
- stays compatible with the current `Creature` runtime
- improves survivability and throughput immediately
- avoids pretending that `Creature` units already support the full player item
  stat model

## Current safe stat boundary

The current AzerothCore `Creature` stat system can safely honor:

- primary stats written directly to the unit fields
  - strength
  - agility
  - stamina
  - intellect
  - spirit
- creature unit modifiers that feed into recalculation
  - bonus health
  - bonus mana
  - bonus armor
  - bonus attack power
  - bonus ranged attack power

These are applied explicitly and then `UpdateAllStats()` is run again.

## Explicitly deferred from V1

The following were originally deferred because the world-bot runtime is still
`Creature`-based and does not yet share the full player item bonus pipeline.
Many of them are now live in a bagless/materialized form:

- spell power / healing power
- resilience / PvP durability semantics
- hit / crit / haste / expertise / armor penetration rating pipelines
- gem / enchant / armor-kit / set-bonus modeling
- glyph aura materialization
- common equip-aura and landed-hit reactive proc support

The still-deliberate deferrals are now narrower:

- visible equipment / true inventory ownership
- full player item-slot lifecycle semantics
- exhaustive weird-item / strange on-use script fidelity
- full player pet/inventory interactions for every item edge case

If we need those later, the next slice should either:

1. add explicit world-bot-only combat coefficients, or
2. move toward a deeper invisible-equipment model.

## Resolution model

V1 uses `living_world_bot_virtual_loadout` in the world DB.

Lookup inputs:

- `class_id`
- canonical `specKey`
- `loadoutKey`
- `gearTier`

Resolution priority:

1. exact `loadoutKey`
2. generic blank `loadoutKey`
3. exact `specKey`
4. generic blank `specKey`

This mirrors how combat profile / talent-template variant resolution already
works elsewhere in the module.

## Runtime contract

- `WorldBotPreparationService` resolves the virtual loadout package.
- `WorldBotPreparedBuild` carries the resolved package.
- `WorldBotCreatureAI::ApplyIdentityToCreature()` applies the safe subset of
  bonuses at spawn.
- `build_prepared` activity logging includes the resolved virtual loadout name
  and bonus summary.

For the newer ledger-shell lane:

- the same assigned-gear truth is rebuilt into real equipped player items
- display loadouts and appearance bytes ride alongside that rebuild
- the creature-side aggregate application path should now be treated as a
  fallback/legacy consumer of the same canonical gear data

## Current seed shape

The initial normal-loadout seed should be read in three layers:

- **class fallback rows** for `gear_tier` `1..3` so existing identities with
  questing / dungeon / raid bands always resolve some package
- **spec-specific normal rows** where we want more believable stat emphasis for a
  canonical spec without requiring an explicit `loadout_key`
- **explicit loadout-key rows** for finer variants that need to override the
  spec-normal package (for example, a cat-focused feral package)

That gives the system a safe default for every existing world-bot identity while
still leaving room for richer build variants later.

## V1.5 invisible assigned gear slice

The next live evolution of this system is **not** to give world bots true player
inventory ownership. Instead, the project now starts a middle layer between pure
virtual stat packages and full item ownership:

- each world bot can carry a persistent **invisible assigned gear set**
- the set is stored as **per-slot item IDs** in the characters DB
- the assigned set is regenerated only when the bot crosses a configured
  **gear refresh band**
- the refresh is deferred until the bot's **next spawn/materialization**, rather
  than hot-swapping gear while the bot is already active

### Current live intent of the assigned-gear slice

- level progression should mark bots for a gear refresh at **5-level bands**
- on the next spawn, the system queries real `item_template` rows for suitable
  equipment candidates by:
  - slot / inventory type
  - class allowance
  - preferred armor family
  - weapon subclass compatibility
  - required level
  - quality band
- sub-80 refreshes still use filtered/scored real item candidates with a level
  and quality curve
- level-80 bots now prefer **curated staged templates** when present, starting
  from stage-0 pre-raid and later falling back to generated gear only where
  staged content is still missing

### Why this exists

This gives world bots a believable gearing path that is grounded in actual item
data without forcing Creature-backed bots to become real item owners.

It also creates a durable bridge toward future features:

- bot inspection/editor tooling can show the bot's invisible assigned items
- future slices can improve filtering/scoring without replacing the data model
- account/guild/world bots can converge on a shared higher-level **build + gear
  assignment vocabulary** even if their runtime delivery differs

### Runtime contract for the assigned-gear slice

- identity progression marks `gear_refresh_pending` when a bot crosses a gear
  refresh band
- `living_world_bot_assigned_gear` stores the invisible slot assignments
- on spawn/materialization, the assigned gear service ensures a set exists and
  refreshes it when needed
- the service aggregates item-derived stats into the same Creature-safe stat
  subset already used by virtual loadouts
- `WorldBotCreatureAI::ApplyIdentityToCreature()` applies:
  1. player-like base stat baselines for class/race/level where safely available
  2. virtual loadout package bonuses
  3. assigned-gear aggregate bonuses

## V1.6 player-like stat baseline slice

The next refinement after invisible assigned gear is to make world bots start
from a **player-authentic baseline** instead of whatever arbitrary primary-stat
or resistance baseline happens to exist on the underlying creature template.

### New runtime intent

- class/level **base health** comes from `PlayerClassLevelInfo`
- class/level **base mana** comes from `PlayerClassLevelInfo` for mana users
- race/class/level **primary stats** come from `PlayerLevelInfo`
- baseline **armor** starts from the normal player convention of `agility * 2`
- non-physical resistances are reset to `0` like a fresh player baseline
- after that baseline is applied, the existing additive layers still stack on top:
  - virtual loadout bonuses
  - assigned-gear summary bonuses

### Why this matters

This moves world bots closer to "built like a player" in the areas players most
immediately feel:

- health pools
- mana pools
- primary stats
- armor baseline
- resistance baseline

without yet requiring the full player item/rating/talent derivation pipeline.

### Still deliberately deferred after V1.6

This slice still does **not** claim full player-stat fidelity. It does not yet
rebuild or model:

- full talent-derived stat modifiers
- player rating pipelines (crit/hit/haste/expertise/armor pen/resilience)
- authentic melee/ranged damage formulas
- dodge/parry/block/crit derived player percentages
- non-mana power-pool authenticity for rage/energy/runic power
- spell-power coefficient fidelity
- proc auras, gems, enchants, set bonuses, or visible equipment

## V1.7 prepared passive-spell application slice

After the player-like class/race/level baseline, the next safe realism step is
to apply the subset of **prepared known spells** that behave like passive or
self-held aura effects.

### New runtime intent

- world-bot preparation already derives a `knownSpellIds` set from:
  - player create info
  - class skill-line spells
  - allocated talent spells
  - combat-profile referenced spells
- on spawn/materialization, the runtime now auto-casts only the safe passive
  subset of those spells:
  - passive spells
  - certain do-not-display stance/self-aura spells that are valid while not
    shapeshifted

### Why this matters

This closes part of the gap between:

- "the bot knows the talent/class passive on paper"
- and "the passive aura/stat effect is actually live on the spawned unit"

That is especially important for talent-granted self-buffs and class passives
that affect throughput, survivability, or mana economy.

### Scope boundary for this slice

This slice still intentionally avoids pretending the world bot has a full player
spellbook/runtime lifecycle. It applies the safe passive/self-aura subset at
spawn time; it does not attempt to fully emulate:

- form-gated aura lifecycle beyond simple safe checks
- full proc systems
- weapon/item passive ownership semantics
- all stance/shapeshift transitions

### Deliberate limitations of the current slice

This live slice still does **not** model:

- real visible equipment on the creature model
- actual bag ownership or lootable personal inventory

Those older gaps have otherwise narrowed substantially:

- enchantments, gems, armor kits, and set bonuses are now materially applied
- full rating-to-combat pipeline fidelity is no longer a blanket missing slice;
  most major combat-relevant rating lanes are now implemented through the
  current Creature-safe hooks

So the assigned gear is currently best understood as:

> **persistent invisible item assignment used to derive believable Creature-safe
> stats**

## V1.8 player-valid talent allocation slice

After passive self-aura application, the next safe authenticity step is to make
the **prepared talent build itself** obey player-valid talent-tree gating rules
instead of just spending points in raw template order.

### New preparation intent

- world-bot preparation still starts from DB-authored talent-template priority
  order
- but talent point spending now respects the same core constraints a player does:
  - talent must belong to the bot's class tree
  - higher-row talents require enough prior points in the same tab
  - prerequisite talents require the needed invested rank first
  - ranks are filled progressively across repeated passes rather than dumping all
    points into a single entry in one shot
- the prepared known-spell set now also includes additional talent-learned spells
  exposed through `SPELL_EFFECT_LEARN_SPELL` when AzerothCore marks them as
  additional talent spells

### Why this matters

This closes an upstream authenticity gap before spawn-time passive application
even runs:

- previously, a template could allocate a final-looking rank mix that a real
  player build could not actually reach yet
- that distorted which passive auras, talent-granted spells, and profile spell
  assumptions became live on the spawned world bot

With this slice, world bots are still not full Player objects, but their
prepared talent layout is now much closer to a legal player build.

### Scope boundary for this slice

This still intentionally avoids full player-spec simulation. It does **not** yet
attempt to model:

- talent reset / respec economics
- dual-spec runtime switching
- glyph interactions
- all player-side spellbook persistence semantics
- rating-derived combat outcomes that would require the broader player stat
  pipeline

## V1.9 player-like attack power baseline slice

After legalizing talent preparation, the next safe runtime-fidelity step is to
stop relying entirely on generic `Creature` attack-power baselines.

### New runtime intent

- world bots now derive a player-like **baseline melee attack power** from:
  - class
  - level
  - strength
  - agility where relevant
- world bots now derive a player-like **baseline ranged attack power** from:
  - class
  - level
  - agility
- virtual loadout and assigned-gear AP bonuses remain additive layers on top of
  that baseline

### Why this matters

Before this slice, world bots could look more player-like in health, mana,
primary stats, and passive auras, but still inherit the generic Creature AP
derivation path.

That especially hurts authenticity for:

- warrior / paladin / death knight melee throughput
- hunter ranged throughput
- rogue / shaman / hunter physical scaling

This slice keeps the same Creature-safe model while making the baseline combat
math meaningfully closer to a player character.

### Scope boundary for this slice

This still does **not** attempt to fully reproduce:

- full player weapon damage formulas
- feral-form-specific attack power rules
- crit / hit / haste / expertise / armor pen rating pipelines
- exact derived combat percentages
- full resource-authentic non-mana opener behavior

## V1.10 player-like power-pool spawn defaults

After improving attack power baselines, the next small authenticity win is to
make world-bot **spawn resource defaults** match player expectations more
closely.

### New runtime intent

- mana users still spawn at full mana
- energy users still spawn at full energy
- rage users now spawn at `0` rage instead of full resource
- death knights now spawn at `0` runic power instead of full resource

### Why this matters

Before this slice, the world-bot spawn path refilled the active power type to
its maximum unconditionally. That is especially unrealistic for:

- warriors spawning with a full rage bar
- death knights spawning with full runic power

This slice improves opener authenticity without requiring a full combat-state or
player-session lifecycle simulation.

### Scope boundary for this slice

This still does **not** attempt to fully model:

- rune state fidelity beyond the existing Creature-safe approximation
- out-of-combat rage decay/history
- pre-combat resource carryover from prior fights
- rogue/druid combo-point persistence semantics

## V1.11 player-like physical damage baseline

After aligning attack power and spawn resource defaults, the next safe realism
step is to stop inheriting the generic `Creature::SelectLevel()` weapon-damage
seed for every world bot.

### New runtime intent

- world bots now replace the shared creature-template base weapon damage with a
  player-like baseline seed before `UpdateAllStats()` runs
- the new seed is intentionally narrow:
  - it mirrors the player no-weapon `1..2` baseline damage range
  - it is scaled by the bot's current attack time so the downstream Creature
    damage formula preserves that player-like base instead of doubling or
    distorting it
- the existing player-like attack power baseline, virtual loadout bonuses, and
  assigned-gear AP bonuses still flow through the normal Creature damage update
  path on top of that seed

### Why this matters

Before this slice, world bots could have player-like stats, talents, passives,
attack power, and resource defaults while still inheriting arbitrary weapon base
damage from the shared world-bot creature template.

That meant physical damage still depended partly on generic creature seeding
instead of a player-authentic baseline.

This slice removes that leftover template influence without claiming full player
weapon or inventory fidelity.

### Scope boundary for this slice

This still does **not** attempt to fully reproduce:

- real item-derived weapon damage ranges
- ammo / wand / thrown-weapon semantics
- dual-wield weapon heterogeneity
- normalized weapon attacks vs true equipped-weapon formulas
- full visible-equipment or inventory-backed weapon ownership

## V1.12 player-like defensive combat baselines

After replacing the leftover creature weapon-damage seed, the next safe combat
authenticity step is to stop using fully generic creature dodge/parry/block
chances for world bots.

### New runtime intent

- world bots now override melee-outcome defensive chances at roll time through a
  `UnitScript` hook instead of trying to write player-only percentage fields
  onto a `Creature`
- dodge is adjusted toward a player-like class/level/agility baseline
- parry is adjusted toward player-like class eligibility rather than generic
  humanoid-creature defaults
- block is adjusted toward player-like shield/class eligibility when the bot's
  assigned offhand item is actually a shield

### Why this matters

Before this slice, a cloth caster world bot and a shield-bearing warrior world
bot could still inherit the same generic creature 5% block/parry/dodge-style
defaults simply because the underlying runtime was Creature-backed.

This slice keeps the implementation Creature-safe while making melee defense
outcomes materially closer to believable player archetypes.

### Scope boundary for this slice

This still does **not** attempt to fully reproduce:

- full defense-skill / defense-rating pipelines
- diminishing returns from bonus defense/rating sources
- exact weapon-presence requirements for parry beyond class-safe approximation
- player character-sheet percentage fidelity
- avoidance interactions from the full item/rating ecosystem

## V1.13 player-like critical strike baseline

After aligning defensive melee outcomes, the next narrow combat-fidelity step is
to stop relying on the generic creature 5% crit baseline for world bots when
they attack.

### New runtime intent

- world bots now adjust melee crit chance at roll time through the same
  Creature-safe combat hook path used for defensive rolls
- crit is derived from the player class/level/agility DBC data that players use
  for base melee crit
- victim-side modifiers such as resilience, defense-skill effects, and other
  existing runtime adjustments remain in place; the slice only replaces the
  generic creature base portion

### Why this matters

Before this slice, world bots could have player-like stats, attack power,
damage, dodge/parry/block behavior, and talents while still opening combat from
the same generic creature crit baseline.

This slice improves physical combat identity further, especially for agility-
driven classes whose crit profiles differ noticeably from flat creature values.

### Scope boundary for this slice

This still does **not** attempt to fully reproduce:

- spell crit fidelity for caster schools
- item/rating-driven crit pipelines beyond existing runtime modifiers
- full crit suppression / resilience parity auditing
- per-weapon or per-spec specialized crit rules beyond the class/level/agility base

## V1.14 player-like spell critical strike baseline

After aligning melee crit, the next narrow caster-authenticity step is to let
world bots use a player-like spell-crit baseline instead of the generic
Creature-backed non-player spell behavior.

### New runtime intent

- world bots now override spell-done crit chance at read time through a narrow
  script hook instead of trying to force player-only crit fields onto a
  `Creature`
- spell crit is derived from the same player class/level/intellect DBC data used
  by players for base caster crit
- the implementation replaces only the generic creature base portion while still
  preserving downstream AzerothCore handling such as:
  - victim-side resilience / crit suppression already applied in
    `SpellTakenCritChance`
  - non-player aura-based crit bonuses already accumulated on the attacker
  - school-specific spell-crit aura modifiers
- the hook also bypasses AzerothCore's normal "ordinary creatures cannot crit
  with spells" guard for world bots only

### Why this matters

Before this slice, world bots could have player-like intellect, talents,
passives, mana, and physical combat identity while still being unable to express
player-like caster crit behavior through the normal creature spell path.

This slice keeps the implementation Creature-safe while making caster world bots
feel materially closer to real player archetypes.

### Scope boundary for this slice

This still does **not** attempt to fully reproduce:

- spell-power / caster-throughput modeling
- item/rating-driven spell crit pipelines beyond existing runtime modifiers
- full player caster stat-sheet parity
- deeper proc / trinket / gem / enchant / set-bonus caster modeling

## Future expansion path

When the project is ready, add a V2 slice for:

- spell-power / caster-throughput modeling
- resilience / PvP-specific durability modeling
- optional invisible equipment-slot modeling
- loadout editing/viewing tools in the editor UI
- context-aware loadout resolution for party / PUG / raid / battleground use
