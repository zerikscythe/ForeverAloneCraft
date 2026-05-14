# Plan: Bot Expansion — Account Bots, Guild Bots, and World Bots

---

## Core Model

The bot system is no longer best described as five hard species of bot.

The real top-level split is:

1. `Account bots`
2. `Guild bots`
3. `World bots`

Everything else should be treated as a separate axis layered on top of one of
those three families, not as a fourth or fifth species.

The document should stay explicit that these are different kinds of decisions:

- **ownership family**
  - `Account bots`
  - `Guild bots`
  - `World bots`
- **spawn/runtime technology**
  - player-session `Player` runtime
  - manager-owned spawned world-bot runtime
- **simulation mode**
  - fully materialized / fully simulated
  - abstract / offscreen progression
- **assignment / duty**
  - ambient
  - tasking
  - raid helper
  - service/support
- **behavior policy**
  - aggressive
  - cautious
  - pacifist
  - other temperament/personality variants
- **faction-contact stance**
  - cooperative / neutral / evasive / hostile depending on faction, personality,
    task, threat, and local context

That separation matters because many previous descriptions mixed together:

- who owns the bot
- how the bot is spawned
- whether the bot is currently abstract or materialized
- what the bot is currently assigned to do
- how the bot tends to react when encountering players or danger

Those are related, but they are not the same classification layer.

In particular:

- `aggressive`, `cautious`, and `pacifist` are not bot categories
- `raid helper`, `ambient`, `tasking`, and `assigned` are not bot categories
- `materialized`, `abstract`, `offscreen`, and `spawned` are runtime states or
  delivery modes, not bot families
- `hostile/rival` is not a bot family; faction conflict is emergent behavior in
  a two-faction world, not a separate species of bot

If an Alliance bot and a Horde bot cross paths, natural conflict may happen
depending on temperament, task, threat evaluation, and local context. Both
factions can have bots that are passive, evasive, or aggressive.

The practical contract for the rest of this roadmap is:

- first decide the bot's **family**
- then decide its **runtime technology**
- then decide whether it is **materialized** or **abstract** right now
- then decide its **assignment** and **behavior policy**

That gives a stable taxonomy and avoids reintroducing fake categories later in
the document.

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

## Combat Orchestration Layers

Another design correction now needs to stay explicit in this document:

believable combat is **not** just `Single Target rotation` vs `AoE rotation`.

The current direction is to treat combat as three cooperating layers:

1. **Build assignment**
2. **Action doctrine**
3. **Navigation doctrine**

### 1. Build assignment

This answers:

- what canonical spec the bot is
- which loadout / variant it is trying to follow
- which combat profile was resolved
- which talent template was resolved

This is where `spec`, `loadout_key`, default combat profiles, talent templates,
and prepared spell/talent packages belong.

Additional world-bot note:

- V1 world-bot gear support should resolve a **virtual loadout stat package**
  during build assignment rather than trying to fabricate true inventory/equipped
  item ownership on Creature-backed bots
- this first slice is intentionally limited to a Creature-safe bonus set
- the next live refinement is **persistent invisible assigned gear**:
  - store real per-slot item IDs for world bots in the characters DB
  - regenerate that invisible set on level-band gear milestones instead of every
    spawn
  - defer the refresh until the bot's next spawn/materialization so active bots
    are not mutated in place
  - aggregate those assigned items back into the same Creature-safe stat subset
    used by world-bot loadouts
  (primary stats, health/mana, armor, physical attack power)
- player-only gear semantics such as resilience rating, spell power pipelines,
  proc effects, gems, enchants, and set bonuses remain deferred to later slices

Broader system note:

- this loadout direction should not stay world-bot-only forever
- account bots, guild bots, and world bots may all eventually participate in
  shared dungeon/raid/PUG assignments
- that means survivability and loadout selection should ultimately converge at
  the **build assignment** layer, even if the implementation differs underneath
  (`Player` equipment vs `Creature` virtual loadout package)

### 2. Action doctrine

This answers:

- what action should be used right now
- whether the current offensive problem is `Single Target` or `AoE`
- which interrupt / utility / resource / emergency action takes priority

This layer owns spell/item choice, conditional entries, and offensive/support
priority resolution.

### 3. Navigation doctrine

This answers:

- where the bot should stand
- whether it should hold, close, kite, retreat, or reposition
- whether the bot is safe enough to stand still for a normal cast/rotation window

This layer owns battlefield posture, distance management, escape pressure, pack
safety, and later PvP maneuvering.

### Orchestration rule

The important rule is:

- **navigation and action logic should stay separate**
- **both should consume the same combat situation state**
- **movement posture constrains which actions are legal or wise**

For example, a caster under melee pressure should not continue a normal hard-cast
rotation just because the best pure-DPS entry says so. Instead:

- navigation says `create distance` / `kite` / `retreat`
- action selection is reduced to movement-safe or emergency actions
- full hard-cast rotation resumes only once a safe stationary window exists

### Required posture model

The old `Single Target` vs `AoE` split is still useful, but it is no longer
sufficient on its own.

Combat should now be thought of as:

- **attack mode**
  - `Single Target`
  - `AoE`
- **movement posture**
  - `Stand Ground`
  - `Reposition`
  - `Kite`
  - `Retreat`
  - `Collapse / Close`
  - `Hold`

Believable behavior comes from the combination, for example:

- `Single Target + Kite`
- `AoE + Hold`
- `Single Target + Reposition`

not from a flat rotation list alone.

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

- `uninterested`
- `opportunistic`
- `aggressive`
- `coward`

### Economy mode

- `simulated`

Because of this, a world bot helping in a dungeon is still just a world bot.
It was already either offline in the manager pool or online doing tasks, and is
temporarily reassigned when needed.

### Required faction-contact personality model

This now needs to be explicit as a required world-bot identity field rather than
an informal future idea.

Each world bot should carry a `personality` policy used when encountering enemy
faction players or bots nearby:

- `uninterested`
  - continues its current work/tasking unless directly attacked or otherwise
    forced into combat
- `opportunistic`
  - evaluates whether the fight is favorable before engaging
- `aggressive`
  - attacks on sight only when the target is no more than `+3 levels` above the
    bot
  - otherwise falls back to non-suicidal evaluation instead of blindly engaging
- `coward`
  - flees when the opposing target is `-3 or greater above` the bot's level
  - otherwise defends itself if forced into combat

This remains a **behavior policy**, not a separate bot family.

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

### Required target pool size and distribution

The intended mature runtime should strive to maintain an **available pool of
roughly 6,000 world bots**.

Baseline distribution goals:

- `~6,000` total available ledger identities
- `50 / 50` faction split
  - `~3,000 Alliance`
  - `~3,000 Horde`
- levels distributed on a **normalized curve across 1-80** rather than a flat
  uniform spread

Further refinement target:

- use historical medium-population realm faction/class/spec mixes as reference
  data where practical, so the ledger does not feel mathematically balanced but
  socially implausible
- this should influence class/spec proportions after the base 50/50 faction
  split and normalized level distribution are established

This is a **target-state design requirement**, not a claim about the current
implementation.

### Required ledger identity fields

Each ledger bot should ultimately have assigned identity/build metadata covering
at least:

- `name`
- `race`
- `class`
- `spec`
- `loadout / variant key`
- `faction`
- `professions` (target model: two profession assignments where appropriate)
- `combat profile`
- `talent tree / talent build`
- `personality`

The current implementation may still represent some of these in simplified form,
but the target system should treat them as first-class identity/build traits.

For current implementation direction, each `world bot` identity should track:

- `current level`
- `total world-online time`
- whether it is currently in an active world session
- whether it has been retired
- `last seen` location/time

Longer-term persistence goal:

- preserve enough last-known world state that bots can gradually end up
  scattered around the world and later resume from believable prior locations,
  rather than always collapsing back to a tiny set of hub spawns

Progression rule:

- each `1 hour` of counted `world bot` online time grants `+1 level`
- this counter is for normal world presence only
- time spent in reassigned `BG` / `dungeon` / service roles should not count
- after reaching level `80`, character level progression stops but **gear
  progression continues**
- a fresh level-80 world bot should begin its endgame phase at roughly
  **pre-raid** gearing
- during the post-80 lifetime, continued counted world-online time should push
  that bot upward through progressively stronger virtual/effective endgame gear
  tiers, similar to a real player continuing to progress through endgame
