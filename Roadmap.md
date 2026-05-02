# Roadmap

## Project Scope

This roadmap tracks the AzerothCore living-world project for
`ForeverAloneCraft`.

It uses the older roadmap as a structural template, but it has been rewritten
for this codebase and current design direction:

- AzerothCore 3.3.5a
- C++ module-first implementation
- living-world architecture with clear separation between model, planner,
  integration, and orchestration layers
- command-driven party bot and account-alt support as near-term gameplay goals
- world mutation kept behind explicit service/adaptor boundaries

The old roadmap included a number of emulator-specific tasks, file names,
runtime assumptions, and command surfaces from a different server stack. Those
details have been intentionally removed here rather than copied forward.

## Recent Progress Snapshot

The latest completed gameplay slice is quest-state continuity for account-alt
bots plus the first bot quest UX layer.

Completed recently:

- active quest accept now mirrors owner -> eligible active bots
- clone -> source quest sync now preserves active quest rows and objective
  counters correctly
- bot quest completion now syncs back during play via
  `LivingWorldPlayerScript::OnPlayerCompleteQuest`
- reward-choice quests now support a smart/manual bot reward flow
- the LivingWorld addon now has a `Quests` tab for pending bot reward choices

Current next-planned slice:

- extend the `Quests` tab into a broader bot quest-actions panel that can show
  bot-specific `Pick Up` / `Turn In` actions for a targeted quest giver,
  including class-specific follow-up quests

Current agreed design workstream:

- define and implement server-authoritative user-overrideable bot combat
  profiles backed by relational DB tables rather than JSON blobs
- ship a baked-in default combat doctrine for one starter spec/role per class,
  with blank profiles falling back to that default behavior automatically
- expose profile editing through an addon-facing API/message layer handled by
  worldserver services/repositories, not by direct addon-to-database access
- support row-based editing of actions/conditions/targets, including
  `enemy_primary` and `enemy_trash` target resolution, primary/secondary
  fallback timing, conservation modes, and optional down-ranking

## Sensitive Data Review

The old roadmap did not contain obvious secrets such as passwords, API keys, or
private tokens.

It did include implementation details that are not useful to carry forward
verbatim, including:

- old emulator project paths and class names
- internal account/template examples from that project
- direct DB/table assumptions tied to the previous emulator
- implementation notes for abandoned runtime paths

Those details are omitted from this roadmap.

## Status Legend

- **Complete**: finished and validated in the current AzerothCore workstream
- **Partial**: some foundation exists, but the milestone is not complete
- **Not Started**: no meaningful implementation yet

---

## Phase 1: Foundation and Compile Safety

**Overall Status: Partial**

### 1) Build Environment and Baseline Workflow

**Overall Status: Complete**

#### Subtasks

1.1 Establish a repeatable Windows build path — **Complete**
- Known-good configure path exists via
  [CMakePresets.json](D:/src/azerothcore-wotlk/CMakePresets.json).
- `worldserver` builds successfully from the validated VS2022 build tree.

1.2 Restore Windows unit test build stability — **Complete**
- MSVC-specific test build issues were fixed.
- `unit_tests` builds successfully on Windows.
- `ctest -C Debug` passes in the validated build tree.

1.3 Keep local development safe and repeatable — **Complete**
- Working preset/configure path is documented in repo state.
- Module foundation compiles without requiring world-mutation features yet.

### 2) Module Skeleton and Repo Structure

**Overall Status: Complete**

#### Subtasks

2.1 Create the first `mod-living-world` module skeleton — **Complete**
- Module loader, config dist file, source layout, and test registration exist.

2.2 Keep the implementation module-first — **Complete**
- New work lives under `modules/mod-living-world/`.
- Core edits so far are limited to build/test integration and portability fixes.

2.3 Define a maintainable folder layout — **Complete**
- Current structure includes:
  - `model/`
  - `planner/`
  - `loader/`
  - `test/`

---

## Phase 2: Data / Model Layer

**Overall Status: Partial**

This phase owns the persistent/runtime data types that planners operate on.
These types should remain free of direct world mutation.

### 3) Core Living-World Models

**Overall Status: Partial**

#### Subtasks

3.1 Create compile-ready foundational model types — **Complete**
- Initial types exist for:
  - `BotProfile`
  - `BotAbstractState`
  - `BotSpawnContext`
  - `RivalGuildProfile`
  - `RivalGuildMember`
  - `PartyBotProfile`
  - `EncounterRecord`
  - `ProgressionPhaseState`

3.2 Define shared encounter and roster enums/types — **Complete**
- Shared planner/model enums and lightweight value types exist.

3.3 Add player-control and party-roster request models — **Complete**
- Initial controllable bot and roster request/context types exist.

3.4 Add persistence-ready identity/state fields — **Partial**
- Type shells exist, but SQL-backed persistence and migration rules are not
  implemented yet.

3.5 Finalize ownership and lifecycle rules for abstract bots — **Not Started**
- Need explicit rules for:
  - generic roster entries
  - account-alt entries
  - possessed/controlled entries
  - temporary active world bodies vs long-lived identity records

---

## Phase 3: Planner Layer

**Overall Status: Partial**

This phase owns decision logic only. Planner outputs should remain testable and
independent from direct AzerothCore world mutation.

### 4) Planner Interfaces

**Overall Status: Partial**

#### Subtasks

4.1 Create named planner interfaces from the design brief — **Complete**
- Initial interfaces/types exist for:
  - `SpawnSelector`
  - `ZonePopulationPlanner`
  - `EncounterPlanner`
  - `RivalAggressionResolver`
  - `PartyRolePlanner`
  - `GroupStateResolver`
  - `RespawnCooldownPolicy`
  - `ProgressionGateResolver`
  - `PartyRosterPlanner`

4.2 Keep planners free of direct emulator mutation — **Complete**
- Current planner layer performs richer pure-logic filtering/scoring but still
  does not mutate AzerothCore state directly.
- World mutation remains out of scope at this stage.

4.3 Define planner input/output contracts more explicitly — **Partial**
- Basic shapes exist.
- Still need stronger normalization around:
  - spawn budgets
  - candidate scoring
  - failure reasons
  - commit-ready action plans

### 5) Planner Stub Implementations

**Overall Status: Partial**

#### Subtasks

5.1 Add a minimal population planner stub — **Complete**
- `SimpleZonePopulationPlanner` exists as a testable pure-logic population
  planner.

5.2 Add a minimal party roster planner stub — **Complete**
- `SimplePartyRosterPlanner` exists as a small testable planner slice.

5.3 Expand stubs into policy-driven planners — **Partial**
- `SimpleZonePopulationPlanner` now owns the first useful policy pass:
  - score-based reprioritization before budget trimming
  - cooldown-aware suppression
  - spawned/dead/relocating suppression
  - activity gating from spawn context
  - unlocked-zone / nearby-zone filtering
- Remaining work should move scoring weights and policy knobs into config/data
  as real consumers appear.

5.4 Add rival-group planning logic — **Not Started**

5.5 Add progression-aware gating logic to planner outputs — **Partial**
- The zone population planner now respects `ProgressionGateResolver` plus
  `ProgressionPhaseState::unlockedZoneIds` for the player zone and candidate
  profile zone preferences.
- Broader phase-aware behavior across party/rival/economy services remains
  future work.

---

## Phase 4: Integration and World Adapters

**Overall Status: Partial**

This phase begins the first real bridge from planner outputs into AzerothCore
runtime state, while still keeping mutation centralized and thin.

### 6) AzerothCore Integration Layer

**Overall Status: Partial**

#### Subtasks

6.1 Define an `AzerothWorldFacade` or equivalent thin adapter — **Complete**
- Pure-virtual `integration::AzerothWorldFacade` now exists covering initial
  read queries: player context, spawn anchors in zone, character-online
  check.
- Pure-virtual `integration::RosterRepository` covers persistent-roster
  lookup (list-by-account, find-by-id-scoped-to-account). Scoping every
  query by account is the cross-account safety guarantee.
- Real AzerothCore-backed implementations now exist: `LiveAzerothWorldFacade`
  and `AccountAltRosterRepository` in `LivingWorldCommandScript.cpp`.
- Test-only fakes remain in place and continue to drive service-level unit tests.

6.2 Separate read context from write actions — **Complete**
- `integration::WorldReadContext.h` provides value-only input types
  (`PlayerWorldContext`, `PartySnapshot`, `SpawnAnchor`, etc.).
- Mutations are expressed only via the discriminated
  `integration::WorldCommitAction` variant.

6.3 Define world commit action types — **Partial**
- Initial action records in place:
  - `SpawnRosterBodyAction`
  - `DespawnRosterBodyAction`
  - `AttachToPartyAction`
  - `UpdateAbstractStateAction`
  - `EnqueueEncounterAction`
