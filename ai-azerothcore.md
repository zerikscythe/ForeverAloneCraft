# AI Assist File - Living World / Bot System for AzerothCore
_Reference document for AI-assisted development of the custom living-world system_

## Purpose of This File

This file exists to brief any future AI assistant or developer on:

- what this project is trying to accomplish
- what the custom system should and should not do
- how code should be structured
- how changes should be made safely
- what project files are expected to own which responsibilities
- how to avoid turning the codebase into a tangled mess

This is intended as a **working engineering brief**, not marketing copy.

---

# 1. Project Summary

The goal is to build a **living-world bot system** for a mostly solo/private World of Warcraft server.

Primary intended target:
- **AzerothCore 3.3.5a**
- Modular implementation
- C++ module-first approach
- Single codebase that can simulate a **Classic -> TBC -> Wrath** style progression inside a Wrath-era server

This project is **not** trying to recreate retail-authentic population at scale.

Instead, it is trying to create:
- a believable world population
- repeatable familiar characters
- party bots for grouped-feeling solo play
- rival guild encounters for recurring world-PvP tension
- a progression-aware world that unlocks over time

This is effectively:
**a single-player / private-server living-world sandbox built on top of AzerothCore**

---

# 2. Design Goals

## Must achieve
- Make the world feel populated around the player
- Support many persistent bot identities, but only a small number active at once
- Support city life, travelers, gatherers, party bots, and rival guild bots
- Keep expensive simulation local to the real player
- Use AzerothCore modules cleanly instead of hard-patching core wherever possible
- Keep the design maintainable over time
- Separate data model, decision logic, and emulator integration where possible

## Must avoid
- Simulating the whole world equally at all times
- Building everything directly into AzerothCore core files unless absolutely necessary
- Hardcoding behavior in one giant monolithic file
- Letting every subsystem know too much about every other subsystem
- Writing code that assumes bots must behave like perfect real players
- Constant per-tick expensive logic for every abstract bot
- Deep coupling to one expansion phase if the goal is phased progression

---

# 3. Core Technical Philosophy

## 3.1 Large abstract roster, small active slice

The system may contain:
- 1,000 to 2,000+ persistent bot identities

But at any moment only a small number should be:
- visible
- spawned
- pathing
- fighting
- gathering
- interacting with the world

Most bots should live in an **abstract state** most of the time.

## 3.2 CPU is the likely bottleneck
The main performance concern is not bot records in memory.

The expensive parts are:
- movement/pathing
- world queries
- combat
- aggro checks
- line-of-sight / range checks
- resource-node logic
- frequent AI decision evaluation

Code should therefore be written to:
- reduce high-frequency expensive work
- favor cached decisions where possible
- keep off-screen simulation cheap
- centralize expensive state changes

## 3.3 Keep world mutation authoritative
The system may eventually benefit from worker/background logic, but:
- world state mutation
- spawning/despawning
- combat application
- node consumption
- loot ownership
must remain safe and centralized

Future architecture should favor:
- background planning
- authoritative world commit

---

# 4. Functional Scope

The custom system should eventually support the following major features.

## 4.1 Ambient world population
Examples:
- city walkers
- inn sitters
- mailbox/AH/bank visitors
- trainer/vendor idlers
- road travelers

## 4.2 Outdoor activity bots
Examples:
- gatherers
- grinders
- quest-area wanderers
- crafters returning to town
- bots fighting mobs or traveling between camps

## 4.3 Party bots
Examples:
- tank companion
- healer companion
- DPS companion
- support / hybrid roles

These help make the server feel grouped and cooperative during solo play.

Party bots should eventually run structured combat-profile data rather than
growing into hardcoded one-off class logic.

## 4.4 Rival guild system
Examples:
- enemy solo scouts
- enemy patrols
- enemy groups up to 5
- recurring guild-tagged enemies with personalities
- escalating “them vs us” encounters

## 4.5 Progression-aware world
Examples:
- Classic phase
- TBC phase
- Wrath phase
- content gated by progression state
- bot behavior aware of unlocked content and level caps