- the final `10 hours` before retirement should represent the bot's
  **full endgame gear** period, i.e. its strongest survivability / throughput
  state before the character leaves the pool
- after the post-80 gear-progression window is complete, the bot is retired
  from the active world-bot pool

Retirement is intended to preserve history, not erase it.

The retired ledger row should remain as world history, while a later slice can
generate a fresh replacement identity for the same faction at a low level.

Replacement and initial seeding should eventually be generated from the same
distribution-aware identity pipeline rather than from tiny hand-authored pools.

---

## Session and Tasking Model

The intended behavior model is still chained session planning rather than
single-purpose static spawning.

A bot session should be composed from:

- class / spec / faction / level identity
- loadout / variant assignment
- professions
- combat profile + talent build
- personality-driven faction-contact behavior
- action doctrine
- navigation doctrine
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
- account/session combat doctrine now has a substantial DB-driven runtime for
  interrupt/rotation evaluation instead of purely hardcoded per-class spell
  choice
- companion/runtime combat already has early movement-related slices including
  follow formations, ranged approach/retreat behavior, and hazard escape
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
  - first-pass prepared build assembly through `WorldBotPreparationService`
    covering canonical spec resolution, role resolution, default combat-profile
    lookup, talent-template lookup, level-aware talent allocation, and derived
    known spells
  - optional `loadout_key` / variant support on identities plus profile/template
    variant metadata in the world DB schema
  - first-pass abstract/offscreen runtime with materialization/dematerialization
    boundaries tied to player-interest zones

### What is in progress

- same-map travel watchdog recovery now exists in both `WorldBotCreatureAI` and
  `AmbientBotAI`; the filtered `TravelWatchdogTest.*` rerun and `worldserver`
  rebuild were completed successfully during headless validation
- world-bot build preparation now exists, but the conceptual model is still only
  partially solidified: `canonical spec`, `requested loadout`, resolved combat
  profile, and resolved talent template are present, but a first-class
  `resolved build assignment` concept is still missing
- the current doctrine system already handles `Single Target` vs `AoE`, but the
  next required slice is a separate-yet-aware navigation/posture layer so bots
  can `kite`, `retreat`, `stand ground`, `reposition`, or `collapse/hold`
  without fighting their action lists
- current companion movement logic is still helper-driven rather than a full
  shared navigation doctrine; world-bot combat movement is narrower still and is
  mostly `evaluate action -> fallback chase`
- the current outdoor runtime is still mainly
  `travel -> simulate activity -> advance`, not yet the richer
  `find herb -> travel -> fight if attacked -> gather -> resume` loop
- obstacle handling is improved by watchdog-triggered teleport recovery, but
  broader route recovery and more reactive world behavior are still future work
- offscreen abstract progression now exists in first-pass form, but persistence,
  rematerialization polish, and broader session recovery still need more work
- headless validation also exposed a real session-selection issue: forced spawn
  can currently assign playlists whose early steps belong to other maps

### Immediate next priorities

1. solidify world-bot build assignment so combat profiles and talent templates
   resolve as one coherent prepared build package
2. formalize combat orchestration as `build assignment + action doctrine +
   navigation doctrine`
3. add a first shared combat-situation / posture layer so movement and action
   selection can cooperate instead of competing
4. prevent cross-map forced-spawn/session mismatches
5. extend authored tasking into reactive gather/combat/resume behavior
6. improve world-bot combat movement from simple chase fallback toward role-aware
   spacing, retreat, and regroup behavior
7. broaden identity seeding into a large normalized ledger with stronger
   race/gender/class/spec coverage and richer build metadata

### Captured follow-up gaps: world-bot build assignment and combat orchestration

The current world-bot combat runtime is now shared enough to drive both
session/account bots and headless/world bots, but the full `spawn flavor ->
complete build` pipeline is still not closed yet. The missing pieces should be
tracked explicitly here so they are not lost between slices:

1. **[Partial] Align world-bot identity spec keys with canonical doctrine/template keys**
   - legacy identity rows and seed data still use values like `warrior_arms`,
     `paladin_ret`, `mage_frost`
   - seeded default combat profiles and seeded talent templates use canonical
     names like `Arms`, `Retribution`, `Frost`
   - runtime canonicalization/preparation now exists, so live lookup is no
     longer blocked on raw DB values alone
   - remaining cleanup: normalize old ledger/seed rows so the stored data itself
     matches the canonical vocabulary instead of relying on repair-at-read time

