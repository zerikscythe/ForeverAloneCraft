# Plan: Bot Expansion — Account Bots, Guild Bots, and World Bots

---

## Core Model

The bot system is no longer best described as five hard species of bot.

The real top-level split is:

1. `Account bots`
2. `Guild bots`
3. `World bots`

Everything else is a deployment mode, manager state, or behavior policy layered
on top of one of those three families.

In particular:

- `aggressive`, `cautious`, and `pacifist` are not bot categories
- `raid helper`, `ambient`, `tasking`, and `assigned` are not bot categories
- `hostile/rival` is not a bot family; faction conflict is emergent behavior in
  a two-faction world, not a separate species of bot

If an Alliance bot and a Horde bot cross paths, natural conflict may happen
depending on temperament, task, threat evaluation, and local context. Both
factions can have bots that are passive, evasive, or aggressive.

---

## Ownership Families

### 1. `Account bots`

These are real account-bound `Player` bots.

They represent persistent player-owned or account-linked characters. They use
real inventories, real progression, real player session behavior, and are the
closest thing to a true player surrogate.

Typical traits:

- real `Player` session
- real spellbook / talents / inventory
- persistent identity
- progression can sync to an owning character or owning account model
- when active, remain **fully simulated/rendered** rather than abstracted

### 2. `Guild bots`

These are also real account-bound `Player` bots, but they exist to serve guild
economy and guild presence.

They gather real materials, hold real items, and perform real deposits into the
guild bank. Their economic behavior is not simulated.

Typical traits:

- real `Player` session
- real inventory and profession interaction
- real guild membership and guild bank access
- persistent named characters
- when active, remain **fully simulated/rendered** because their work is real

### 3. `World bots`

These are manager-owned spawned bots that are not account-bound.

They are intended to converge toward the same class-identity and combat-doctrine
model as account bots where practical, but the current live implementation is
still narrower: a server-owned ambient runtime focused on DB-authored sessions,
travel, observability, and first-pass recovery instead of a fully unified
reassignment-capable combat actor.

They may be:

- offline in the manager pool
- online doing world tasks
- temporarily redirected to support a group or dungeon need
- pulled into combat naturally while already out in the world

This means `ambient world bot` and `raid helper world bot` are the same bot
family. The difference is assignment, not species.

---

## Spawn Technologies

There are still two spawn technologies, but they map to ownership and economy,
not to an artificial five-category taxonomy.

### Player-session bots

Used for bot families that require true `Player` behavior:

- real inventory
- real profession skill checks
- real guild bank deposits
- real guild roster presence
- real account persistence

This applies to:

- `Account bots`
- `Guild bots`

Account pools remain separate:

- `BOTHOUSE` for account bot clone/session usage
- `GUILDBOT` for guild bot accounts

These pools must never be mixed.

### Spawned world bots

Used for bots that should appear and behave like players in the world, but do
not need true account persistence or true economic output.

This applies to:

- `World bots`

Target capabilities for world bots:

- move around the world
- travel between tasks
- fight using class-like spell logic
- simulate gathering and deplete nodes
- be reassigned into helper/service roles

But by default they do **not** create real item ownership. If they gather a
node, the node can still be depleted, while no actual item needs to exist in a
bot inventory.

Current implementation note:

- the live slice already covers authored ambient session composition, travel,
  population maintenance, activity logging, and validation tooling
- the full gather/combat/resume loop and broader support-role reassignment are
  still future work
- the newly agreed direction is that offscreen world bots should usually be
  **abstract progression only** rather than always-on full creature simulation

---

## Shared Combat and Ability Layer

One important design correction must remain explicit in this document:

the system is **not** meant to maintain one combat brain for `Player*` bots and
another fake combat brain for spawned world bots.

The design direction is that player-specific combat handling was abstracted so
more of the bot ability pipeline can operate against `Unit*`, not just
`Player*`.

That matters because spawned bots should still be able to use player-like class
abilities and spells in a way that mimics a real player as closely as possible.

### Design intent

- bot doctrine and rotation logic should be shared whenever possible
- spawned world bots should not feel like a lower-fidelity fake AI tier
- differences between account/guild/world bots should mostly be about
  ownership, persistence, and economy