---

# 5. Module Strategy

The preferred implementation target is a set of **AzerothCore modules**, not broad direct edits to the server core.

Recommended initial module split:

## 5.1 mod-living-world
Main dynamic system.

Should own:
- bot identities
- abstract state
- spawn selection
- city / travel / outdoor encounter planning
- party bot management rules
- rival guild management
- group personality handling
- encounter cooldowns
- progression awareness at the behavior layer

## 5.2 mod-living-world-progress
Progression and content gating support.

Should own:
- phase flags
- level cap rules
- Dark Portal / Northrend unlock rules
- progression SQL/config gating
- integration points with living-world logic

## 5.3 Optional integration modules
Possible later integration with:
- playerbot systems
- npcbot systems
- custom account/character progression support

These should be integrations, not the foundation of the architecture.

---

# 6. Required Separation of Concerns

Future code should be written in layers.

## 6.1 Data / Model Layer
Contains persistent and runtime state models only.

Examples:
- BotProfile
- BotAbstractState
- BotSpawnContext
- RivalGuildProfile
- RivalGuildMember
- PartyBotProfile
- EncounterRecord
- ProgressionPhaseState

Rules:
- this layer should not perform world actions directly
- keep it as plain and predictable as possible
- prefer simple structs/classes with clear ownership

## 6.2 Decision / Planner Layer
Contains behavior and selection logic.

Examples:
- SpawnSelector
- ZonePopulationPlanner
- EncounterPlanner
- RivalAggressionResolver
- PartyRolePlanner
- GroupStateResolver
- RespawnCooldownPolicy
- ProgressionGateResolver

Rules:
- this layer decides what should happen
- this layer should avoid direct world mutation
- this layer should be testable with fake/mock data where possible

Combat-profile selection belongs here as well:
- choose class/role/level-band profile
- apply loadout assumptions
- apply player overrides
- resolve the next intended action candidate

## 6.3 Integration / Adapter Layer
Contains AzerothCore-specific operations.

Examples:
- querying player zone
- finding nearby creatures/players
- spawning/despawning actors
- issuing combat actions
- retrieving map/world context
- checking combat state
- checking death state
- trying node interaction

Rules:
- keep core logic out of this layer
- this layer should be thin
- this layer converts planner outputs into server actions

## 6.4 Orchestration Layer
Owns update flow and high-level coordination.

Examples:
- LivingWorldManager
- WorldPopulationService
- RivalGuildService
- PartyBotService
- ProgressionSyncService

Rules:
- do not let this become a junk drawer
- orchestration should call planners and adapters, not contain all behavior inline

Combat execution should follow the same rule:
- orchestration/service updates active combat state
- planner resolves the intended action
- adapter/integration performs the actual cast or item use attempt

---

# 7. Suggested Folder / File Layout

Example for `mod-living-world`:

```text
modules/
  mod-living-world/
    conf/
      mod-living-world.conf.dist

    data/
      sql/
        db-world/
        db-characters/

    src/
      loader/
        mod_living_world_loader.cpp

      model/
        BotProfile.h
        BotAbstractState.h
        RivalGuildProfile.h
        EncounterTypes.h
        (etc. — plain value types only)

      planner/
        SpawnSelector.h
        EncounterPlanner.h
        GroupStateResolver.h
        RivalAggressionResolver.h
        RespawnCooldownPolicy.h
        SimplePartyRosterPlanner.{h,cpp}
        SimpleZonePopulationPlanner.{h,cpp}

      integration/
        WorldReadContext.h          <- value types for read queries
        WorldCommitAction.h         <- explicit mutation records
        AzerothWorldFacade.h        <- pure-virtual world read interface
        AzerothWorldFacade*.cpp     <- real AC-backed implementation
                                       (lives here, not in planner/)
        RosterRepository.h          <- pure-virtual roster lookup
        RosterRepository*.cpp       <- real SQL/config-backed impl

      service/
        LivingWorldManager.{h,cpp}
        PartyBotService.{h,cpp}
        RivalGuildService.{h,cpp}
        ProgressionAwarePopulationService.{h,cpp}

      script/
        LivingWorldCommandGrammar.{h,cpp}  <- pure parser, testable
        LivingWorldCommandScript.cpp       <- registers .lwbot in AC
        LivingWorldWorldScript.cpp
        LivingWorldPlayerScript.cpp
        LivingWorldCreatureScript.cpp
```

