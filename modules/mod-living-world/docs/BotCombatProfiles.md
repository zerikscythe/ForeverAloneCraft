# Bot Combat Profiles - Design and Handoff Notes

This document captures the agreed design direction for server-authoritative,
user-overrideable bot combat profiles.

It exists so a future developer or AI agent can continue the work without
reconstructing the plan from chat history.

---

## Current status

This is still primarily a design-direction document, not a "fully landed live
system" description.

Today the broader world-bot stack already has:

- persistent identity
- virtual/assigned loadouts
- player-like stat baseline work
- growing travel realism

But combat depth is still behind travel/system depth.

In practice that means:

- many world-bot class/spec behaviors remain shallow
- this combat-profile/doctrine direction is still one of the largest future
  realism tracks
- it is important, but it is not currently the nearest shipping slice compared
  with city-presence and quest-hub runtime work

---

## 1. Problem Statement

The current bot combat logic in `CompanionAI.cpp` is class/role driven and
mostly hardcoded. That is good enough for the first live bot-party slice, but
it does not provide:

- per-bot profile slots
- player-editable combat rules
- per-spec doctrine
- clean fallback to defaults when the player has not configured anything
- a stable addon-facing editing model

The new system should allow each bot to:

- use a baked-in default combat doctrine chosen by server-side best guess
- optionally apply player-selected spec/role overrides
- optionally use one of 10 profile slots per bot/source character
- execute row-based actions and conditions instead of one giant hardcoded
  switch

The system must remain server-authoritative.

The addon is only a UI + request layer.

---

## 2. Architectural Principles

### 2.1 Server authoritative

The addon does **not** talk directly to the database.

The intended path is:

```text
Addon UI
  -> LivingWorld profile API/messages
  -> worldserver request handler
  -> validation / auth / profile service
  -> repository
  -> database
```

The addon can be described as "updating the database" at a zoomed-out level,
but in implementation terms it sends requests to a worldserver-owned API layer.

### 2.2 Relational storage, not JSON blobs

The agreed design is to store profile state in normalized relational tables,
not a single JSON blob.

Reasons:

- row-level edits are small and addon-friendly
- profile reset/copy/default fallback are easier to reason about
- partial updates do not require shipping a whole blob
- runtime load paths can query strongly typed data directly

### 2.3 Blank profile means baked-in defaults

If the player never customizes a profile, the bot should still work.

Therefore:

- a blank profile is equivalent to "use server default doctrine"
- defaults are resolved from the effective spec/role
- player data should only store overrides/custom rows

This keeps the no-setup experience simple.

### 2.4 Best guess first, player override second

The server should make a best guess at the bot's effective combat focus using:

1. talent-tree scan where possible
2. class heuristic fallback where needed

Then the player can override it per profile slot with a more specific
`Spec:Role` pair such as:

- `Protection:TANK`
- `Holy:HEAL`
- `Retribution:DPS`

Runtime uses:

- override if present
- otherwise best guess

### 2.5 Start small

V1 default coverage is **one baked-in starter doctrine per class**.

Recommended initial defaults:

- Warrior -> Arms:DPS
- Paladin -> Retribution:DPS
- Hunter -> BeastMastery:DPS
- Rogue -> Combat:DPS
- Priest -> Shadow:DPS
- DeathKnight -> Unholy:DPS
- Shaman -> Elemental:DPS
- Mage -> Frost:DPS
- Warlock -> Affliction:DPS
- Druid -> Balance:DPS

Later work can add healer/tank/specialized variants.

---

## 3. Execution Model

The runtime combat profile engine should conceptually behave like this:

```text
1. Evaluate interrupt rules
   - these may break the current cast

2. Apply conservation gate
   - Full Force
   - Conservative
   - JIT Casting

3. Walk normal rotation rows by priority
   - each row has primary and optional secondary action

4. For each row:
   - if primary usable now -> use it
   - else if primary usable within next tick (500ms) -> wait
   - else try secondary
   - if secondary usable now -> use it
   - else if secondary usable within next tick (500ms) -> wait
   - else skip row

5. Resolve target and optional down-rank before final execution
```

### 3.1 Tick timing rule

Current agreed tick expectation is 500ms.

Primary/secondary fallback uses that same window.

If the server knows the primary action is ready by the next tick, the bot waits.
Otherwise it attempts the fallback.

### 3.2 Account bots vs world bots

The combat runtime is intentionally **shared** between:

- account/session bots backed by real `Player` objects
- non-account/world bots backed by `Creature` objects

The important split is **not** the doctrine or row executor. The split is in how
the bot arrives at the runtime:

- session/account bots build their known-spell set from `Player::GetSpellMap()`
- world bots build a synthetic player-like known-spell set from:
  - creature spell slots
  - doctrine/profile action spells
  - spell-chain rank resolution
  - level gating

After that, both paths converge on the same layers:

- `BotCombatDoctrineResolver`
- `BotCombatProfilePreparationService`
- `BotCombatRuntimeEvaluator`

So world bots should be thought of as using a **modified Player-style combat
preparation flow**, not as a wholly separate combat engine. Creature-specific
helpers such as `Creature::CanCastSpell(...)` are only one final feasibility
check inside that broader shared runtime path.

---

## 4. Conservation Modes

### 4.1 Full Force

- no resource conservation
- cast the best available eligible action

### 4.2 Conservative

- offensive casting is gated by low/high water marks
- example doctrine:
  - if mana falls below low-water threshold, stop offensive magic
  - if mana rises above high-water threshold, resume offensive magic
- defensive/heal/cleanse/buff actions can still remain eligible

### 4.3 JIT Casting

- no offensive magic assistance
- only buffs, cleanses, healing, and similar support actions
- intended mostly for healers/support modes

Thresholds should be data-driven per profile/default doctrine, not hardcoded in
the addon.

---

## 5. Down-Ranking

Optional feature.

If enabled:

- when the desired spell rank cannot be afforded
- walk the rank chain downward
- stop at the configured floor
- cast the best lower rank that is currently affordable

This is especially useful for healing/support profiles.

---

## 6. Targets

The profile system needs stable target concepts, not only raw unit pointers.

### 6.1 Standard targets

- `self`
- `owner`
- `enemy`
- `lowest_hp_party`
- `role:tank`
- `role:healer`
- `named:<CharacterName>`

### 6.2 AoE targets

- `aoe_centroid`
- `aoe_feet`

AoE rows can also carry:

- minimum target count
- scan radius
- AoE mode override

### 6.3 Group-combat specific targets

#### `enemy_primary`

Intended focus/kill target.

Resolution order:

1. tank victim
2. owner victim
3. bot victim

#### `enemy_trash`

Non-primary hostile target near the tank/owner.

Preferred resolution priority:

1. hostile add attacking healer
2. hostile add attacking owner
3. hostile add attacking another party member
4. nearest valid non-primary hostile near the group

These target concepts are especially important for dungeon and add-control
behavior.

---

## 7. Row Authoring Model

The addon editor is expected to be row-based.

Example conceptual row:

```text
[Greater Healing Wave] [none]
target: [owner]
if [owner] [hp_pct] [<=] [60]
interrupt: [yes]
break_cast: [yes]
```

Normal rotation row:

```text
primary:   [Blizzard]
secondary: [Frostbolt]
target:    [enemy_primary]
if [nearby_enemies] [>=] [2]
```

Each row should support:

- enabled/disabled
- priority
- label
- interrupt vs normal rotation
- break-cast flag
- primary action
- optional secondary action
- one or more conditions
- AND/OR condition logic at the row level

Useful condition keys supported by the current runtime include:

- `hp_pct`
- `mana_pct`
- `power`
- `power_pct`
- `runic_power`
- `runic_power_pct`
- `distance`
- `aura` / `has_aura`
- `aura_stacks`
- `aura_remaining_secs`
- `combo_points`
- `threat_pct`
- `is_aggro_holder`
- `party_members_below_hp_pct`
- `nearby_enemies` (`numericValue` = enemy count threshold, `stringValue` = scan radius)
- `runes_ready` / `runes_available` (`numericValue` = ready-rune threshold, `stringValue` = `blood|frost|unholy|death|any`)

### 7.1 Smart profile adjustments inside one spec/profile

The current model is already capable of handling a large class of
"smart-adjustment" behavior inside a **single** spec/profile.

That is the preferred direction for rotations such as:

- Death Knight single-target vs AoE
- Mage single-target vs cleave
- healer triage vs group-heal branches

The recommended authoring model is:

- keep one canonical profile for `Spec + Role (+ Context)`
- encode alternate branches as separate priority rows
- gate those rows with conditions such as:
  - `nearby_enemies`
  - `party_members_below_hp_pct`
  - `aura_remaining_secs`
  - resource/rune/runic-power thresholds
- optionally use row/action AoE metadata (`aoe_mode`, `aoe_min_targets`, `aoe_radius`)

That means the runtime can swap behavior **without changing profile identity**.
For example, a Frost DK PvE profile can contain:

- single-target rows for disease maintenance, Obliterate, Frost Strike
- AoE rows for Howling Blast / pestilence-style behavior
- conditions such as `nearby_enemies >= 2` or `>= 3`

In practice, the row conditions act like the mode-switch flag.

