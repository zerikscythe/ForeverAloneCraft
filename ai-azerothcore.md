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
- Account-alt runtime work has started with a pure service seam:
  `model/AccountAltRuntime.h`,
  `integration/AccountAltRuntimeRepository.h`,
  `integration/BotAccountPoolRepository.h`, and
  `service/AccountAltRuntimeService.{h,cpp}`. This path intentionally uses
  bot-owned account-pool accounts for live clones instead of rewriting
  AzerothCore's account login/session limit. The service decides whether to
  prepare a new clone, reuse an active clone, recover an interrupted clone, or
  block when no bot account is available. It also records source/clone progress
  snapshots so recovery does not overwrite newer clone progress with stale
  source data.
- `script/` contains `LivingWorldCommandGrammar.{h,cpp}` as a pure
  parser and `LivingWorldCommandScript.cpp` as the first AzerothCore
  command surface. `.lwbot roster list` reads account alts from the
  characters DB; `.lwbot roster request <id>` routes through
  `PartyBotService` and prints approved / rejected output plus
  `WorldCommitAction` intent. `.lwbot roster dismiss <id>` is still a
  placeholder.
- No living-world code executes party-bot world mutation yet. The next core
  slice should introduce account-alt SQL-backed runtime repositories and then
  the authoritative commit executor that consumes `WorldCommitAction` records
  and performs the server-side spawn/attach work.
- Session feasibility note: a null-socket `WorldSession` IS viable with a
  minimal core patch (see section 20). The previous note saying it was not
  viable has been superseded by a full audit of all socket-liveness check
  sites. The bot account pool design is still correct — bot sessions use
  separate bot-owned accounts, not the player's account — but the session
  lifecycle is enabled by the core patch rather than being blocked by it.
- Account-alt crash recovery is the next safety boundary. The model now
  distinguishes recovery plans and sync domains so clone-to-source writes can
  be gated by sanity checks instead of blindly copying character data. The
  first implementation should recover XP/level/money only; inventory,
  equipment, reputation, quests, and mail need separate domain rules before
  any destructive write path exists.
- `LivingWorldCommandScript` now routes account-alt spawn requests through
  `AccountAltRuntimeCoordinator` before `BotSessionFactory`. This means runtime
  records and snapshots are consulted first, reserved bot accounts are reused
  deterministically, and clone-ahead cases are blocked for manual review.
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

## 12.2.1 Re-examining docs must start with a remote sync check
If the user asks to "re-examine the .mds", "get back up to speed from the
docs", "review the roadmap/agent file again", or similar wording, do not
assume the current checkout is the newest codebase.

Required first step:
- run `git fetch` for the repo
- compare local `HEAD` to the remote branch tip before trusting the docs

If local `HEAD` is behind the remote branch:
- say so explicitly
- do not continue doc/code analysis as if the local checkout were current
- either fast-forward to the remote tip or work from a fresh clone, whichever
  is safer for the current workspace state

This rule exists because the roadmap/agent files are only useful if read
against the newest branch state. A stale local checkout can make the docs look
incorrect when the real issue is that the local repo is behind.

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

---

# 20. Bot Session Architecture — Audit and Decision

This section documents the architecture decision made for spawning account-alt
companions as real `Player` objects rather than `TempSummon` creatures.

## 20.1 The two options considered

**Option A — Bot account pool with a null-socket WorldSession**
Each active bot alt runs as a real `Player` loaded from a bot-owned account.
The session has no real network socket; AI code drives the player directly
by calling `Player` methods on the map thread.

**Option B — Multi-character-per-account unlock**
Relax AzerothCore's one-session-per-account constraint so the same account
can have multiple characters active simultaneously.

Option B was ruled out. The session map (`WorldSessionMgr::_sessions`) is
keyed by account ID. Allowing multiple sessions per account would require
changing the map type, auditing every `FindSession(accountId)` callsite, and
altering the auth server session-key model. The blast radius is too wide and
the risk to real-player sessions is non-trivial.

**Option A was chosen.** Bot sessions use separate bot-owned accounts from the
pool, so no session-map collisions occur with the requesting player.

## 20.2 Full socket-liveness check site audit

Every location that could remove or kill a null-socket `WorldSession` was
audited. Results:

| Site | Code | Null-socket behavior | Action needed |
|------|------|----------------------|---------------|
| `WorldSession::Update` line 583 | `if (!m_Socket) return false;` | **Kills session** | Guard with bot-mode flag |
| `WorldSessionMgr::UpdateSessions` line 117 | `HandleSocketClosed()` checks `m_Socket &&` | Returns false, session left alone | None |
| `WorldSession::Update` line 363 | Idle-close checks `&& m_Socket` | Skipped when null | None |
| `WorldSession::Update` line 381 | Packet loop checks `while (m_Socket &&` | Loop skipped — correct | None |
| `WorldSession::Update` line 565 | Warden update checks `m_Socket &&` | Skipped | None |
| `WorldSession::Update` line 575 | Socket-close detect checks `m_Socket &&` | Skipped | None |
| `WorldSession::Update` line 570 | `ShouldLogOut` checks `_logoutTime > 0` | Always false for bot | None |
| `KickPlayer` | Sets `IsKicked() = true`, calls `CloseSocket` if socket | No socket close; kick flag still set | Guard must also respect `IsKicked()` |

**Exactly one guard line is needed in core.** The complete guard:
```cpp
// WorldSession.cpp — in Update(), replacing:  if (!m_Socket) return false;
if (!m_Socket && (!m_isBotSession || IsKicked()))
    return false;
```
This keeps bot sessions alive through normal world ticks and allows explicit
kicks to still remove them cleanly.

## 20.3 Core changes required (minimal, surgical)

Four files. All changes are additive except the single guard line above.

**`src/server/game/Server/WorldSession.h`**
- Add to private members:
  - `bool m_isBotSession = false;`
  - `ObjectGuid m_botLoginTarget;`
- Add to public interface:
  - `void EnableBotMode() { m_isBotSession = true; }`
  - `bool IsBotSession() const { return m_isBotSession; }`
  - `void SetBotLoginTarget(ObjectGuid guid) { m_botLoginTarget = guid; }`
  - `ObjectGuid GetBotLoginTarget() const { return m_botLoginTarget; }`
  - `bool StartBotLogin(ObjectGuid const& guid);` (declaration only)

**`src/server/game/Server/WorldSession.cpp`**
- One guard line as shown above in `Update()`.

**`src/server/game/Handlers/CharacterHandler.cpp`**
- Add `WorldSession::StartBotLogin` implementation. It uses the file-local
  `LoginQueryHolder` class (already defined there) to queue the async character
  load, which calls back into the existing `HandlePlayerLoginFromDB`. All
  `SendPacket` calls inside that handler are already null-socket safe.

**`src/server/game/Server/WorldSessionMgr.cpp`**
- In `AddSession_()`, after the existing `session->InitializeSession()` call:
  ```cpp
  if (session->IsBotSession() && session->GetBotLoginTarget().IsPlayer())
      session->StartBotLogin(session->GetBotLoginTarget());
  ```

## 20.4 Module-side architecture

Once the core patch is in place, the module needs these new pieces:

**`integration/BotSessionFactory`** — queries `acore_auth.account` for bot
account details, constructs the `WorldSession` with null socket, calls
`EnableBotMode()` + `SetBotLoginTarget()`, then calls
`sWorldSessionMgr->AddSession()`. The `AddSession_` hook then triggers
`StartBotLogin` on the world-update thread.

**`service/BotPlayerRegistry`** — maps `ownerCharGuid → Player*` for active
bot companions. Populated from `PlayerScript::OnLogin` when
`player->GetSession()->IsBotSession()` is true.

**`ai/CompanionAI`** — a `BasicEvent` subclass scheduled on the bot player's
own `m_Events` processor so it runs on the map thread (thread-safe). Each
tick (~500 ms): assist combat by attacking the owner's current target; follow
when idle. Class-specific spell casting is the next slice after this.

**`script/LivingWorldPlayerScript`** — thin `PlayerScript` hook. On `OnLogin`,
checks `IsBotSession()`, registers with `BotPlayerRegistry`, and schedules
the first `CompanionAIEvent`.

**`script/LivingWorldWorldScript`** — stub for future world-tick orchestration.

## 20.5 Thread-safety rule for bot player AI