Current state as of the first foundation + first runtime command slice:
- `integration/` contains `WorldReadContext.h`, `WorldCommitAction.h`,
  `AzerothWorldFacade.h`, and `RosterRepository.h`. Tests use fake
  subclasses. The current runtime command script includes thin AC-backed
  adapter implementations for player context, online checks, and account-alt
  roster lookup; extract them into dedicated integration files once more
  command/service consumers appear.
- `planner/` contains `SimpleZonePopulationPlanner` and
  `SimplePartyRosterPlanner`. The zone planner now performs planner-owned
  eligibility filtering from abstract state plus progression/unlocked-zone
  checks, then reprioritizes selector output with simple scores before
  budget trimming. The weights are still code-local and should eventually
  move to config/data.
- `service/` contains `LivingWorldManager` and `PartyBotService`. The
  service resolves roster entries through `RosterRepository` scoped to
  the requesting account. Neither class executes world mutation; they
  produce `WorldCommitAction` records.
- `script/` contains `LivingWorldCommandGrammar.{h,cpp}` as a pure
  parser and `LivingWorldCommandScript.cpp` as the first AzerothCore
  command surface. `.lwbot roster list` reads account alts from the
  characters DB; `.lwbot roster request <id>` routes through
  `PartyBotService` and prints approved / rejected output plus
  `WorldCommitAction` intent. `.lwbot roster dismiss <id>` is still a
  placeholder.
- No living-world code executes party-bot world mutation yet. The next core
  slice should introduce the authoritative commit executor that consumes
  `WorldCommitAction` records and performs the server-side spawn/attach work.
- Local runtime validation has reached a working end-to-end WotLK install:
  MySQL 8, authserver, worldserver, extracted dbc/maps/vmaps/mmaps, client
  login, character creation, and starting-zone entry. This validates the
  environment for party-bot runtime work, but the install/deploy scripts live
  outside the repo under `D:\Wow Private\WotLK`.

This exact structure can change, but the separation should remain.

---

# 8. Main Concepts That Must Be Preserved

## 8.1 Persistent bot identity
Bots should feel like recurring characters, not disposable random spawns.

Each bot should be able to carry:
- name
- race/class/faction
- guild
- level
- personality
- role
- preferred zones
- abstract current region
- cooldowns
- history

## 8.2 Abstract state first
Bots should not require live world presence in order to “exist.”

They should maintain:
- current status
- bound region
- death cooldown
- relocation timer
- encounter timer
- city/outdoor preference state

## 8.3 Player-centered population
The world should feel active near the real player.

Priority population areas:
1. current player city / hub
2. current road / travel corridor
3. current questing zone
4. nearby adjacent activity
5. rest of world abstract only

## 8.4 Rival guild continuity
The rival guild should feel like a recurring opposing group, not random hostile nameless bots.

Important:
- repeated familiar names
- shared guild identity
- mixed spawn sizes
- group reactions
- history of encounters

## 8.5 Party bot utility
Party bots should be reliable enough to support the player without requiring perfect raid AI.

## 8.6 Combat behavior should be data-authored
Combat behavior should eventually be authored as structured data:
- class-aware
- role/type-aware
- level-band-aware
- loadout-aware
- player-overridable

Bots should behave like role-driven agents executing prioritized rules, not
like giant hardcoded class switches.

---

# 9. Personality and Group Rules

## 9.1 Personality types
Current intended base personalities:
- Indifferent
- Cautious
- Aggressive

### Indifferent
- usually ignores player
- fights if attacked or ally is attacked