- combat behavior should stay as unified as possible through a `Unit`-driven
  abstraction layer

### Practical meaning

If an ability/rotation/profile system can target `Unit*`, then:

- `Player` bots can use it
- spawned world bots can use it
- both can cast the same class-style spells and follow the same doctrine rules

This is the preferred architecture for believable bot behavior.

---

## World Bots Are Manager-Owned

`World bots` belong to a world bot manager.

They are not personal player property and not guild-owned economic actors.
They are population and service actors controlled by server-side orchestration.

The manager is responsible for:

- deciding whether a bot is currently offline or online
- selecting task chains
- placing bots into the world
- reclaiming them back into the pool
- temporarily reassigning them to support roles

This is why `world bots` should be modeled as a pool of reusable actors rather
than as a collection of unrelated subcategories.

---

## World Bot State Model

World bots are best understood as `type + state`, not as separate types.

### Type

- `world`

### State / assignment

- `offline / pooled`
- `offline / abstract progressing`
- `online / tasking`
- `temporarily assigned`
- `returning to task pool`

### Behavior policy

- `pacifist`
- `cautious`
- `aggressive`

### Economy mode

- `simulated`

Because of this, a world bot helping in a dungeon is still just a world bot.
It was already either offline in the manager pool or online doing tasks, and is
temporarily reassigned when needed.

### Simulation mode boundary

This now needs to stay explicit:

- **world bots in a real player's active zone/town** should be fully spawned and
  simulated
- **world bots outside player-interest areas** should usually advance by
  abstract timers/state instead of full pathing/combat ticks
- once a player leaves, that map/zone should stay **hot for about 10 minutes**
  before nearby world bots are allowed to fall back to abstract progression
- a player merely flying over on a taxi should **not** count as meaningful
  interest for spawning/materialization
- **account bots** stay fully simulated when active
- **guild/workforce bots** stay fully simulated when active because their
  gathering/fishing/crafting work is materially real

Current v1 implementation direction:

- world-bot interest is tracked by map/zone hotness
- hotness refreshes from real player presence and decays on a 10-minute timer
- spawned world bots can dematerialize back into an abstract runtime snapshot
  after the hot timer expires
- offscreen abstract bots only rematerialize when a real non-taxi player is
  actually present in the relevant zone
- taxi-destination preheat is intentionally deferred as a follow-up slice

---

## World Bot Ledger Progression

The persistent `world bot ledger` should also track long-form life cycle state,
not just identity and spawn availability.

For current implementation direction, each `world bot` identity should track:

- `current level`
- `total world-online time`
- whether it is currently in an active world session
- whether it has been retired
- `last seen` location/time

Progression rule:

- each `1 hour` of counted `world bot` online time grants `+1 level`
- this counter is for normal world presence only
- time spent in reassigned `BG` / `dungeon` / service roles should not count
- after reaching max level, the bot may continue to appear for another `24`
  combined counted world hours
- after that it is retired from the active world-bot pool

Retirement is intended to preserve history, not erase it.

The retired ledger row should remain as world history, while a later slice can
generate a fresh replacement identity for the same faction at a low level.

---

## Session and Tasking Model

The intended behavior model is still chained session planning rather than
single-purpose static spawning.

A bot session should be composed from:

- class / spec / faction / level identity
- combat doctrine
- a chained task list

Example world bot session:

1. Travel to zone
2. Patrol road
3. Visit hub
4. Move to gathering area
5. Simulate gathering
6. React to local threats or opportunities
7. Return to route or despawn back to pool

For world bots, this same actor may later be reassigned:

1. Pulled from pool or interrupted mid-task
2. Sent to support a group / dungeon / service request
3. Performs combat role
4. Returns to manager control
5. Either resumes tasks or goes back offline

That reassignment is normal and should be described as state transition, not as
crossing into a different bot category.

### Current abstract/offscreen runtime status

The current live world-bot slice now has a usable first pass of this runtime:

- offscreen sessions can continue through abstract timed progression
- same-map travel can rematerialize from an interpolated in-between position
- fully spawned world bots can snapshot their current task/session state and
  dematerialize back to abstract mode when their zone cools off
- current v1 avoids taxi-flyover false positives by ignoring in-flight players
  for interest/materialization checks

