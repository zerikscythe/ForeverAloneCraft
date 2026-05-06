# Bot Combat Profiles - Design and Handoff Notes

This document captures the agreed design direction for server-authoritative,
user-overrideable bot combat profiles.

It exists so a future developer or AI agent can continue the work without
reconstructing the plan from chat history.

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