Bot player AI MUST run on the map thread. The correct pattern is:
- Schedule AI as a `BasicEvent` on `botPlayer->m_Events`
- Inside `Execute()`, run the AI logic, then reschedule a new event before
  returning `true` (which deletes the current event)
- Never drive bot player AI from `WorldScript::OnUpdate` without dispatching
  through the map thread

## 20.6 What the TempSummon path becomes

The existing `TempSummon` (template 111/112) spawn in `LivingWorldCommandScript`
is a temporary stand-in. Once `BotSessionFactory` is wired in,
`ExecuteSpawnRosterBodyAction` for `AccountAlt` source entries should call
`BotSessionFactory::SpawnBotPlayer`. The TempSummon fallback can remain for
generic bot entries that do not map to a real character.

---

## 21. Account-alt runtime recovery and data-loss prevention

The long-term account-alt model is persistent clone first:

- The player-owned source character remains the canonical identity.
- A bot-owned account-pool account hosts a persistent clone character.
- Runtime state records which source/clone pair was last used and which owner
  character requested it.
- A crash, client disconnect, or worldserver stop should leave enough data for
  the next player session to decide whether to reuse, recover, sync, or block.

### 21.1 Durable runtime table

`living_world_account_alt_runtime` belongs in the characters DB because it
references character GUIDs and clone state. It should track:

- `source_account_id`, `source_character_guid`
- `owner_character_guid` for the last player character that requested the bot
- `clone_account_id`, `clone_character_guid`
- runtime `state`
- source and clone snapshots for level, XP, and money
- last clean sync and last recovery-check timestamps

The auth DB `living_world_bot_account_pool` remains only the account lease
surface. It should not become the source of truth for clone progress.

`SqlAccountAltRuntimeRepository` is the concrete module-side repository for
this table. `SqlCharacterProgressSnapshotRepository` is the first read-only
snapshot adapter and intentionally reads only level, XP, and money from
`characters`.

`AccountAltRuntimeCoordinator` is the orchestration seam above those
repositories. It currently supports:

- prepare a new reserved runtime account
- materialize or reuse a persistent clone character on the reserved bot account
- block clone-ahead states for manual review
- start the safer exact-name path by parking the offline source alt under its
  reserved hidden name so the runtime clone can lease the real visible source
  name

It now executes clone-to-source sync for level / XP / money only, gated by the
sanity checker and the sync executor. Broader domains remain blocked.

`integration::SqlCharacterCloneMaterializer` now wraps AzerothCore's
`PlayerDumpWriter` / `PlayerDumpReader` to create the first persistent clone.
It reuses an existing clone by visible name when present, falls back to the
older hidden-name clone path for legacy runtimes, and otherwise:
- renames the offline source alt to `reservedSourceCharacterName`
- updates AzerothCore's `CharacterCache` immediately
- imports the runtime clone under `sourceCharacterName`

This exact-name path now has both halves in code:
- spawn parks the offline source alt under `reservedSourceCharacterName` and
  materializes the runtime clone under the live visible name
- dismiss/logout routes through a dedicated reclaim step that attempts to give
  the source alt its live name back and park the clone under the reserved
  hidden name again

That reclaim step lives behind `CharacterNameLeaseRepository` and is meant to
be cautious. If the runtime/name state is unexpected, blocking/manual review is
safer than guessing.

The first observation layer for item domains now exists as well:
- `model::CharacterItemSnapshot` groups read-only item state into equipment,
  inventory, bank, and other buckets.
- `integration::SqlCharacterItemSnapshotRepository` reads
  `character_inventory` + `item_instance` and classifies nested bag contents
  based on the root bag domain.
- `service::CharacterItemSnapshotClassifier` is the pure seam for slot/domain
  mapping and should be reused by future equipment/inventory/bank sanity
  rules.
- `service::CharacterItemSanityChecker` is the next pure validation seam. It
  currently rejects duplicate/missing item guids, uncategorized storage state,
  malformed equipment slot/container layouts, and invalid inventory/bank
  container chains. It now surfaces `Equipment`, `Inventory`, and `Bank` as
  planning domains when the snapshots are structurally sane, though only
  `Equipment` has an executable write path.