Still intentionally deferred for later:

- taxi destination preheat when a route is selected
- more aggressive distance-based cooldown override (for example, 2-zones-away)
- durable DB persistence of abstract mid-session step state across server restarts

### Direction update: DB-authored task templates and task steps

The current activity-library system is now explicitly a bridge toward a richer
editor-owned task model, not the final authoring shape.

The intended evolution is:

- `living_world_activity_library` remains the low-level activity vocabulary and
  fallback selector surface
- higher-level reusable work chains move into DB-authored task templates, with
  ordered DB-authored task steps
- the server runtime interprets those templates; it does not hardcode each new
  task chain in C++

The first vertical slice for this direction is:

- **Travel -> generic gathering chain**

Examples:

- Travel to zone -> Gather herb (generic)
- Travel to zone -> Gather ore (generic)
- Travel to zone -> Fish (generic)

This gives the editor/database ownership over:

- where a bot should go
- what generic work type it should perform there
- how long it should spend doing it
- which factions / level bands / profession flags are eligible

Future slices should extend the same structure instead of adding one-off
composer logic, for example:

- Travel -> gather specific herb (`Kingsblood`)
- Travel -> gather specific ore
- Travel -> craft item -> repeat N
- guild work orders / profession batches
- city service points like bank / mailbox / AH / inn resolved from DB-authored
  named task points rather than being hardcoded as generic zone anchors

Architectural rule going forward:

- **C++ owns the task runtime/interpreter**
- **DB owns task definitions, ordering, weighting, and authorable work chains**

---

## Economy Split

This is one of the most important boundaries in the system.

### `Account bots` and `Guild bots`

These operate in the **real economy**:

- real items
- real inventory
- real profession outcomes
- real deposits / transfers where applicable

### `World bots`

These operate in a **simulated economy**:

- they can appear to gather
- they can deplete nodes
- they can perform believable world tasks
- but no real item must exist in inventory unless explicitly designed otherwise

This allows high world population without requiring massive account pools or
economic distortion.

---

## Command Namespaces

### `.lwbot roster`

This namespace is for `account bots` only.

It must never query guild bot or world bot pools.

### `.lwbot guild`

This namespace is for `guild bots` only.

It controls persistent guild-function characters and real guild economy work.

### World bot controls

World bots may still have commands for testing, GM control, reassignment, or
service requests, but those commands should be understood as manager controls,
not ownership controls.

The manager owns world bots.

---

## What World Bots Must Preserve From Player Bots

Even though world bots are spawned and non-account-bound, they should preserve
as much of the real player illusion as possible.

That includes:

- class/spec identity
- familiar player spells and ability usage
- believable travel and routing
- reaction to enemies and threats
- doctrine-driven combat behavior
- enough shared behavior with account bots that the difference is hard to spot

What should differ is mainly:

- ownership
- persistence
- inventory reality
- economic side effects

---

## Revised Taxonomy Summary

### Real families

1. `Account bots`
2. `Guild bots`
3. `World bots`

### Not families

- `ambient`
- `raid helper`
- `hostile`
- `aggressive`
- `cautious`
- `pacifist`

Those are assignment modes or behavior policies.

---

## Implementation Direction

### Account bots

Remain real `Player` session bots.

### Guild bots

Remain real `Player` session bots with real economy outputs.

### World bots

Remain manager-owned spawned bots that:

- use the shared combat/ability abstraction
- can task in the open world
- can enter combat naturally while tasking
- can be reassigned into support/service usage
- can simulate gathering while still depleting world nodes

The long-term architecture should continue pushing bot combat and spell logic
toward shared `Unit*`-based systems rather than duplicating special cases for
spawned bots.

First implementation rule for scaling:

- do **not** fully simulate every world bot all the time
- keep offscreen world bots on an abstract progression path
- only materialize full creature AI where a real player can observe/interact

## Current Implementation Snapshot

This document is still the target-model design note, but the repo now contains
enough concrete world-bot work that the current status should be stated
explicitly.

### What is already live

- account-alt bots are the most mature slice and already support real
  player-session lifecycle, combat profiles, talent templates, quest sync, and
  dismissal/recovery behavior