- Despawn/encounter actions are defined but not yet emitted by any service.

6.4 Add safety rules for authoritative world mutation — **Partial**
- Architectural rule is in place: the service layer is the only place that
  may produce `WorldCommitAction` values.
- `ExecuteCommitActions` in `LivingWorldCommandScript` is the current commit
  executor: it dispatches `SpawnRosterBodyAction` (real bot session login or
  temp-summon fallback) and `AttachToPartyAction` (real group membership).
- `DespawnRosterBodyAction` and `EnqueueEncounterAction` are defined but not
  yet emitted by any service path.

---

## Phase 5: Orchestration and Runtime Services

**Overall Status: Partial**

### 7) High-Level Services

7.0 Spawn race / roster stability note — **Partial**
- Spawn requests check pending bot login state before queueing a second request
  for the same owner or bot — done: guard in `ExecuteSpawnRosterBodyAction`.
- Roster numbering is stable by character creation order (`ORDER BY guid ASC`)
  — done.
- Self-target actions are blocked explicitly — done: `characterGuid ==
  player->GetGUID()` check in all request/dismiss/cast paths.
- Multi-bot “Summon the boys” style macro requires batch spawn orchestration
  and a 1-to-N `BotPlayerRegistry` — **not yet started**.

**Overall Status: Partial**

#### Subtasks

7.1 Introduce `LivingWorldManager` — **Partial**
- Minimal `service::LivingWorldManager` exists. It owns a reference to the
  facade, a `SimplePartyRosterPlanner`, and the first `PartyBotService`.
- Update scheduling and multi-subsystem coordination are not implemented
  yet.

7.2 Introduce `WorldPopulationService` — **Not Started**
- Player-local ambient population orchestration.

7.3 Introduce `PartyBotService` — **Complete**
- `service::PartyBotService` is implemented. It resolves the player
  context via the facade, looks up the requested entry through the
  `RosterRepository` scoped to the requesting account, enforces the
  "alt already online" rule, delegates to the party roster planner, and
  translates an approved plan into explicit `WorldCommitAction` records.
- Commit actions are executed via `ExecuteCommitActions` in the command script.
- Unit tests in `test/PartyBotServiceTest.cpp` cover: no player context,
  entry not found, cross-account scoping, approved-with-three-actions,
  alt-already-online, and dead-player paths against fake adapters.

7.4 Introduce `RivalGuildService` — **Not Started**
- Own recurring rival guild identity and encounter continuity.

7.5 Introduce `ProgressionSyncService` — **Not Started**
- Centralize phase-aware world constraints.

---

## Phase 6: Player-Controlled Party Bots

**Overall Status: Partial**

This is the first major player-facing feature direction already reflected in the
design and foundation code.

### 8) Command-Driven Playerlike Bots

**Overall Status: Partial**

#### Subtasks

8.1 Define player-facing roster flow — **Complete**
- `service::PartyBotService::DispatchRosterRequest` is the end-to-end path:
  request in, structured result + commit actions out, committed to world state.
- In-game command UX covers the full cycle: spawn, follow, cast, profile, dismiss.

8.2 Implement first command surface for controllable bots — **Partial**
- Backend grammar parser exists: `script::ParseLivingWorldCommand` produces a
  structured `ParsedCommand` consumable by both the chat command script and a
  future addon message channel.
- `script::LivingWorldCommandScript` registers `.lw` and `.lwbot` with
  AzerothCore. Both are top-level commands from a single `CommandScript` —
  no naming conflict.
- Current live command surface:
  - `.lw loglevel <1-4>` — player-local chat verbosity. Level 1 (BareMinimum)
    is the default; higher levels progressively surface detailed, debug, and
    trace output. Server logs always keep full trace regardless of this setting.
  - `.lwbot list` — list roster entries with position, name, class, level, source
  - `.lwbot request <#|name>` — spawn and party an account-alt bot
  - `.lwbot dismiss <#|name>` — dismiss active bot, runs full logout pipeline
  - `.lwbot roster list/request/dismiss` — aliases for the above
  - `.lwbot <#|name|party> profile <1-10>` — switch active combat profile slot
  - `.lwbot <#|name> cast <Ability Name> [on yourself|me|mytarget|focus|<name>]`
    — issue a natural-language cast command to a specific bot
  - `.lwbot <#|name|party> attack` — force bot(s) to attack owner's current target
  - `.lwbot <#|name|party> disengage` — stop combat and hold position briefly
  - `.lwbot <#|name|party> retreat [<duration>]` — enter retreat mode (follow +
    instant heals only) for the specified duration
  - `.lwbot <#|name|party> follow` — clear combat overrides, resume following owner
  - `.lwbot <#|name|party> refreshments` — consume food if HP < 60%, drink if
    mana < 60%
  - `.lwbot <#|name|party> buff` — force re-apply OOC class maintenance buffs
    immediately, bypassing the normal OOC guard
- `party` is a valid bot reference for all commands except `cast` and `request/dismiss`.
  It fans out to all bots currently registered to the owner.
- Quest UX command surface now also exists:
  - `.lwbot quests` — push current pending bot reward-choice state to the addon
  - `.lwbot questmode <smart|manual>` — toggle bot reward behavior per owner
  - `.lwbot <#|name> reward <questId> <choiceNumber>` — apply a manual
    reward-choice selection for a specific active bot
- Full `[LivingWorldDebug]` trace remains server-log-first.
- Switch control/possession target is still not implemented.

8.3 Add party slot/rule validation — **Partial**
- Party size limits — done: `partyMemberCount >= maxPartyMembers` rejection in
  the planner.
- Duplicate roster restrictions — done: `isAlreadySummoned` check blocks
  re-requesting an already-online alt.
- Ownership rules — done: `OwnershipMismatch` rejection and account-scoped
  roster queries.
- Combat-state restrictions — not yet: no explicit block on requesting a bot
  while in combat.

8.4 Add follow/assist/control mode definitions — **Not Started**

8.5 Define possession rules separate from spawn rules — **Not Started**
- A bot being active in the world is not the same as the player actively
  controlling it.

### 9) Account Alt Support

**Overall Status: Partial**

#### Subtasks

9.1 Define owned-alt eligibility rules — **Complete**
- Only alts from the player's account are eligible. Enforced via account-scoped
  roster queries (`source_account_id = {}`), `OwnershipMismatch` planner
  rejection, and `PartyBotService`'s pre-planner cross-account guard.

9.2 Define runtime representation for account-derived roster entries — **Partial**
- `model::AccountAltRuntimeRecord` and
  `service::AccountAltRuntimeService` now define the first clone-account
  lifecycle seam: prepare a new runtime clone, reuse an active clone, recover
  an interrupted clone, or block when no bot account is available.
- Runtime clones are intended to live on bot-owned account-pool accounts rather
  than by rewriting AzerothCore's one-active-session-per-account assumption.
- New clone materialization now begins the safer exact-name path: the offline
  source alt can be parked under its reserved hidden name so the runtime clone
  can lease the real player-facing alt name during spawn.
- `AccountAltRecoveryService` now defines the first pure recovery-plan seam:
  clone progress can be reused, blocked, routed to manual review, or synced
  back to the source only after sanity checks identify safe domains.

9.3 Define progression for XP / items / rep ownership — **Partial**
- The runtime model now carries source/clone progress snapshots and marks the
  clone as authoritative during recovery when clone progress is ahead of the
  current source snapshot.
- Sync-domain types distinguish XP, money, inventory, equipment, reputation,
  quests, achievements, and mail.
- Clone→source sync is now implemented for: level/XP/money, equipment,
  inventory, bank, reputations, quest completions, and achievements.
- Quest sync now also preserves active/in-progress quest rows and objective
  counters correctly during bot dismiss/logout recovery.
- Quest completion with choice rewards now has a bot-facing reward pipeline:
  smart auto-pick by class/item fit when the answer is clear, otherwise leave
  the reward pending for manual selection in the addon.
- `CharacterProgressSnapshot` now carries `completedQuestCount`,
  `achievementCount`, and `totalReputationStanding` for tiebreaking in
  "clone is ahead" comparisons across both `CloneProgressIsAhead` and
  `SourceProgressIsAhead`.
- Auras/buffs are intentionally excluded: `character_aura` rows carry stale
  `remaintime` and `caster_guid` pointing to the clone GUID, making them
  incorrect for the source character. They are ephemeral and reapplied
  naturally through gameplay.
- Mail domain remains unimplemented.

9.4 Block conflicting login/runtime states — **Partial**
- Need explicit rules for:
  - alt already online
  - alt already active as bot
  - persistence/save timing
- `PartyBotService` already blocks owned alts that are online as normal
  characters. `AccountAltRuntimeService` now blocks/reuses/recovers existing
  runtime records before reserving a new bot account.