- `integration::SqlCharacterEquipmentSyncRepository` is the first item-domain
  write repository. It deletes the source character's equipped-slot rows,
  duplicates clone equipped `item_instance` rows with fresh item guids owned by
  the source character, and reinserts them into slots 0-18 inside one DB
  transaction. Wrapped gift-backed items are blocked for now.
- `service::AccountAltEquipmentSyncExecutor` mirrors the existing progress
  executor state machine: mark `SyncingEquipment`, call the repository, then
  mark `Active` on success.
- `service::AccountAltItemRecoveryService` is the higher-level item recovery
  seam. It consumes item-sanity results and decides whether to do nothing,
  sync equipment, sync bag domains when policy allows, block unsupported
  inventory/bank deltas, or require manual review.
- Bag-domain write seams now exist for:
  - `integration::CharacterInventorySyncRepository` /
    `SqlCharacterInventorySyncRepository`
  - `integration::CharacterBankSyncRepository` /
    `SqlCharacterBankSyncRepository`
  - `service::AccountAltInventorySyncExecutor`
  - `service::AccountAltBankSyncExecutor`
- These now are wired into the orchestration layer, but only through an
  explicit policy seam. They duplicate item instances, remap nested container
  GUIDs, and rewrite `character_inventory` transactionally for inventory or
  bank domains.
- `AccountAltItemRecoveryService` now owns the explicit bag-domain policy seam.
  It can return a bag-domain sync plan when inventory/bank deltas are sane and
  policy enables them, or keep those domains blocked by default.
- `AccountAltRuntimeCoordinator`, `AccountAltStartupRecoveryService`, and
  `AccountAltDismissalService` now all understand bag-domain recovery plans and
  can route them into the inventory/bank executors when policy allows it.
  The default policy remains conservative: inventory/bank sync stays disabled
  on the live path until runtime validation is possible.
- `service::AccountAltDismissalService` is now the main bot-logout recovery
  seam. It resolves runtimes by clone guid, runs safe progress recovery, runs
  item-domain recovery where allowed, restores names through the name-lease
  repository, and only then lets cleanup continue.

### 21.2 Recovery sequence

On player login, and before any account-alt spawn:

1. List recoverable runtime records for the account.
2. Load current source and clone snapshots.
3. Build a pure `AccountAltRecoveryPlan`.
4. If the clone is not ahead, reuse the persistent clone.
5. If the clone is ahead, run sanity checks before any write.
6. If sanity checks fail, block and require manual review.
7. If sanity checks pass, sync only the approved domains.

### 21.3 Sync domains and first safe scope

Sync must be domain-gated. The current domain list is:

- XP / level
- money
- inventory
- equipment
- reputation
- quests
- mail

First implementation should sync only XP/level and money. Inventory,
equipment, reputation, quests, and mail must remain blocked until they have
domain-specific sanity rules, transaction strategy, and duplicate/loss
prevention.

Inventory and bank now have the first planning and execution rules:
- detect when those domains differ
- validate root-slot and container-chain plausibility
- route into dedicated executors only when policy explicitly allows it
- otherwise block with a clear planner reason

Those dedicated executors now exist in code and are understood by the runtime
services, but they should remain disabled by default until runtime validation
is possible.

### 21.4 Non-negotiable safety rules

- Never blindly copy clone rows over source rows.
- Never run clone-to-source sync without a persisted runtime record.
- Mark a runtime `SyncingBack` before mutation and clear it only after commit.
- Make sync idempotent so repeated recovery after a crash is safe.
- Prefer blocking/manual review over guessing when ownership or snapshots do
  not match.

---

## 22. Bot party membership and dismiss

### 22.1 Why explicit group management is required

`WorldSession::LogoutPlayer` guards its automatic group-removal block with
`m_Socket &&`. Bot sessions use a null socket, so the auto-removal is skipped.
Without explicit removal, dismissed or crashed bots stay in the group slot
forever. All bot group changes must therefore be done by module code, not
relied on from core.

### 22.2 Group join on bot login

`LivingWorldPlayerScript::OnPlayerLogin` calls the module-local helper
`AddBotToOwnerGroup(bot, owner)` after `ScheduleCompanionAI`.

```
AddBotToOwnerGroup:
  group = owner->GetGroup()
  if (!group)
      group = new Group()
      if (!group->Create(owner)) { delete group; return; }
      sGroupMgr->AddGroup(group)
  if (group->IsFull()) return
  group->AddMember(bot)
```