- world bots now have a first-pass ambient runtime with:
  - DB-authored session composition from legacy activities, task templates, and
    playlists
  - named task points plus taxi/transit route support for authored travel
  - population maintenance in `LivingWorldWorldScript`
  - forced-spawn override config for local visual testing
  - session-source metadata (`legacy_activity`, `task_template`, `playlist`)
    surfaced in spawn logs and activity logs
  - dedicated realtime DB viewer tooling with filters, badges, and recent
    activity panes
  - focused helper tests for spawn override and travel-watchdog logic

### What is in progress

- same-map travel watchdog recovery now exists in both `WorldBotCreatureAI` and
  `AmbientBotAI`; the filtered `TravelWatchdogTest.*` rerun and `worldserver`
  rebuild were completed successfully during headless validation
- the current outdoor runtime is still mainly
  `travel -> simulate activity -> advance`, not yet the richer
  `find herb -> travel -> fight if attacked -> gather -> resume` loop
- obstacle handling is improved by watchdog-triggered teleport recovery, but
  broader route recovery and more reactive world behavior are still future work
- the next scaling pass is to make offscreen world bots abstract by default and
  only fully materialize them in player-interest zones/towns
- headless validation also exposed a real session-selection issue: forced spawn
  can currently assign playlists whose early steps belong to other maps

### Immediate next priorities

1. implement abstract offscreen progression for world bots only
2. keep account bots and guild/workforce bots on the always-real/full-sim path
3. add player-interest activation/materialization rules for world bots
4. prevent cross-map forced-spawn/session mismatches
5. extend authored tasking into reactive gather/combat/resume behavior
6. broaden identity seeding with normalized level-range coverage and larger
   race/gender name pools

### Captured follow-up gaps: world-bot combat buildout

The current world-bot combat runtime is now shared enough to drive both
session/account bots and headless/world bots, but the full `spawn flavor ->
complete build` pipeline is still not closed yet. The missing pieces should be
tracked explicitly here so they are not lost between slices:

1. **Align world-bot identity spec keys with canonical doctrine/template keys**
   - legacy identity rows and seed data still use values like `warrior_arms`,
     `paladin_ret`, `mage_frost`
   - seeded default combat profiles and seeded talent templates use canonical
     names like `Arms`, `Retribution`, `Frost`
   - first closure slice: normalize world-bot identities to the canonical keys
     so default profile / talent-template lookup can work predictably

2. **Close coverage gaps between seeded world-bot flavors and seeded defaults**
   - several currently seeded world-bot flavors still do not yet have matching
     default combat-profile / talent-template coverage in the DB vocabulary
   - examples include `Marksmanship`, `Survival`, `Subtlety`, `Enhancement`,
     `Arcane`, `Fire`, `Destruction`, and `Frost` for Death Knight flavor

3. **Add a world-bot spawn/materialization build step**
   - current materialization applies identity presentation/stats only
   - missing step: resolve default combat profile + corresponding talent
     template from bot flavor at spawn/materialization time

4. **Add headless/world-bot talent progression application**
   - the intended model is to store a full max-level talent template per
     class/spec, then spend only the points available for the bot's current
     level in template order
   - by level 80 the build should match the full template
   - deterministic re-application on each spawn/materialization is acceptable
     for this first implementation

5. **Derive world-bot known spells from the resolved build**
   - current world-bot known-spell sourcing is still partial
   - long-term the world-bot spell payload should be derived from
     class/spec/level/build state so combat evaluation sees a believable
     player-like spell set

6. **Refresh the build package when ledger level increases**
   - the world-bot ledger already advances level over time
   - missing step: rebuild talent allocation / usable spell set after level-up
     so progression affects the actual runtime build instead of only the stored
     level field

---

## Critical Clarification For Future Revisions

Do **not** reintroduce a document structure that implies:

- `hostile bots` are a top-level bot family
- `ambient bots` and `raid helper bots` are different species
- spawned world bots require an entirely separate low-fidelity spell system

The intended model is:

- three bot ownership families
- one shared bot behavior/combat direction as much as practical
- world-bot manager ownership for non-account bots
- faction conflict as emergent world behavior
- simulated economy for world bots, real economy for account/guild bots