This is preferable to splitting one spec into separate "single-target profile"
and "AoE profile" records unless the behavior is so different that maintenance
becomes unmanageable.

### 7.2 Current schema/runtime fit for smart switching

The current storage layout already supports this style reasonably well:

- profile-level defaults:
  - `default_aoe_mode`
  - `default_aoe_min_targets`
  - `default_aoe_scan_radius`
- per-action overrides:
  - `aoe_mode`
  - `aoe_min_targets`
  - `aoe_radius`
- per-entry condition lists:
  - `subject_key`
  - `stat_key`
  - `comparison`
  - `numeric_value`
  - `string_value`

So the present model can already express:

- "use AoE action when at least N enemies are nearby"
- "use single-target action otherwise"
- "refresh disease/buff/debuff only when aura time is low"
- "switch to spender only when runic power / combo points / mana state allows it"

The likely next design question is not whether the model can switch modes at
all, but whether we later want explicit authoring affordances such as:

- named rotation branches (`SingleTarget`, `AoE`, `Execute`, `EmergencyHeal`)
- branch-level debugging/telemetry
- branch-level editor grouping in the addon/UI

Those would be ergonomics improvements, not proof that separate profiles are
required.

---

## 8. Database Schema Concepts

The exact SQL has not been finalized yet, but the intended relational shape is:

### 8.1 Default doctrine tables

Server-maintained baked-in defaults.

- `lwb_default_profile`
- `lwb_default_entry`
- `lwb_default_action`
- `lwb_default_condition`

These hold one starter doctrine per class/spec/role.

### 8.2 Player profile tables

Player-editable data.

- `lwb_profile`
- `lwb_profile_entry`
- `lwb_profile_action`
- `lwb_profile_condition`

Each source character gets 10 slots.

Profile-level fields should include at least:

- source character guid
- slot number
- profile name
- guessed spec
- guessed role
- optional spec override
- optional role override
- conservation mode
- mana low-water mark
- mana high-water mark
- down-rank enabled
- down-rank floor
- default AoE mode
- default AoE min-target count
- default AoE scan radius

### 8.3 Runtime linkage

The account-alt runtime row should eventually carry:

- `active_profile_slot`

so the active bot session knows which player profile slot to use.

### 8.4 Current practical takeaway for Icy Veins-style rotations

For guide-driven rotations sourced from places like Icy Veins, the current
system should be treated as:

- one profile per spec/role/context,
- many rows within that profile,
- condition-driven switching between rotation branches.

In other words, if a guide says:

- "single target do X"
- "on 2+ targets do Y"
- "refresh disease at low remaining time"
- "dump runic power above threshold"

the first implementation target should be **more rows and better conditions**,
not more profile identities.

---

## 9. Blank Profile vs Custom Profile Rules

Agreed v1 behavior:

- blank profile -> use default doctrine for effective spec/role
- non-blank profile -> use custom rows/settings

One open implementation choice remains:

- whether a custom profile fully replaces defaults
- or whether defaults are merged with explicit player overrides

V1 can safely start with full replacement for non-blank profiles if that keeps
runtime simpler.

---

## 10. Addon/API Responsibilities

### Addon responsibilities

- display profile slots
- display guessed and overridden spec/role
- add/edit/remove rows
- move priorities
- edit conditions and actions
- request reset/apply/select-active-profile

### Server responsibilities

- validate every edit
- resolve spell/item identity
- persist row data
- compute best-guess spec/role
- load default doctrine when needed
- resolve targets at runtime
- enforce cooldown/wait/fallback timing
- perform actual execution

The addon should never be the source of truth for executable runtime state.

---

## 11. First Groundwork Implementation Order

The implementation should begin in this order:

1. add handoff-quality docs (this file)
2. add shared model enums/types for combat profiles
3. add repository interfaces for profile/default-profile reads and writes
4. add spec/role resolver interface/service
5. add SQL schema and seed-data scaffolding
6. add profile load service
7. add runtime row executor
8. wire profile selection into bot runtime
9. expose server API/message layer
10. build addon editor against that API

---

## 12. Current Groundwork Targets

The next agent should expect the first code scaffolding to introduce:

- profile model types under `src/model/`
- repository contracts under `src/integration/`
- later services under `src/service/`

The first code pass does **not** need to implement final SQL or runtime
execution yet. It should establish names, ownership, and boundaries cleanly.

---

## 13. Open Decisions Still Worth Tracking

These are known but intentionally left open while groundwork lands:

1. exact SQL schema and enum encoding
2. full-replacement vs merged override semantics
3. precise starter default doctrine per class
4. exact condition vocabulary and row-level OR/AND encoding
5. how much runtime preview/debug state should be returned to the addon

Those decisions should be made in implementation docs / PR notes as they land.