### Cautious
- notices player
- may stop and watch
- may face player
- may reposition
- caster may prep buffs
- may engage if conditions favor it

### Aggressive
- likely to approach or attack
- prefers favorable conditions
- can trigger group escalation

## 9.2 Group state escalation
Groups may be:
- Idle
- Alert
- Engaged

Rule:
- if one group member initiates and the player fights back, the group flips to engaged/aggressive mode

This must remain a design anchor unless intentionally revised.

## 9.3 Caster caution behavior
In caution mode, casters may:
- stop and face player
- cast visible self-buffs or prep spells
- use this both as intimidation and tactical preparation

Do not implement this as spam.
One or two meaningful actions are enough.

---

# 10. Encounter Design Rules

Encounters should not all become fights.

The system should support:
- pass-by encounters
- suspicious observation
- stalking
- small skirmishes
- larger rare clashes

The player should sometimes think:
- “they noticed me”
- “they might attack”
- “they decided not to”
- “this group is sizing me up”

That tension is valuable.

## Anti-frustration rules
Avoid:
- direct teleport ambushes
- repeated instant re-engagement
- corpse camping
- constant unavoidable attacks
- too many back-to-back rival encounters

---

# 11. Progression Rules

The long-term target is a **Classic -> TBC -> Wrath progression simulation inside a 3.3.5a environment**.

That means:

- the system should not assume all content is always available
- bot level ranges and zone preferences should eventually become phase-aware
- rival and ambient populations may differ by phase
- content gating should be centralized rather than scattered across random files

Progression-sensitive logic belongs in:
- phase config
- progression service
- progression-aware selectors

Avoid hardcoding phase checks everywhere.

---

# 12. How AI Assistants Should Approach Code Changes

Any future AI assistant working on this project should follow these rules.

## 12.1 Do not dump logic into one huge file
If a requested feature expands responsibility significantly:
- create or extend a dedicated class/service
- wire it in cleanly
- avoid “just add 400 lines to manager.cpp”

## 12.2 Prefer surgical edits
When editing existing files:
- change the smallest correct surface area
- preserve naming style where possible
- preserve module conventions
- do not unrelated-refactor half the project unless explicitly requested

## 12.3 Keep comments meaningful
Add useful comments where:
- a rule is non-obvious
- a behavior is intentionally game-design-driven
- a performance tradeoff exists
- a future expansion hook is intended

Do not fill files with obvious noise comments.

## 12.4 Preserve modularity
New features should be added by:
- extending models
- extending planners
- extending services
- adding hooks/config/sql where needed

Avoid:
- hidden static globals
- duplicate policy logic in multiple places
- one-off hacks that bypass planners/services

## 12.5 Prefer config over hardcoding where tuning will matter
Things likely to need tuning should live in config/SQL/data:
- local population caps
- rival encounter cooldowns
- party/rival group size weights
- caution/aggressive probabilities
- phase gating values
- respawn timers

---

# 13. How Main Project Files Should Be Edited

This section is specifically for AI or developer guidance.

## 13.1 Loader file
The loader file should:
- register scripts/services cleanly
- remain small
- not contain behavior logic

If the loader starts becoming “smart,” something is in the wrong place.

## 13.2 Script hook files
Hook files should:
- respond to AzerothCore hook entry points
- forward work into services/managers
- avoid containing full feature logic inline

Example:
- Player zone changed -> call population service
- Player died -> call encounter cooldown service
- World update tick -> call living world manager

Hook files are entry points, not the full system.

## 13.3 Manager/service files
Manager/service files should:
- coordinate subsystems
- call planners
- call adapters
- own lifecycle/state orchestration

They should **not** become endless switchboards of hardcoded behavior.

If a service begins owning too many unrelated rules:
- split out planner/policy/helper classes

## 13.4 Model files
Model files should:
- stay plain and stable
- avoid direct server calls
- avoid hidden “smart” side effects

## 13.5 Planner/policy files
These are preferred locations for:
- spawn scoring
- aggression logic
- cooldown calculations
- progression-aware decisions
- personality rules