- `LivingWorldPlayerScript::OnPlayerLogin` now runs a lightweight owner-login
  recovery pass. It retries `SyncingBack` progress-only runtimes, reports
  pending recovery when a materialized clone is ahead, and surfaces manual
  review / blocked counts without performing broader destructive sync.
- `LivingWorldPlayerScript::OnPlayerUpdate` now drives the stock trade flow for
  owner-controlled bot-session clones: it auto-opens the trade window on the
  bot side, then auto-confirms only after the real player clicks accept. The
  bot still uses AzerothCore's native trade handlers, so inventory-space and
  trade-validity checks remain authoritative. This is the current in-game test
  seam for account-alt inventory persistence.
- Owner-triggered clone dismissal now starts from
  `LivingWorldPlayerScript::OnPlayerBeforeLogout`, and it calls the bot
  session's real `LogoutPlayer(true)` path rather than `KickPlayer()`. This is
  important for socketless bot sessions because recovery/name-release/item-sync
  work lives on the normal logout path.
- Clean dismissal now retires the runtime row after successful sync/name
  restore instead of leaving a stale `Active` record behind. Bot accounts stay
  reserved to the source alt, and fresh spawn on that reserved account now
  deletes any stale leftover clone body before rebuilding from the current
  source state. This prevents old offline clone equipment from being treated as
  authoritative after the real source alt logs in and changes gear/items.

9.5 Decide whether generic bots and account alts share one runtime pipeline — **Not Started**

---

## Phase 7: Ambient World and Rival Guild Population

**Overall Status: Not Started**

### 10) Ambient Population

**Overall Status: Not Started**

#### Subtasks

10.1 City ambient population planning — **Not Started**
10.2 Travel corridor population planning — **Not Started**
10.3 Outdoor activity planning — **Not Started**
10.4 Despawn/respawn budget rules — **Not Started**
10.5 Abstract-state cooling and relocation rules — **Not Started**

### 11) Rival Guild System

**Overall Status: Not Started**

#### Subtasks

11.1 Persistent rival guild roster model — **Not Started**
11.2 Rival group size/composition policy — **Not Started**
11.3 Alert / engaged / disengage group states — **Not Started**
11.4 Personality-driven caution/aggression rules — **Not Started**
11.5 Encounter continuity/history tracking — **Not Started**

---

## Phase 8: Progression-Aware World Rules

**Overall Status: Not Started**

### 12) Progression and Content Gating

**Overall Status: Not Started**

#### Subtasks

12.1 Define phase-state representation for Classic/TBC/Wrath simulation — **Not Started**

12.2 Add progression-aware filters to planners — **Not Started**

12.3 Centralize content unlock rules outside planner call sites — **Not Started**

12.4 Decide module split between `mod-living-world` and optional progression module — **Not Started**

---

## Phase 9: Persistence, Config, and Data

**Overall Status: Not Started**

### 13) SQL / Config / Tuning Surfaces

**Overall Status: Not Started**

#### Subtasks

13.1 Add initial `db-world` / `db-characters` schema for living-world data — **Partial**
- `living_world_account_alt_runtime` now has a SQL-backed repository,
  progress snapshot loader, progress sync repository/executor, and owner-login
  startup recovery pass for `SyncingBack` crash retries.
- Clone→source sync repositories now exist for all non-ephemeral domains:
  `SqlCharacterReputationSyncRepository` (merge by max standing per faction),
  `SqlCharacterQuestSyncRepository` (INSERT IGNORE into rewarded quests), and
  `SqlCharacterAchievementSyncRepository` (INSERT IGNORE for achievements +
  max-counter merge for criteria progress).
- Remaining work is mail sync and broader lifecycle persistence hardening.

13.2 Define tunable config values — **Not Started**
- Examples:
  - local population caps
  - rival encounter cooldowns
  - roster limits
  - aggression weights
  - abstract-state timers

13.3 Add seed/default data for bot identities and rival guilds — **Not Started**

13.4 Separate tuning data from hardcoded logic — **Not Started**

---

## Phase 10: Validation and Tooling

**Overall Status: Partial**

### 14) Testing Strategy

**Overall Status: Partial**

#### Subtasks

14.1 Keep planner logic testable without world mutation — **Partial**
- Initial unit tests exist for the planner stubs.

14.2 Add more planner contract tests — **Not Started**

14.3 Add service-level tests once orchestration exists — **Not Started**

14.4 Add regression tests for Windows builds — **Partial**
- Current build/test fixes are in place, but no CI automation exists yet.

### 15) Documentation

**Overall Status: Partial**

#### Subtasks

15.1 Keep `ai-azerothcore.md` aligned with implementation — **Partial**

15.2 Keep this roadmap current as features land — **Complete**
- This roadmap replaces the old emulator-specific task list for this project.

15.3 Add developer setup notes for working presets/builds — **Partial**

---

## Completed Slice: Persistent Runtime Records and Recovery Planning

The null-socket bot-session slice landed. This slice added durable account-alt
recovery so crashes do not strand progress on bot-owned clone characters or
overwrite good source-character data.

### A) Persistent runtime records

Add and wire `living_world_account_alt_runtime` in the characters DB. This
records the source account/character, the last owner character that requested
the bot, the bot account, the clone character, runtime state, source/clone
progress snapshots, and last clean sync/recovery timestamps.

The repository contract now needs a SQL-backed implementation for:
- find by source account + source character
- list recoverable records for an account on player login
- save state/snapshot transitions transactionally

Current implementation status:
- `SqlAccountAltRuntimeRepository` can read/write
  `living_world_account_alt_runtime`.
- `SqlCharacterProgressSnapshotRepository` can read level, XP, and money from
  `characters`.
- `AccountAltRuntimeCoordinator` now wires those repositories into the
  account-alt spawn path before `BotSessionFactory` queues a login.
- `SqlCharacterCloneMaterializer` now uses AzerothCore's `PlayerDump`
  import/export path to create or reuse a persistent clone character on the
  reserved bot account before spawn. New clone materialization now leases the
  real source character name to the clone by first parking the offline source
  alt under its reserved hidden name.
- If a materialized clone exists and appears ahead of the source snapshot, the
  command path now blocks the spawn for manual review instead of guessing.
- Clone-to-source sync writes now exist for level / XP / money only, gated by
  `AccountAltSanityChecker` and `AccountAltSyncExecutor`.

### B) Recovery planning before spawning

Before `.lwbot roster request <id>` queues a bot login, route account-alt
entries through the runtime service and recovery planner:
- no existing runtime: reserve a bot account and prepare a persistent clone
- active runtime: reuse the clone rather than allocate a new account
- interrupted runtime: build a recovery plan before spawning
- failed/incomplete runtime: block rather than guessing

### C) Sanity-check layer — **Complete**

`service::AccountAltSanityChecker` compares source and clone snapshots and
returns an `AccountAltSanityCheckResult` with approved sync domains.

Rules implemented:
- Level delta must not exceed 5 (a conservative per-session cap). Larger
  deltas indicate a data anomaly and send the runtime to manual review.
- Money gain must not exceed 5,000,000 copper (500 gold). Gains above the
  cap are similarly sent to manual review.
- Approved safe domains when both pass: `Experience` and `Money`.
- Inventory, equipment, reputation, quests, and mail are never approved here.

`AccountAltRuntimeCoordinator::PlanSpawn` now calls the checker instead of
the previous hardcoded `passed = false` stub, so clone-ahead runtimes with
plausible progress will reach `SyncCloneToSource` instead of always landing
in `ManualReviewRequired`.

7 unit tests cover: equal snapshots, small level gain, exact delta cap,
exceeded level cap, small money gain, exceeded money cap, and both
checks failing together.

### D) Sync executor — **Complete**

`service::AccountAltSyncExecutor` executes the domain-restricted progress copy
inside a `SyncingBack` state guard. Implementation details:

- Constructs a target snapshot from the clone, limited to `domainsToSync`.
- Marks runtime `SyncingBack` and saves before any write (crash-safe).
- Calls `integration::CharacterProgressSyncRepository::SyncProgressToCharacter`
  to UPDATE `characters` (level, xp, money) using `DirectExecute` so the row
  is committed before a bot session loads the character.
- Marks runtime `Active` with the new source snapshot after success.
- Returns false on executor write failure; coordinator then returns
  `ManualReviewRequired`.

`AccountAltRuntimeCoordinator::PlanSpawn` now calls the executor inline when
`SyncCloneToSource` is the plan, and returns `SpawnUsingPersistentClone` on
success. The `RecoveryRequired` spawn decision kind is no longer reachable in
the clone-ahead path.

Inventory, equipment, bank, achievements, quests, reputation, and mail remain
blocked until they have domain-specific sanity rules and ownership checks.