`Group::Create` initialises the group with the owner as leader and persists it
through `GroupMgr`. `AddMember` wires in the bot as a real member so the
client party frame shows it immediately.

### 22.3 Owner logout initiates dismiss

`LivingWorldPlayerScript::OnPlayerLogout` already kicks the active bot when the
real owner logs out. That is the correct place to begin the dismiss lifecycle:
owner logout should not leave the clone standing in-world with no owner.

### 22.4 Group removal on bot logout

`OnPlayerLogout` removes the bot before unregistering it:

```cpp
if (Group* group = player->GetGroup())
    group->RemoveMember(player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
```

`GROUP_REMOVEMETHOD_LEAVE` is used (not KICK) because the bot leaving is a
clean departure, not a punishment. The group remains alive for the owner if
other members are still present.

### 22.5 Dismiss flow

`.lwbot roster dismiss <id>` resolves to `RenderDismissBot`:

1. `BotPlayerRegistry::FindBotForOwner(player->GetGUID())` — look up active bot.
2. `group->RemoveMember(bot->GetGUID(), GROUP_REMOVEMETHOD_LEAVE)` — clean party.
3. `bot->GetSession()->KickPlayer("LivingWorld roster dismiss")` — sets
   `IsKicked() = true`.

The kick flag is read on the next worldserver update tick by the guard in
`WorldSession::Update`:

```cpp
if (!m_Socket && (!m_isBotSession || IsKicked()))
    return false;
```

Returning false from `Update` triggers `LogoutPlayer` → `OnPlayerLogout` hook
→ `UnregisterBotPlayer` + bot-pool DB release. The sequence is fully
deterministic and produces no orphaned registry entries or group slots.

### 22.6 Current dismiss/logout lifecycle

The current dismiss lifecycle is:
1. find the runtime by `clone_character_guid`
2. sync approved progress/item domains from clone back to source
3. attempt source-name reclaim through `CharacterNameLeaseRepository`
4. then continue cleanup and bot-account release

This is intentionally conservative. If the current DB/runtime names do not
match the expected leased layout, name reclaim should fail safe rather than
guess.

### 22.7 What remains for multi-bot support

The next meaningful gap after the current dismiss lifecycle is not the basic
logout path anymore. It is:
- giving bag-domain policy a real config/manual-control surface
- strengthening nested-container/manual-review rules
- runtime-verifying name reclaim, dismissal ordering, and bag-domain safety

---

## 23. Clone-to-source sync executor

### 23.1 Design goals

The sync executor is the first slice that actually writes clone progress back
to the source character. Three invariants govern it:

1. **SyncingBack guard**: the runtime state is set to `SyncingBack` and
   persisted before any character write. A crash between the state write and the
   character write leaves a recoverable record; a startup recovery pass can
   detect `SyncingBack` and retry.
2. **Domain gating**: only domains listed in `domainsToSync` are copied.
   Fields not in the approved list keep the source character's existing values.
3. **DirectExecute**: `SqlCharacterProgressSyncRepository` uses
   `CharacterDatabase.DirectExecute()` so the UPDATE is committed before
   control returns. This ensures the characters row is durable before a bot
   session loads it.

### 23.2 Execution sequence

```
AccountAltRuntimeCoordinator::PlanSpawn
  sanity check passes, plan = SyncCloneToSource
    → AccountAltSyncExecutor::Execute(runtime, cloneSnapshot, domainsToSync)
        1. runtime.state = SyncingBack  → SaveRuntime
        2. build target snapshot (clone fields for approved domains)
        3. SqlCharacterProgressSyncRepository::SyncProgressToCharacter
             DirectExecute: UPDATE characters SET level, xp, money WHERE guid=sourceGuid
        4. runtime.sourceSnapshot = target, runtime.state = Active → SaveRuntime
        5. return true
    ← SpawnUsingPersistentClone
  on executor failure → ManualReviewRequired (state stays SyncingBack for recovery)
```

### 23.3 Layer placement

```
LivingWorldCommandScript  (executes on world thread, map thread context)
  └─ AccountAltRuntimeCoordinator  (orchestration: decision + sync trigger)
       └─ AccountAltSyncExecutor   (service: state machine)
            ├─ AccountAltRuntimeRepository  (runtime record I/O)
            └─ CharacterProgressSyncRepository  (character row write)
                  └─ SqlCharacterProgressSyncRepository  (DirectExecute impl)
```