Keep these deterministic and readable.

## 13.6 SQL/config files
Use SQL/config for:
- default rosters
- guild membership
- progression flags
- tunable weights
- per-phase settings
- optional content gating

Do not bury tuning-only constants in random C++ files unless absolutely necessary.

---

# 14. Editing Discipline for Future AI Code Work

When asked to implement a new feature, the assistant should first determine:

1. Is this a model change?
2. Is this a planner/policy change?
3. Is this a service/orchestration change?
4. Is this a hook/integration change?
5. Is this a config/sql change?
6. Is this a progression-aware feature?
7. Is this likely to need tuning later?

Then place code accordingly.

## Good example
Feature request:
“Cautious casters should buff before possible combat.”

Likely changes:
- personality/planner logic
- maybe config/tuning values
- maybe a helper in encounter planner
- maybe a spell-prep action mapping

Likely **not**:
- random hardcoded spell logic shoved into the world tick manager

## Good example
Feature request:
“Rival guild groups should spawn in sizes of 1 to 5.”

Likely changes:
- spawn planner
- group composition policy
- config weights
- encounter service

Likely **not**:
- ad hoc random group-size code repeated in three files

---

# 15. Coding Style Guidance

## Prefer
- readable names
- small focused classes
- explicit ownership
- low-surprise code paths
- practical comments
- predictable update flow

## Avoid
- giant God classes
- clever but opaque template/meta tricks unless necessary
- duplicated logic
- deeply nested if/else trees for behavior rules
- cross-layer dependencies that make testing/reasoning hard

---

# 16. Long-Term Extensibility Goals

The system should be written so that future features can be layered in without rewrites.

Possible future additions:
- named nemesis subgroup inside rival guild
- profession-aware economic simulation
- player reputation with rival groups
- zone-specific rival patrol routes
- more advanced party formations
- encounter storytelling/history logs
- rare world events
- player-driven phase timing
- expansion-aware travel changes

The code should not assume the current feature list is the end state.

---

# 16A. Bot Control and Combat Profiles

The current intended player-facing control model is:
- command-driven first
- addon-assisted as the primary long-term UX
- structured combat-profile editing rather than freeform scripting

## 16A.1 Keep control layers separate

The bot system should be split into three distinct control layers:

### Roster layer
Answers:
- who is available
- who is active
- who belongs to the player
- who is in the party

### Behavior layer
Answers:
- follow / hold / assist / autonomous
- passive / defensive / aggressive
- whether the player is possessing the bot or only commanding it

### Combat-profile layer
Answers:
- what the bot does inside combat
- priority rules
- maintenance logic
- utility logic
- racial and trinket usage
- player overrides

These layers should not collapse into one giant command surface.

## 16A.2 Addon-first command philosophy

Text commands should be treated as the backend control protocol for a future
UI addon, not the final UX.

That means the system should prefer:
- stable bot/runtime IDs
- stable stance/mode enums
- structured state snapshots
- machine-friendly command grammar
- predictable acknowledgements/results

## 16A.3 Combat-profile behavior model

When a bot enters combat, behavior should resolve from:
- class
- role/type
- level band
- race
- loadout assumptions
- runtime context
- optional player override

The expected loop is:
1. gather combat context
2. evaluate structured rules in priority order
3. choose the first valid action
4. execute it through the integration layer

This should behave like a prioritized rule engine, not a static rotation list.

## 16A.4 Combat-profile data direction

Profiles should eventually be serialized in separate files, likely JSON, with
one profile per class/type/level band.

Expected dimensions:
- class
- role/type
- optional subtype/spec style
- level range
- loadout profile
- behavior flags
- maintenance rules
- combat rules
- utility rules
- racial rules
- trinket rules

## 16A.5 Level-band strategy

Support 5 to 10 level chunks, but prefer 10-level bands as the baseline.
Split into 5-level bands only where meaningful power spikes justify the extra
authoring cost.

## 16A.6 Loadout-aware doctrine