### E) Startup/player-login recovery pass — **Complete**

Crash/forced-close recovery now starts early enough to fix character-select
state instead of waiting for the source character to fully enter the world.

- `LivingWorldAccountScript::OnAccountLogin` now runs on successful account auth
  before character enumeration.
- That hook routes recoverable runtimes through the existing authoritative
  dismissal/recovery path (`AccountAltDismissalService::DismissClone`) so the
  source character can receive:
  - progress sync
  - approved equipment/inventory/bank sync
  - source-name lease restoration
  - runtime retirement after successful cleanup
- Live validation now confirms a forced client-close path can recover item state
  on next login without leaving visible reserved-name issues on the character list.

Group-roster cleanup remains split into two phases for safety:
- `CleanupStaleGroupBots` still detects/logs offline clone members during owner login.
- A deferred event after player spawn now removes those offline clone members
  from the group, avoiding the earlier login-time `Group::RemoveMember` crash path
  while still giving the owner a clean party roster shortly after entry.

---

## Completed Slice: Bot Player Sessions

The architecture decision for real bot players has been made and fully audited
(see `ai-azerothcore.md` section 20). The previous note saying "do not attempt
a socketless session" is superseded — a minimal core patch makes it viable.
This slice implemented that patch and connected it to the module.

### A) Core patch — WorldSession bot mode (4 files, all small)

This is the foundational change everything else builds on.

**`src/server/game/Server/WorldSession.h`**
- Add private member `bool m_isBotSession = false;`
- Add private member `ObjectGuid m_botLoginTarget;`
- Add public `void EnableBotMode() { m_isBotSession = true; }`
- Add public `bool IsBotSession() const { return m_isBotSession; }`
- Add public `void SetBotLoginTarget(ObjectGuid guid) { m_botLoginTarget = guid; }`
- Add public `ObjectGuid GetBotLoginTarget() const { return m_botLoginTarget; }`
- Add public declaration `bool StartBotLogin(ObjectGuid const& guid);`

**`src/server/game/Server/WorldSession.cpp`** — one guard in `Update()`:
```cpp
// Change: if (!m_Socket) return false;
// To:
if (!m_Socket && (!m_isBotSession || IsKicked()))
    return false;
```

**`src/server/game/Handlers/CharacterHandler.cpp`** — new method alongside
the existing login handlers. Uses the already-defined local `LoginQueryHolder`
class:
```cpp
bool WorldSession::StartBotLogin(ObjectGuid const& guid)
{
    ASSERT(m_isBotSession);
    auto holder = std::make_shared<LoginQueryHolder>(GetAccountId(), guid);
    if (!holder->Initialize())
        return false;
    m_playerLoading = true;
    CharacterDatabase.DelayQueryHolder(holder,
        [this](std::shared_ptr<CharacterDatabaseQueryHolder> h)
        {
            HandlePlayerLoginFromDB(static_cast<LoginQueryHolder&>(*h));
        });
    return true;
}
```

**`src/server/game/Server/WorldSessionMgr.cpp`** — three lines at the end of
`AddSession_()`, after the existing `session->InitializeSession()` call:
```cpp
if (session->IsBotSession() && session->GetBotLoginTarget().IsPlayer())
    session->StartBotLogin(session->GetBotLoginTarget());
```

All `SendPacket` calls inside `HandlePlayerLoginFromDB` and
`SendInitialPacketsBeforeAddToMap` / `SendInitialPacketsAfterAddToMap` are
already no-ops for null-socket sessions — no additional changes needed there.

### B) Module — BotSessionFactory (new integration class)

New file: `modules/mod-living-world/src/integration/BotSessionFactory.h/.cpp`

The factory queries `acore_auth.account` for the bot account details
(name, expansion, locale), constructs a `WorldSession` with a null socket,
calls `EnableBotMode()` and `SetBotLoginTarget(characterGuid)`, then calls
`sWorldSessionMgr->AddSession(session)`. The `AddSession_` hook then triggers
`StartBotLogin` automatically.

Bot accounts are separate accounts in `acore_auth.account` — never the same
account as the requesting player. The session map is keyed by account ID so
there is no collision with the player's session.

### C) Module — BotPlayerRegistry (new service class)

New file: `modules/mod-living-world/src/service/BotPlayerRegistry.h/.cpp`