2. **[Partial] Close coverage gaps between seeded world-bot flavors and seeded defaults**
   - several currently seeded world-bot flavors still do not yet have matching
     default combat-profile / talent-template coverage in the DB vocabulary
   - coverage has improved since the earliest draft: healer doctrine seeding,
     additional DPS defaults, Arcane doctrine follow-up, talent-template
     variants, and missing Arcane Mage template repair all landed
   - remaining work is to close the rest of the spec/class/loadout matrix rather
     than assuming the first DPS/healer slices are enough

3. **[Partial] Add a world-bot spawn/materialization build step**
   - this now exists in first-pass form through `WorldBotPreparationService`
     and `WorldBotCreatureAI::SetIdentityAndSession(...)`
   - remaining work: turn the prepared build into a more explicit resolved build
     assignment model and keep it aligned with future reassignment/materialization
     flows

4. **[Partial] Add headless/world-bot talent progression application**
   - the intended model is to store a full max-level talent template per
     class/spec, then spend only the points available for the bot's current
     level in template order
   - level-aware allocation now exists in prepared-build form
   - remaining work is broader lifecycle integration, consistency across
     materialization/reassignment, and future persistence decisions

5. **[Partial] Derive world-bot known spells from the resolved build**
   - current preparation now derives a believable spell set from player-create
     info, class skill lines, allocated talents, and combat-profile actions
   - remaining work is to keep this aligned with the mature prepared-build model
     and any later persistent/runtime spell learning decisions

6. **[Partial] Refresh the build package when ledger level increases**
   - the world-bot ledger already advances level over time
   - a fresh materialization already re-prepares the build using the current
     stored level
   - remaining question: whether mid-session/offscreen progression should force
     a rebuild immediately or wait until the next preparation boundary

7. **[Pending] Replace tiny starter seeding with a real large-scale ledger generator**
   - mature target is an available pool of roughly `6,000` world-bot ledger rows
   - required baseline split is `~3,000 Alliance / ~3,000 Horde`
   - level distribution should follow a normalized 1-80 population curve
   - later refinement should bias class/spec ratios using plausible medium-pop
     realm mixes instead of simple equal-weight flavor selection

8. **[Partial] Upgrade world-bot identity records from simple flags to richer build data**
   - progress now includes personality and optional `loadout_key` support on the
     ledger identity itself
   - target ledger traits still need richer first-class treatment for
     professions, explicit build assignment, combat/talent package identity, and
     later navigation/personality tuning hooks

9. **[Partial] Add personality-driven opposing-faction reaction logic**
   - `uninterested` -> keep working unless forced into combat
   - `opportunistic` -> engage only when local evaluation is favorable
   - `aggressive` -> attack on sight only when target is within `+3` levels
   - `coward` -> flee when the target is meaningfully stronger, otherwise defend
   - progress now includes a normalized personality field on world-bot identity
     and early loadout-selection bias hooks
   - remaining work is the actual battlefield/world-contact behavior layer that
     makes those policies visible in combat and travel decisions

10. **[Pending] Add shared combat navigation doctrine**
    - current repo state has useful movement helpers, but not yet a first-class
      navigation layer shared across bot families
    - required posture vocabulary now includes at least:
      - `Stand Ground`
      - `Reposition`
      - `Kite`
      - `Retreat`
      - `Collapse / Close`
      - `Hold`
    - this layer should decide whether a bot is safe to stand still, should
      create distance, should regroup, or should press into a pack

11. **[Pending] Make action doctrine posture-aware**
    - the current doctrine split of `Single Target` vs `AoE` is useful but too
      flat to create believable combat by itself
    - action selection should understand whether the bot is currently stationary,
      moving, pressured, retreating, or kiting
    - normal hard-cast rotations should only run inside a safe stationary window;
      movement-safe or emergency actions should dominate outside that window

12. **[Pending] Add pull-safety and spatial threat awareness to combat movement**
    - later navigation work needs local awareness of nearby unrelated hostiles,
      risky retreat vectors, and pack-shaping concerns
    - this matters for both PvE pull safety and later PvP maneuvering so bots do
      not blindly kite or chase through bad space

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