### 23.4 What the startup recovery pass does

When an owner player logs in, `LivingWorldPlayerScript::OnPlayerLogin`
(non-bot path) now:
1. Queries `living_world_account_alt_runtime` for the source account.
2. For each record with `state = SyncingBack`: reloads source/clone snapshots,
   re-runs sanity checks, and retries `AccountAltSyncExecutor::Execute`.
3. For each record with `state = SyncingEquipment`, `SyncingInventory`, or
   `SyncingBank`: reloads item snapshots, rebuilds the guarded item-recovery
   plan, and retries only that specific executor.
4. For active materialized clones: builds the recovery plan and surfaces counts
   for pending recovery, manual review, and blocked runtimes through login
   messages.

This pass intentionally stays lightweight:
- it only performs writes for runtimes already marked `SyncingBack`,
  `SyncingEquipment`, `SyncingInventory`, or `SyncingBank`
- it does not auto-run broader clone-ahead recovery for `Active` runtimes
- it does not auto-run new inventory/equipment/bank recovery for `Active`
  runtimes that have not already entered a guarded syncing state

### 23.5 Sync domain scope

Currently synced on the main progress path: `Experience` (level + XP) and
`Money`.

Additional guarded write paths now exist for:
- `Equipment` — via explicit item sanity + executor path
- `Inventory` — via explicit item sanity + policy-gated executor path
- `Bank` — via explicit item sanity + policy-gated executor path

Still not safe without additional rules:
- `Reputation` — faction relation edge cases
- `Quests` — quest state machine conflicts
- `Mail` — item attachment ownership

---

## 24. Bag-domain policy surface and container sanity tightening

### 24.1 Config-driven policy

`mod-living-world.conf.dist` now exposes:
```
LivingWorld.AccountAlt.EnableInventorySync = 0
LivingWorld.AccountAlt.EnableBankSync = 0
```

Both default to 0 (disabled). All three service construction sites that accept
`AccountAltItemRecoveryOptions` read these values via
`sConfigMgr->GetOption<bool>` at construction time:
- `AccountAltRuntimeCoordinator` in `LivingWorldCommandScript`
- `AccountAltStartupRecoveryService` in `LivingWorldPlayerScript`
- `AccountAltDismissalService` in `LivingWorldPlayerScript`

This replaces the previous hardcoded struct default (always disabled) with a
real operator-controlled knob while keeping the safe default.

### 24.2 Per-container item cap

`CharacterItemSanityChecker::Check` now rejects any snapshot where a single
bag container holds more than 36 items. 36 is the maximum slot count of any
bag in WoW 3.3.5a. Items inside bags are counted by their `containerGuid`; if
any guid accumulates more than 36 children, the check adds:
`"inventory/bank snapshot has a container exceeding the bag size cap"`.

### 24.3 Bag-container-change detection

`AccountAltSanityCheckResult` has a new `bagContainersChanged` field.
`CharacterItemSanityChecker::Check` computes it by comparing the itemEntry of
bag containers at root inventory bag slots (19–22) and root bank bag slots
(67–73) between source and clone. It is computed unconditionally — even when
other checks fail — so the recovery service has the information regardless.

`AccountAltItemRecoveryService::BuildRecoveryPlan` uses this field to route to
`ManualReview` when `bagContainersChanged` is true and inventory or bank domains
differ. This blocks automated sync when the bags themselves changed, since the
inventory/bank sync executors remap container GUIDs and the result could be
unexpected if the root bags are a different type than what was recorded.

### 24.4 Safety line

- Inventory/bank sync remains disabled by default in conf.dist.
- Even when enabled by config, bag-container-change detection causes ManualReview
  to take priority over SyncBagDomainsToSource.
- Dismissal and startup-recovery summaries now report per-domain recovery
  outcomes so command/script callers can tell whether equipment, inventory, or
  bank sync actually ran.
- The next step before enabling inventory/bank sync in any live config is runtime
  verification: exercise the full spawn → dismiss → startup recovery cycle and
  confirm source characters are in correct state after each step.