Tracks active bot players: `ownerCharGuid → botPlayer*`. Updated when a bot
player enters the world via `PlayerScript::OnLogin` (detecting `IsBotSession()`
on the player's session). Provides lookup for the companion AI tick.

### D) Module — CompanionAI (new ai class)

New file: `modules/mod-living-world/src/ai/CompanionAI.h/.cpp`

Drives a real `Player*` as a companion. Scheduled as a repeating
`BasicEvent` on the bot player's own event processor so it runs on the map
thread (thread-safe). Each tick (~500 ms).

Role classification (`GetCombatRole`) maps class to one of four combat roles:

| Role | Classes |
|---|---|
| Healer | Priest |
| HybridHealer | Druid, Paladin, Shaman |
| Ranged | Mage, Warlock, Hunter |
| Melee | Warrior, Rogue, Death Knight |

Combat behaviour per role:
- **Healer**: monitors owner health continuously; applies fast/sustained heals
  regardless of combat state; Power Word: Shield pre-pull for Priests.
- **HybridHealer**: heals first when owner is below threshold; otherwise closes
  to melee, autoattacks, and casts class-specific damage spells (Crusader Strike,
  Flame Shock, Moonfire, Hammer of Wrath, etc.).
- **Ranged**: three-zone positioning — retreats when target is within 8y,
  approaches via `MoveChase(target, 25y)` when target is beyond 30y (cast range),
  casts damage spells in the 8–30y sweet spot. Falls back to melee autoattack
  when OOM.
- **Melee**: chases victim with `MoveChase`, casts class-appropriate offensive
  spells (Execute, Rend, DK disease rotation, etc.).

Assist target resolution: sticks to current victim → owner's victim → owner's
right-click selection (hunter-pet "Attack" semantic).

Out-of-combat: applies maintenance buffs (class-specific), follows owner at 2y.

The event chain has a `MaxNotInWorldRetries = 20` cap with exponential backoff
(500 ms → 1 s → 2 s → 4 s) to handle mid-login bot state without runaway
rescheduling.

### E) Module — script hooks

New file: `modules/mod-living-world/src/script/LivingWorldPlayerScript.cpp`
- `OnLogin`: if `player->GetSession()->IsBotSession()`, register with
  `BotPlayerRegistry` and schedule the first `CompanionAIEvent` on the player.

New file: `modules/mod-living-world/src/script/LivingWorldWorldScript.cpp`
- Reserved for future world-tick orchestration. Stub only in this slice.

### F) Module — command script update

`LivingWorldCommandScript.cpp` `ExecuteSpawnRosterBodyAction` currently
spawns a `TempSummon` with template 111/112 as a stand-in. Replace with a
call to `BotSessionFactory::SpawnBotPlayer(botAccountId, characterGuid,
ownerGuid)`. The template 111/112 path can remain as a fallback for generic
(non-account-alt) roster entries.

### G) SQL

`data/sql/updates/pending_db_auth/` — bot account pool reservation table:
```sql
CREATE TABLE IF NOT EXISTS `living_world_bot_account_pool` (
    `account_id`   INT UNSIGNED NOT NULL,
    `is_available` TINYINT(1)   NOT NULL DEFAULT 1,
    `reserved_for` BIGINT UNSIGNED NULL,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB;
```

Bot account rows in `acore_auth.account` must be created manually for initial
testing. The factory reads from this pool table to select a free account.

---

## Completed Slice: Party Membership + Dismiss

The bot player now joins the owner's real `Group` on login and is cleanly
removed from it on logout or dismiss.

### A) OnPlayerLogin — group join

`LivingWorldPlayerScript::OnPlayerLogin` calls `AddBotToOwnerGroup(bot, owner)`
immediately after `ScheduleCompanionAI`. The helper:
- Reads `owner->GetGroup()`. If none exists, creates one via `Group::Create`
  and registers it with `sGroupMgr->AddGroup`.
- Skips if the group is already full (`IsFull()`).
- Calls `group->AddMember(bot)`.

This makes the bot a real party member in the client UI from the moment it
enters the world.

### B) Owner/bot logout hooks - dismiss + cleanup

`LogoutPlayer` in core skips automatic group removal when the session has no
socket (the `m_Socket &&` guard around the group-removal block). Bot sessions
have no socket, so they were silently left in groups forever.

`LivingWorldPlayerScript` now splits the responsibilities across the safe hooks:
- `OnPlayerBeforeLogout` on the real owner starts controlled-bot dismissal
  early and calls the bot session's real `LogoutPlayer(true)` path.
- `OnPlayerLogout` on the bot explicitly removes the bot from the group before
  unregistering it and running dismissal recovery.

The bot branch explicitly calls:
```cpp
if (Group* group = player->GetGroup())
    group->RemoveMember(player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
```
This fires before `UnregisterBotPlayer` and the bot-pool DB release so the
group is clean before clone recovery/name-release/item-sync work runs.

### C) `.lwbot roster dismiss <id>` - real implementation

`RenderDismissBot` replaced the old `RenderDismissPlaceholder`. It:
1. Looks up the owner's active bot via `BotPlayerRegistry::FindBotForOwner`.
2. Removes the bot from its group with `GROUP_REMOVEMETHOD_LEAVE`.
3. Calls `bot->GetSession()->LogoutPlayer(true)` so the stock logout pipeline
   runs immediately for the socketless bot session, triggering
   `OnPlayerBeforeLogout`/`OnPlayerLogout`, clone dismissal recovery, and
   registry cleanup in the same authoritative path.

---

## Completed Slice: Account-Alt Sync Executor

See section D of "Completed Slice: Persistent Runtime Records and Recovery Planning" above for full detail.

---

## Completed Slice: Bag-Domain Policy Surface and Container Tightening

### A) Config-driven bag-domain policy — **Complete**

`mod-living-world.conf.dist` now exposes two operator-controlled knobs:
- `LivingWorld.AccountAlt.EnableInventorySync = 1`
- `LivingWorld.AccountAlt.EnableBankSync = 1`

Both now default to enabled so account alts keep persistent inventory and bank
state with their source character unless an operator explicitly turns them off.
All three service construction sites that accept
`AccountAltItemRecoveryOptions` — `AccountAltRuntimeCoordinator` in
`LivingWorldCommandScript`, and `AccountAltStartupRecoveryService` /
`AccountAltDismissalService` in `LivingWorldPlayerScript` — now read these
values via `sConfigMgr->GetOption<bool>` and pass them through instead of
using the hardcoded struct default.

### B) Per-container item cap — **Complete**

`CharacterItemSanityChecker` now rejects snapshots where any single bag
container holds more items than that specific bag can legally contain.
The checker now prefers the container item's `ItemTemplate::ContainerSlots`
value, so normal bags follow their real slot count and profession bags follow
their own larger cap. When template metadata is unavailable, it falls back to
a conservative 32-slot limit rather than assuming oversized bag capacity. A
snapshot that exceeds the resolved container capacity fails with:
`"inventory/bank snapshot has a container exceeding the bag size cap"`.

### C) Bag-container-change detection - **Complete**

`AccountAltSanityCheckResult` has a new `bagContainersChanged` field.
`CharacterItemSanityChecker::Check` sets it when the bag type (itemEntry) at
any root inventory bag slot (19–22) or root bank bag slot (67–73) differs
between the source and clone snapshots. The field is computed unconditionally
so the recovery service can use it even when all other checks pass.

`AccountAltItemRecoveryService::BuildRecoveryPlan` now routes to `ManualReview`
when `bagContainersChanged` is true and inventory or bank domains differ. This
prevents automated sync when the underlying bag containers themselves changed —
a case where the container-guid remapping done by the sync executor could
produce unexpected layouts that require human review.

Tests added:
- `RejectsExcessiveContainerSize` - 37 items inside one bag fails
- `SetsBagContainersChangedWhenInventoryBagsDiffer` - different bag type at slot 19
- `SetsBagContainersChangedWhenBankBagsDiffer` - different bag type at slot 67
- `DoesNotSetBagContainersChangedWhenOnlyContentsDiffer` - same bag type, different contents
- `RequiresManualReviewWhenBagContainersChangedEvenIfPolicyEnabled` - ManualReview overrides policy

### D) Review-fix hardening pass - **Complete**

The account-alt recovery path received a focused review-fix pass after the
bag-domain policy work. Fixes landed:
- name reclaim no longer rejects the logging-out clone itself, so the normal
  bot logout path can restore the source name during dismissal
- `SyncingEquipment` is included in `ListRecoverableForAccount`, so interrupted
  equipment sync can actually be retried on owner login
- inventory/bank snapshot comparison now uses logical container paths instead
  of clone-specific item GUIDs, preventing false differences when source and
  clone bags have the same layout but different GUIDs
- item sanity now rejects duplicate nested slots inside the same container
- equipment recovery now takes priority when equipment and bag domains both
  differ, so disabled inventory/bank sync does not block approved equipment
  recovery

Tests added:
- logical bag contents match across different source/clone GUIDs
- duplicate nested container slots are rejected
- equipment sync is planned before blocked inventory when both domains differ

Important fixture note for future agents:
- tests that intend to model "inventory contents changed" must keep the root
  bag container type the same on source and clone
- changing the itemEntry of the bag in slots 19-22 or 67-73 is now treated as
  a real bag-container change and should correctly route to manual review
  instead of bag sync

---

---

## Completed Slice: Reputation, Quest, and Achievement Sync (Clone→Source)

This slice completed the remaining additive data-transfer paths from the clone
character back to the source character. Together with the earlier item-sync
slice, all non-ephemeral progression domains are now implemented.

### A) Model layer extensions — **Complete**

`CharacterProgressSnapshot` now carries three new fields:
- `completedQuestCount` — count of rewarded quests
- `achievementCount` — count of completed achievements
- `totalReputationStanding` — sum of all non-negative faction standings

`AccountAltSyncDomain` enum gained the `Achievements` entry alongside the
previously-added `Reputation` and `Quests` entries.

Both "is ahead" comparison functions (`CloneProgressIsAhead` in
`AccountAltRecoveryService` and `SourceProgressIsAhead` in
`AccountAltRuntimeCoordinator`) now cascade through all six fields in order:
level → experience → money → completedQuestCount → achievementCount →
totalReputationStanding.

### B) Snapshot repository extension — **Complete**

`SqlCharacterProgressSnapshotRepository` query extended with three correlated
subqueries:
- `COUNT(*)` from `character_queststatus_rewarded`
- `COUNT(*)` from `character_achievement`
- `SUM(GREATEST(standing, 0))` from `character_reputation`

### C) Integration repositories — **Complete**

Three new SQL-backed write repositories, each implementing a pure-virtual
interface:

**`SqlCharacterReputationSyncRepository`**
```sql
INSERT INTO character_reputation (guid, faction, standing, flags)
SELECT {sourceGuid}, faction, standing, flags
FROM character_reputation WHERE guid = {cloneGuid}
ON DUPLICATE KEY UPDATE
  standing = IF(VALUES(standing) > standing, VALUES(standing), standing),
  flags = VALUES(flags)
```

**`SqlCharacterQuestSyncRepository`**
```sql
INSERT IGNORE INTO character_queststatus_rewarded (guid, quest, active)
SELECT {sourceGuid}, quest, active
FROM character_queststatus_rewarded WHERE guid = {cloneGuid}
```

**`SqlCharacterAchievementSyncRepository`**
```sql
-- Completed achievements:
INSERT IGNORE INTO character_achievement (guid, achievement, date)
SELECT {sourceGuid}, achievement, date FROM character_achievement WHERE guid = {cloneGuid}
-- Criteria progress (take max counter):
INSERT INTO character_achievement_progress (guid, criteria, counter, date)
SELECT {sourceGuid}, criteria, counter, date FROM character_achievement_progress WHERE guid = {cloneGuid}
ON DUPLICATE KEY UPDATE
  counter = IF(VALUES(counter) > counter, VALUES(counter), counter),
  date = IF(VALUES(counter) > counter, VALUES(date), date)
```

### D) Sanity checker — **Complete**

`AccountAltSanityChecker` always marks Reputation, Quests, and Achievements as
safe domains when clone counts ≥ source counts. These domains are additive-only
so they can never lose source progress.

### E) Service layer wiring — **Complete**

- `AccountAltSyncExecutor` now handles Reputation, Quests, and Achievements
  domains after writing progress.
- `AccountAltDismissalService` calls all three repos unconditionally before the
  recovery plan check, so additive progress is preserved even in `ReuseClone`
  sessions where no XP/gold was earned.
- `AccountAltStartupRecoveryService` and `AccountAltRuntimeCoordinator` pass
  the three new repos through to `AccountAltSyncExecutor` construction.

All three service construction sites in `LivingWorldCommandScript` and
`LivingWorldPlayerScript` updated to supply the new SQL repo instances.

### F) Intentional exclusion: auras/buffs — **Complete (by design)**

`character_aura` is intentionally not synced. Reasons:
- `remaintime` is stale the moment the clone logs out.
- `caster_guid` references the clone's GUID, not the source's.
- Buffs are ephemeral and naturally reapplied through gameplay.
Syncing them would inject bad data rather than preserve progress.

### G) Tests updated — **Complete**

`AccountAltDismissalServiceTest`, `AccountAltStartupRecoveryServiceTest`, and
`AccountAltRuntimeCoordinatorTest` all updated with fake implementations of the
three new repository interfaces and updated constructor call sites.

---

## Completed Slice: Party Bot Combat AI and Control Expansion

This slice added healer mana management, party-wide command fanout, follow/
refreshments/buff commands, OOC class buffs for Warlock/Priest/Mage/Druid, and
a full LivingWorld Control Panel addon overhaul.

### A) Healer hybrid mana management — **Complete**

`CompanionAI` now implements hysteresis mana thresholds for pure Healer bots:
- Stop casting offensive spells when mana falls below 40% (`HealerManaConserveBelow`)
- Resume when mana recovers above 60% (`HealerManaResumeAbove`)
- The `_healerConserving` bool is carried through the `CompanionAIEvent` chain
  so it survives re-scheduling without oscillation at the boundaries.
- Priest healer path now casts SW:Pain → Mind Blast → Smite offensively when
  not conserving and a valid assist target exists.
- Healer branch in `Tick()` now calls `TryApplyOutOfCombatBuff` so healers
  apply maintenance buffs outside combat (was previously only reached by
  non-healer roles).

### B) Party-wide command fanout — **Complete**

Grammar parser now accepts `party` as a bot reference. `IsPartyBotRef()` detects
it and `ResolveSelectedBotsForOwner()` fans the command out to all bots currently
registered to the owner. All commands except `cast`, `request`, and `dismiss`
accept `party`.

### C) Follow / Refreshments / Buff commands — **Complete**

Three new `.lwbot` commands:
- `follow` — clears combat overrides, calls `AttackStop()` + `MoveFollow(owner)`.
- `refreshments` — inventory scan via `ItemTemplate::Spells[i].SpellCategory`
  (11=food, 59=drink); uses food if HP < 60%, drink if mana < 60%.
- `buff` — calls `ForceBotBuffRefresh(bot, owner)` which applies maintenance
  buffs immediately, bypassing the OOC guard. `ForceBotBuffRefresh` is the
  public API wrapping the internal `ApplyBotBuff`.

### D) OOC class maintenance buffs expanded — **Complete**

`ApplyBotBuff` now handles:
- `CLASS_WARLOCK`: Fel Armor → Demon Armor → Demon Skin (self-only, chain
  fallback).
- `CLASS_PRIEST`: Power Word: Fortitude buffed on all group members (one per
  tick). Uses `group->GetMemberSlots()` + `ObjectAccessor::FindConnectedPlayer`.
- `CLASS_MAGE`: Arcane Intellect buffed on all group members (one per tick).
- `CLASS_DRUID`: Mark of the Wild buffed on all group members (one per tick).

### E) Commanded caster combat-lock stabilization — **Partial**

Follow-up live testing found that `.lwbot attack` needed a stricter separation
from normal assist/follow heuristics, especially for casters.

Stabilization now in place:
- `attackLocked` is a command-latched combat mode distinct from transient engine
  `IsInCombat()` flags.
- Commanded targets now use a looser viability test than normal assist targets,
  so early pull / LoS / engagement flicker does not immediately collapse the
  target context.
- `BreakFollowForAttack` clears stale `FOLLOW_MOTION_TYPE` movement when a
  commanded attack starts, so near-target owner repositioning no longer drags
  casters along via the old follow generator.

Current status from live validation:
- long-range commanded pulls improved substantially
- near-target reposition behavior is improved and considered workable for now
- further polish may still be needed if other movement generators show up in logs

### E) LivingWorld Control Panel addon — **Complete**

- Slot 0 is a permanent `Party` entry; slot selector wraps through it.
  Commands that require a single target (Cast, Spawn, Dismiss) guard against
  Party selection.
- `GetBotRef()` returns `"party"` for slot 0, character name for all others.
- Auto-refresh roster on panel show; `CHAT_MSG_SYSTEM` event parses roster
  list lines and filters out the logged-in player's own character.
- New buttons: Follow, Eat/Drink (Refreshments), Buff.
- Gear tab now supports direct bot inventory management through addon-backed
  chat commands: right-click bag item to retrieve, left-click bag item then
  left-click gear slot to equip, and right-click equipped gear slot to unequip
  back into the bot's bags.
- Ctrl+right-click on a stacked bag item now opens a quantity prompt and sends
  `.lwbot <bot> retrieve <itemGuid> <count>` so partial stack retrieval is
  available without dragging the full stack to the player.
- Log level +/- auto-sends the command immediately; Set button removed.

---

## Completed Slice: Quest Log Sync to Account-Alt Bots

When the player accepts or abandons a quest, all active account-alt bots owned
by that player now mirror the change automatically.

### A) Core patch — OnPlayerQuestAccept (4 files)

`PlayerScript` had no player-level quest-accept hook. A minimal patch adds one
following the same pattern as the existing bot-session patch:

- `src/server/game/Scripting/ScriptDefines/PlayerScript.h` — added
  `PLAYERHOOK_ON_QUEST_ACCEPT` enum entry and `virtual void
  OnPlayerQuestAccept(Player*, Quest const*)` virtual method.
- `src/server/game/Scripting/ScriptDefines/PlayerScript.cpp` — added
  `ScriptMgr::OnPlayerQuestAccept` dispatcher using `CALL_ENABLED_HOOKS`.
- `src/server/game/Scripting/ScriptMgr.h` — added `OnPlayerQuestAccept`
  declaration alongside the existing `OnPlayerQuestAbandon`.
- `src/server/game/Entities/Player/PlayerQuest.cpp` — added
  `sScriptMgr->OnPlayerQuestAccept(this, quest)` at the end of
  `AddQuestAndCheckCompletion`, after all per-giver script hooks have fired.

### B) Module — LivingWorldPlayerScript hooks

`OnPlayerQuestAccept` — fires after the player's own accept is committed.
Skips bot sessions. Iterates `BotPlayerRegistry::FindBotsForOwner`, checks
`GetQuestStatus == QUEST_STATUS_NONE`, runs `CanTakeQuest` + `CanAddQuest`
eligibility, then calls `bot->AddQuestAndCheckCompletion(quest, nullptr)`.
Passing `nullptr` as questGiver is safe: no per-giver script hooks fire and
the quest is added cleanly.

`OnPlayerQuestAbandon` — fires after the player's own abandon. Skips bot
sessions. Iterates the same registry, checks that the bot actually holds the
quest (`status != QUEST_STATUS_NONE && status != QUEST_STATUS_REWARDED`),
then calls `bot->AbandonQuest(questId)` + `bot->RemoveActiveQuest(questId)`.

Both paths log at `LOG_INFO` on action and `LOG_DEBUG` on skip with
`[LivingWorldDebug]` prefix.

### C) Bug fix — active quest sync and SaveToDB timing

Live validation revealed two bugs that prevented the source character from
seeing accepted quests after a bot session:

**Bug 1 — SaveToDB timing**: `OnPlayerLogout` fires before the normal
`SaveToDB` call inside AzerothCore's logout path. The bot's in-memory quest
state (written by `AddQuestAndCheckCompletion`) was never in
`character_queststatus` at the point the dismissal service read it. Fix: call
`player->SaveToDB(false, false)` explicitly at the top of the bot
`OnPlayerLogout` branch, before `RunBotDismissalRecovery`.

**Bug 2 — rewarded-only sync**: `SyncQuestsFromCloneToSource` only copied
`character_queststatus_rewarded` (completed quests). Active/in-progress quests
live in `character_queststatus` and were never transferred to the source. Fix:
added a second `DirectExecute` in `SqlCharacterQuestSyncRepository`:
```sql
INSERT IGNORE INTO character_queststatus
  (guid, quest, status, explored, timer,
   mobcount1..4, itemcount1..6, playercount)
SELECT {sourceGuid}, quest, status, ...
FROM character_queststatus WHERE guid = {cloneGuid} AND status != 0
```
`INSERT IGNORE` prevents overwriting a quest the source already holds.
`status != 0` filters stale zero-rows left by AzerothCore after quest removal.

Later hardening:
- active quest sync can also be blocked by stale zero-status rows already
  present on the source character because `(guid, quest)` is the primary key.
- `SqlCharacterQuestSyncRepository` now deletes only those stale zero-status
  source rows for quests the clone currently has active, then uses
  `INSERT ... ON DUPLICATE KEY UPDATE` with `GREATEST(...)` progress merges
  instead of relying on `INSERT IGNORE` alone.
- repository debug logs now emit pre/post active-quest row counts under the
  `[LivingWorldDebug] QuestSync repo ...` prefix to make live validation easier.

After both fixes: owner accepts quest → bot gets it in memory → on dismiss,
bot flushes to DB → sync copies active rows to source → source logs in and
sees the quest in their quest log.

---

## Completed Slice: Spell and Skill Sync (Clone→Source) + Live Propagation

Bot clones are created from a PlayerDump of the owner, so they start with the
owner's full spell and skill state. The remaining gap is spells and skills
gained *after* the clone is active, including spells learned at a trainer and
talent spells learned via the player's talent panel.

### A) Dismissal sync — additive write paths

Both domains are monotonically increasing during normal play (no training
rollback, no skill loss in ordinary gameplay). They are therefore safe to sync
unconditionally in the same additive style as reputation/quests/achievements.

`CharacterSpellSyncRepository` / `SqlCharacterSpellSyncRepository`:
```sql
INSERT IGNORE INTO character_spell (guid, spell, active, disabled)
SELECT {sourceGuid}, spell, active, disabled
FROM character_spell
WHERE guid = {cloneGuid} AND active = 1
```
Only active spells are propagated. Disabled/suppressed spells are intentionally
excluded because they represent temporary UI state, not real learning.

`CharacterSkillSyncRepository` / `SqlCharacterSkillSyncRepository`:
```sql
INSERT INTO character_skills (guid, skill, value, max)
SELECT {sourceGuid}, skill, value, max
FROM character_skills WHERE guid = {cloneGuid}
ON DUPLICATE KEY UPDATE
  value = GREATEST(value, VALUES(value)),
  max   = GREATEST(max,   VALUES(max))
```
`GREATEST` on both `value` and `max` ensures neither the current level nor the
cap ever shrinks. Skills the source already has converge to the higher level;
skills only the clone earned are inserted fresh.

Both repositories are constructed inside `LivingWorldPlayerScript` at:
- `RecoverAccountAltRuntimesForAccount` (account-login recovery path)
- `RunBotDismissalRecovery` (bot-logout dismissal path)

and passed into `AccountAltDismissalService` alongside the existing spell,
skill, reputation, quest, and achievement repositories.

`AccountAltDismissalSummary` tracks `spellsSynced` and `skillsSynced` booleans.
Both LOG_INFO summary lines now include `spells={}` and `skills={}` fields.

### B) Live propagation — in-session hooks

Two `PlayerScript` hooks already existed in AzerothCore, requiring no core changes:

`OnPlayerLearnSpell(Player*, uint32 spellId)` — fires after the player learns
any spell. Skips bot sessions. Iterates
`BotPlayerRegistry::FindBotsForOwner`, checks `bot->HasSpell(spellId)`, and
calls `bot->learnSpell(spellId, false)` if the bot is missing it.

`OnPlayerLearnTalents(Player*, uint32 talentId, uint32 talentRank, uint32 spellId)` —
fires after the player spends a talent point. Same guard pattern. If `spellId`
is non-zero and the bot does not already have it, calls
`bot->learnSpell(spellId, false)`.

Both hooks log at `LOG_INFO` on propagation and `LOG_DEBUG` on skip with
`[LivingWorldDebug]` prefix.

### C) What is intentionally not synced

- **Talents back to source**: bots have no mechanism to change their own talents;
  the clone starts with the owner's talents and cannot spend talent points.
- **Glyphs**: copied at `PlayerDump` clone-creation time; no in-session glyph
  change mechanism exists on the bot side.

---

## Immediate Next: Multi-Bot Support and Remaining Verification

Happy-path spawn, follow, cast, and dismiss are verified working on a live
server. The remaining gaps are:

- **Multi-bot support (1-to-N)**: `BotPlayerRegistry` is hard-limited to one
  active bot per owner (`_botsByOwner` is a single-value map). Supporting a
  full 4-player party requires changing registry maps to `vector<ObjectGuid>`
  per owner and updating spawn guards and `CompanionAI` event scheduling
  accordingly.
- **Crash/interrupt recovery verification**: **Partial**
  - Live test now confirms forced client close + relog can recover inventory
    state and restore visible character names correctly on the next login.
  - Still worth broadening this into a fuller regression matrix:
    spawn → client kill / world crash → relog → verify progress, equipment,
    inventory, bank, name lease, and party roster cleanup.
- **Bot-session restart validation**: verify that null-socket bot sessions
  survive a worldserver restart scenario without leaving orphaned runtime rows.
- **Combat-state spawn restriction**: no explicit block on requesting a bot
  while the owner is in active combat.

Current implementation status:
- `model::CharacterItemSnapshot` now provides a read-only item-state shape for
  equipment, inventory, bank, and uncategorized items.
- `SqlCharacterItemSnapshotRepository` reads `character_inventory` joined to
  `item_instance` and classifies nested bag contents into inventory-vs-bank
  domains.
- `CharacterItemSnapshotClassifier` has unit coverage for equipment,
  inventory, bank, and nested bag classification.
- `CharacterItemSanityChecker` now validates duplicate item guids,
  uncategorized storage state, equipment slot/container shape, and
  inventory/bank container plausibility, including duplicate nested slots.
- It now surfaces `Equipment`, `Inventory`, and `Bank` as planning domains
  when the snapshots are structurally sane. Inventory/bank comparisons use
  logical container paths so source-vs-clone item GUID differences do not
  create false recovery work.
- `SqlCharacterEquipmentSyncRepository` and
  `AccountAltEquipmentSyncExecutor` now provide the first transactional
  equipment-only write path by duplicating clone equipped `item_instance` rows
  onto the source character with new item guids.
- `AccountAltDismissalService` now runs during bot logout and can:
  - resolve the runtime by clone guid
  - sync safe progress domains back to the source
  - sync equipment when approved
  - restore the source live name through `CharacterNameLeaseRepository`
- `AccountAltItemRecoveryService` now plans `NoAction`,
  `SyncEquipmentToSource`, `SyncBagDomainsToSource`, `Blocked`, or
  `ManualReview` from item-sanity results.
- Bag-domain write seams now exist:
  - `CharacterInventorySyncRepository` +
    `SqlCharacterInventorySyncRepository`
  - `CharacterBankSyncRepository` +
    `SqlCharacterBankSyncRepository`
  - `AccountAltInventorySyncExecutor`
  - `AccountAltBankSyncExecutor`
- Those seams duplicate `item_instance` rows, remap nested container GUIDs, and
  rewrite `character_inventory` rows transactionally, but they are not wired
  into default live recovery yet.
- `AccountAltItemRecoveryService` now has explicit bag-domain policy gating:
  it can plan inventory/bank recovery when enabled, and the current default
  policy now enables those domains. Equipment recovery is evaluated before bag-domain
  blocking so safe gear recovery still proceeds when inventory/bank sync is
  disabled.
- `AccountAltRuntimeCoordinator`, `AccountAltStartupRecoveryService`, and
  `AccountAltDismissalService` now understand bag-domain recovery plans and can
  execute the inventory/bank sync executors when policy allows it. The current
  live default is now on.
- `AccountAltDismissalSummary` now reports per-domain item results so callers can
  distinguish progress sync from equipment, inventory, and bank recovery.
- `AccountAltRuntimeCoordinator` now runs item snapshot loading, item sanity,
  and item recovery after progress recovery succeeds or when the clone is
  otherwise reusable.
- `AccountAltStartupRecoveryService` now distinguishes `SyncingBack`
  (progress retry), `SyncingEquipment`, `SyncingInventory`, and `SyncingBank`
  on owner login.
- `AccountAltStartupRecoverySummary` now tracks recovered progress,
  equipment, inventory, and bank syncs separately in addition to the aggregate
  `recoveredSyncs` count.
- Account-login recovery now also has an earlier account-auth hook path via
  `LivingWorldAccountScript::OnAccountLogin`, which uses
  `AccountAltDismissalService` to restore source names and sync clone state back
  before character enumeration.
- Owner login now schedules deferred stale-party cleanup after world entry so
  disconnected clone members do not keep blocking fresh bot requests.

Important current safety line:
- inventory/bank execution now defaults on for the live path so account alts
  keep persistent item state with their source character
- bag-container-change detection and manual-review escalation are now the main
  safeguards against unsafe automatic bag sync

After this, the next follow-on slice should be:
1. a real config/manual surface for bag-domain policy
2. tighter nested-container/manual-review rules
3. runtime verification of dismiss/logout, name reclaim, and bag safety before
   any default-on behavior is considered

---

### Planner / command / progression work (unchanged priority)

4. **Move planner policies toward config/data as consumers appear**
- `SimpleZonePopulationPlanner` now has the first scoring, cooldown, activity,
  and unlocked-zone filtering pass. The next planner work should avoid piling
  on hardcoded weights; extract policy knobs into config/data once the runtime
  service path needs tuning.

5. **Keep economy/event/progression additions modular**
- The simulated AH, event reaction, and milestone-unlock systems should be
  implemented as separate policy/service tracks rather than folded into
  the first party bot runtime slice.

6. **Harden the runtime command surface**
- Keep `.lwbot list/request/dismiss`, `.lwbot <#|name> cast`, and
  `.lwbot <#|name> profile` stable as the single-bot command surface.
- Keep dismiss aligned with the bot-session logout path so clone recovery,
  name/item sync, and group removal stay authoritative.
- Next expansion: multi-bot support requires extending `.lwbot request` to
  allow queuing multiple bots and updating all registry lookups to 1-to-N.

7. **Expand quest UX beyond reward-choice handling**
- Reuse the existing addon `Quests` tab as the home for broader bot quest
  actions rather than creating a second quest addon.
- Primary next step: target-questgiver driven actions panel.
- Goal: when the owner targets a quest giver, show bot-specific `Pick Up` and
  `Turn In` actions for quests that active bots are eligible for even if the
  owner is not, including class-specific trainer quests and chain
  continuations.
- Do not make proximity-based auto-accept the default behavior. Prefer
  explicit player-triggered actions from the panel to avoid noisy or unsafe
  automation.
- Reward-choice turn-ins from that panel should continue to route through the
  smart/manual reward system rather than a separate implementation.

---

## Coding Standards

- Use spaces with four spaces per indentation level.
- Prefer clear names and small focused classes.
- Keep world mutation out of model/planner layers.
- Keep comments meaningful and sparse.
- Prefer tunable config/data over hardcoded behavioral constants when likely to
  change.
- Preserve modularity over quick one-off hacks.

---

## Summary Direction

The project has moved well past the initial foundation pass. Current state:

- Module scaffold, model/planner foundations, and clean build/test workflow are
  established.
- Full account-alt bot lifecycle is working on a live server: spawn, party join,
  role-based CompanionAI combat, natural-language cast commands, profile
  switching, dismiss with name reclaim and progress/equipment sync.
- Account-alt runtime records, sanity checking, sync executors, and startup
  recovery are all implemented and backed by unit tests.
- The current system supports one active bot per owner (hard registry limit).

The next milestone is **multi-bot support**: extend `BotPlayerRegistry` to
track N bots per owner, update spawn guards, and scale `CompanionAI` scheduling
to match — unlocking the full 5-player party gameplay goal.

---

## Planned Extension Tracks

The following tracks were added after reviewing additional design notes and are
intended to be woven into the mod as future work rather than treated as side
ideas.

### A) Simulated Economy / Auction House

**Overall Status: Not Started**

#### Candidate tasks

A.1 Define listing/source ownership types
- Future economy data should be able to distinguish:
  - player-owned auctions
  - bot-owned auctions
  - system-seeded stock

A.2 Define market reference values and demand classes

A.3 Add market absorption rules for player auctions
- Fairly priced common goods should usually sell after a believable delay.
- Expensive or niche items should be riskier.

A.4 Add seeded stock pools and soft refresh behavior

A.5 Make economy progression- and event-aware

### B) Event-Aware World Reaction

**Overall Status: Not Started**

#### Candidate tasks

B.1 Query or mirror active AzerothCore game-event state

B.2 Add event-based population modifiers
- City crowding
- travel shifts
- themed ambient activity

B.3 Add event-based market modifiers
- themed stock pools
- temporary demand spikes
- event-sensitive supply

B.4 Add event-based rival/world behavior modifiers

### C) Milestone-Driven Progression

**Overall Status: Not Started**

#### Candidate tasks

C.1 Support progression state driven by player accomplishments, not only time

C.2 Define milestone unlock inputs
- boss-kill flags
- special unlock events
- optional AQ-style gated progression

C.3 Make population/economy/service policies react to progression state

C.4 Keep progression logic centralized in dedicated services/config rather than
scattered checks

### D) Bot Control, Combat Profiles, and Addon UX

**Overall Status: Partial**

Primary design/handoff doc for this track:
- `modules/mod-living-world/docs/BotCombatProfiles.md`

#### Candidate tasks

D.1 Define addon-friendly command grammar and stable bot IDs — **Partial**
- [x] Keep bot/profile control server-authoritative through a worldserver API
      layer rather than direct addon-to-database access.
- [x] Treat addon actions as zoomed-out requests: addon -> API/messages ->
      profile service -> repository -> DB.
- [ ] Finalize the concrete request/response surface for profile CRUD,
      row edits, priority changes, reset, and active-slot changes.

D.2 Keep roster, behavior, and combat-profile control surfaces separate — **Partial**
- [x] Separate high-level concerns conceptually: roster selection, party
      controls, and combat profile editing are distinct UI/API surfaces.
- [ ] Reflect that split in the addon panel layout and command/message naming.

D.3 Define `CombatProfile` data model — **Partial**
- class
- role/type
- optional subtype/style
- level band
- loadout profile
- behavior flags
- combat/maintenance/utility rules
- racial rules
- trinket rules

- [x] Decide that a blank player profile means "use baked-in default behavior"
      for the effective spec/role.
- [x] Decide that profile state should be stored in relational DB tables, not a
      JSON blob.
- [x] Decide each source character gets 10 profile slots.
- [x] Decide each profile may carry a spec/role override while still retaining
      a server-side best-guess fallback.
- [ ] Finalize the exact schema for:
  - profile settings table
  - rotation entry table
  - action table (primary/secondary)
  - condition table
  - baked-in default profile tables
  - runtime active-profile slot linkage

D.4 Define structured rule/action/condition schema — **Partial**
- [x] Agree on row-based authoring model: add row -> action/item/spell,
      optional target, condition subject/stat/operator/value.
- [x] Agree on primary/secondary action slots per row.
- [x] Agree on interrupt/global override rules that can break an active cast.
- [x] Agree on primary fallback timing rule:
  - wait if primary becomes usable within the next 500ms tick
  - otherwise attempt secondary
  - otherwise skip row
- [x] Add target concepts `enemy_primary` and `enemy_trash` to the design.
- [ ] Finalize the first-pass condition/operator vocabulary and DB-backed enum
      mapping.
- [ ] Finalize target-resolution rules for all supported target types.

D.5 Define level-band strategy — **Not Started**
- Support 5 to 10 level chunks.
- Prefer 10-level bands first.

D.6 Define loadout-aware combat doctrine — **Not Started**

D.7 Define player override model — **Partial**
- default profile
- bot-specific override
- optional session override
- reset path

- [x] Decide that user override is optional and should sit on top of server
      defaults.
- [x] Decide that "best guess" spec/role is the fallback when no override is
      present.
- [ ] Decide whether v1 uses full custom-profile replacement or partial merge
      against baked-in defaults.
- [ ] Define reset-to-default behavior precisely at the row/settings level.

D.8 Define addon MVP surfaces — **Partial**
- roster UI
- party control UI
- bot detail UI
- combat editor UI

- [x] Agree that the combat editor should support row authoring with simple
      controls such as spell/item, target, and `IF` conditions.
- [ ] Design the first-pass combat profile editor layout, including spec/role
      dropdowns and profile slot selection.
- [ ] Define how the addon queries and refreshes server-stored profile data.

D.9 Use external guide resources for combat doctrine only — **Not Started**
- role identity
- rotation philosophy
- cooldown concepts
- buff/debuff priorities
- racial/trinket usage concepts
- loadout assumptions

D.10 Keep local runtime data authoritative for executable values — **Partial**
- [x] Agree that executable truth stays on the server.
- [x] Agree that the addon should send intent/edits, while the server resolves
      spells, targets, cooldown timing, role guesses, and default behavior.
- [ ] Define which runtime-derived values can be exposed back to the addon for
      preview/debug without making the addon authoritative.

D.11 Define doctrine-to-profile authoring workflow — **Partial**
- read guide concepts
- convert to structured internal profile data
- validate against local runtime truth
- execute only valid actions

- [x] Decide v1 starter coverage is one baked-in default spec/role per class.
- [ ] Choose the exact 10 initial spec/role defaults to seed.
- [ ] Author and validate those defaults against live class spell/rank data.

#### Current active checklist

- [ ] Add DB schema for bot combat profiles, entries, actions, and conditions.
- [ ] Add baked-in default data/service path for one starter spec/role per
      class.
- [ ] Implement spec/role best-guess resolver with optional profile override.
- [ ] Add runtime resolver for blank-profile -> default-profile behavior.
- [ ] Add primary/secondary row execution with 500ms wait-or-fallback logic.
- [ ] Implement target resolvers for `enemy_primary` and `enemy_trash`.
- [ ] Add conservation modes: Full Force, Conservative, JIT Casting.
- [ ] Add optional down-rank support with floor rules.
- [ ] Expose profile CRUD/edit/reset/apply operations through the server API /
      message layer for addon consumption.
- [ ] Build the first addon-side combat profile editor against that API.