Combat profiles should account for loadout assumptions such as:
- one-hand + shield
- two-hander
- dual wield
- ranged emphasis
- caster vs melee orientation
- stat bias / gear intent

Loadout should influence behavior selection, not only presentation.

## 16A.7 Player-editable overrides

Players should be able to tweak bot combat behavior through the addon, but
within structured guardrails.

Safe editable areas:
- priority order
- enabled/disabled actions
- health/mana thresholds
- utility aggressiveness
- maintenance toggles
- racial use policy
- trinket use policy

Early implementation should avoid arbitrary script execution.

## 16A.8 External combat doctrine vs local runtime truth

External sites such as Wowhead and Icy Veins are acceptable sources for combat
doctrine only:
- role identity
- rotation philosophy
- cooldown timing concepts
- buff/debuff priorities
- racial/trinket ideas
- loadout assumptions

They are not the runtime source of truth for:
- exact spell IDs
- exact item IDs
- exact local DB values
- what the bot actually knows or has equipped

Use them to derive concepts, then convert those concepts into internal profile
data. The local emulator/runtime remains authoritative for executable values.

## 16A.9 Recommended authoring pipeline

1. Read external class-guide doctrine
2. Convert doctrine into internal structured combat-profile data
3. Validate against local AzerothCore/runtime truth
4. Execute only what is legal and available

---

# 17. Practical Development Order

Preferred implementation order:

## Phase 1
- module skeleton
- config/sql setup
- bot profile model
- abstract state model
- living world manager
- simple player zone awareness

## Phase 2
- local city population
- simple ambient spawns
- despawn/respawn timing
- local population budgets

## Phase 3
- rival guild roster
- solo/small-group encounters
- group alert/engage states
- personality-driven reactions

## Phase 4
- party bot support
- role-based assist/follow behavior
- basic reliable group play

## Phase 5
- outdoor gatherers/grinders
- real node checks where needed
- progression-aware population behavior

## Phase 6
- tuning
- optimization
- optional worker/planning split
- richer continuity/history systems

## Phase 7
- simulated economy / AH seeding
- event-aware market and population changes
- milestone-driven progression unlocks

## Phase 8
- deeper continuity systems
- richer world-history reactions
- optional special progression events

---

# 18. Final Instruction for Future AI Assistants

When working on this project:

- think in systems, not patches
- preserve modularity
- keep heavy logic local to the player experience
- do not over-simulate off-screen actors
- keep behavior believable rather than perfect
- prefer one stable extensible codebase over short-term hacks
- do not assume “more realism” means “more constant simulation”
- when unsure, place logic in the smallest responsible layer

The goal is not to build a fully autonomous MMO civilization.

The goal is to build a **convincing living-world layer** that makes a mostly solo AzerothCore server feel inhabited, social, and reactive.

That is the target.

---

# 19. Additional Design Threads To Preserve

The following ideas were added later from adjacent design notes and should be
treated as valid future directions for this project.

## 19.1 Simulated economy should be policy-driven

The server may eventually support a believable Auction House / economy layer,
but it should avoid overengineering a full fake-player economy.

Preferred design direction:
- seeded supply
- market absorption for player auctions
- event-aware demand shifts
- progression-aware stock pools
- soft stock rotation rather than rigid player-style expiration for all seeded
  inventory

Avoid assuming we need:
- full bot-wallet simulation
- fully autonomous buyer/seller negotiation
- per-tick market recalculation

## 19.2 Events should influence population and supply

Active world or seasonal events should eventually be able to affect:
- city crowding
- travel routes
- stock pools
- rival/ambient behavior
- local activity weighting

This should remain data- and policy-driven, not hardcoded into one-off hooks.

## 19.3 Progression should be able to react to player accomplishments

Long-term progression should not be forced to depend only on wall-clock timing.

The architecture should leave room for milestone-driven progression such as:
- boss kill unlock flags
- special unlock events
- AQ-style special gating flows
- phase-sensitive population/economy changes

If implemented, this belongs in centralized progression data/services rather
than scattered checks across unrelated behavior code.
