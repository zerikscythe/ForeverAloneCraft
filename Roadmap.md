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

6.1 Define an `AzerothWorldFacade` or equivalent thin adapter — **Partial**
- Pure-virtual `integration::AzerothWorldFacade` now exists covering initial
  read queries: player context, spawn anchors in zone, character-online
  check.
- Pure-virtual `integration::RosterRepository` covers persistent-roster
  lookup (list-by-account, find-by-id-scoped-to-account). Scoping every
  query by account is the cross-account safety guarantee.
- Real AzerothCore-backed implementations are still deferred; test-only
  fakes for both are in place and drive service tests.

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
  may produce `WorldCommitAction` values, and nothing executes them yet.
- A single authoritative commit layer still needs to be written before the
  first mutation lands.

---

## Phase 5: Orchestration and Runtime Services

**Overall Status: Partial**

### 7) High-Level Services

**Overall Status: Partial**

#### Subtasks

7.1 Introduce `LivingWorldManager` — **Partial**
- Minimal `service::LivingWorldManager` exists. It owns a reference to the
  facade, a `SimplePartyRosterPlanner`, and the first `PartyBotService`.
- Update scheduling and multi-subsystem coordination are not implemented
  yet.

7.2 Introduce `WorldPopulationService` — **Not Started**
- Player-local ambient population orchestration.

7.3 Introduce `PartyBotService` — **Partial**
- `service::PartyBotService` is implemented. It resolves the player
  context via the facade, looks up the requested entry through the
  `RosterRepository` scoped to the requesting account, enforces the
  "alt already online" rule, delegates to the party roster planner, and
  translates an approved plan into explicit `WorldCommitAction` records.
- World-facing commit execution is intentionally not implemented.
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

8.1 Define player-facing roster flow — **Partial**
- `service::PartyBotService::DispatchRosterRequest` is the first supported
  end-to-end path: request in, structured result + commit actions out.
- A first in-game command UX exists through `.lwbot roster ...`; it is
  intentionally read/intent-only until an authoritative commit executor lands.

8.2 Implement first command surface for controllable bots — **Partial**
- Backend grammar parser exists: `script::ParseLivingWorldCommand`
  recognises `.lwbot roster list`, `.lwbot roster request <id>`, and
  `.lwbot roster dismiss <id>`, producing a structured `ParsedCommand`
  result consumable by both a chat command script and a future addon
  message channel.
- `script::LivingWorldCommandScript` registers `.lwbot` with AzerothCore,
  lists account-alt roster entries from the characters DB, scopes lookup to
  the requesting account, routes requests through `PartyBotService`, and
  renders approved / rejected planner output plus commit-action intent.
- The command does not execute world mutation yet.
- Switch control/possession target is still not implemented.

8.3 Add party slot/rule validation — **Not Started**
- Enforce:
  - party size limits
  - duplicate roster restrictions
  - ownership rules
  - combat-state restrictions where needed

8.4 Add follow/assist/control mode definitions — **Not Started**

8.5 Define possession rules separate from spawn rules — **Not Started**
- A bot being active in the world is not the same as the player actively
  controlling it.

### 9) Account Alt Support

**Overall Status: Partial**

#### Subtasks

9.1 Define owned-alt eligibility rules — **Not Started**
- Only alts from the player's account should be eligible.

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
  current source snapshot. Item, reputation, quest, and mail sync rules remain
  unimplemented.
- Sync-domain types now distinguish XP, money, inventory, equipment,
  reputation, quests, and mail. Only XP/money-level style progress should be
  treated as first-pass syncable; inventory-like domains remain explicitly
  gated behind future sanity rules.

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
- Remaining work is broader clone lifecycle persistence and additional sync
  domains beyond progress-only recovery.

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

## Immediate Next Implementation Slice

The null-socket bot-session slice has landed. The next milestone is durable
account-alt recovery so crashes do not strand progress on bot-owned clone
characters or overwrite good source-character data.

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

### E) Startup/player-login recovery pass

On player login, list recoverable runtime records for the account. For each
record, either reuse the persistent clone, queue a safe sync plan, or mark it
for manual review. This gives the new session a deterministic answer to "what
bots/accounts were last used?"

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
thread (thread-safe). Each tick (~500 ms):
- If owner is in combat and bot has no target: `bot->Attack(owner->GetVictim())`
- If owner leaves combat: `bot->AttackStop()` + re-issue `MoveFollow`
- If not in combat and bot is too far: `MoveFollow(owner, 2.0f, M_PI)`

Class-specific spell behavior is the next slice after this one lands.

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

### B) OnPlayerLogout — owner dismiss + bot cleanup

`LogoutPlayer` in core skips automatic group removal when the session has no
socket (the `m_Socket &&` guard around the group-removal block). Bot sessions
have no socket, so they were silently left in groups forever.

`OnPlayerLogout` currently has two responsibilities:
- owner logout kicks the active bot so it does not remain in-world with a null
  owner
- bot logout explicitly removes the bot from the group before unregistering it

The bot branch explicitly calls:
```cpp
if (Group* group = player->GetGroup())
    group->RemoveMember(player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
```
This fires before `UnregisterBotPlayer` and the bot-pool DB release so the
group is clean regardless of whether the bot was kicked or timed out.

### C) `.lwbot roster dismiss <id>` — real implementation

`RenderDismissBot` replaced the old `RenderDismissPlaceholder`. It:
1. Looks up the owner's active bot via `BotPlayerRegistry::FindBotForOwner`.
2. Removes the bot from its group with `GROUP_REMOVEMETHOD_LEAVE`.
3. Calls `bot->GetSession()->KickPlayer("LivingWorld roster dismiss")` — this
   sets `IsKicked()=true`, which causes the null-socket guard in
   `WorldSession::Update` to return false on the next tick, triggering
   `LogoutPlayer` and the full cleanup chain.

---

## Completed Slice: Account-Alt Sync Executor

See section D of "Immediate Next Implementation Slice" above for full detail.

---

## Completed Slice: Bag-Domain Policy Surface and Container Tightening

### A) Config-driven bag-domain policy — **Complete**

`mod-living-world.conf.dist` now exposes two operator-controlled knobs:
- `LivingWorld.AccountAlt.EnableInventorySync = 0`
- `LivingWorld.AccountAlt.EnableBankSync = 0`

Both default to disabled. All three service construction sites that accept
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

### C) Bag-container-change detection — **Complete**

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
- `RejectsExcessiveContainerSize` — 37 items inside one bag fails
- `SetsBagContainersChangedWhenInventoryBagsDiffer` — different bag type at slot 19
- `SetsBagContainersChangedWhenBankBagsDiffer` — different bag type at slot 67
- `DoesNotSetBagContainersChangedWhenOnlyContentsDiffer` — same bag type, different contents
- `RequiresManualReviewWhenBagContainersChangedEvenIfPolicyEnabled` — ManualReview overrides policy

---

## Immediate Next: Runtime Verification and End-to-End Testing Prep

The config surface and tighter sanity rules are now in place. The remaining
work before any bag-domain sync can be considered for default-on is:

- **Runtime verification pass**: exercise the full spawn → dismiss → startup
  recovery cycle on a live server and confirm that name reclaim, progress sync,
  and equipment sync leave source characters in correct state
- **Bot-session session-key validation**: verify that null-socket bot sessions
  survive a worldserver restart scenario correctly
- **Manual test checklist**: document and execute the dismiss/logout
  lifecycle, source-name reclaim, and clone-to-source sync for at least one
  account alt before enabling inventory/bank sync in any live config

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
  inventory/bank container plausibility.
- It now surfaces `Equipment`, `Inventory`, and `Bank` as planning domains
  when the snapshots are structurally sane.
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
  it can plan inventory/bank recovery when enabled, but the default policy keeps
  those domains blocked.
- `AccountAltRuntimeCoordinator`, `AccountAltStartupRecoveryService`, and
  `AccountAltDismissalService` now understand bag-domain recovery plans and can
  execute the inventory/bank sync executors when policy allows it, while still
  staying default-off on the live path.
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

Important current safety line:
- inventory/bank execution exists in code, but the default policy still blocks
  those domains on the live path
- another agent should not silently enable bag-domain sync by default without
  a deliberate config/manual-control surface and runtime testing

After this, the next follow-on slice should be:
1. a real config/manual surface for bag-domain policy
2. tighter nested-container/manual-review rules
3. runtime verification of dismiss/logout, name reclaim, and bag safety before
   any default-on behavior is considered

---

## Completed Slice: Account-Alt Sync Executor

See section C of "Immediate Next Implementation Slice" above for full detail.

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
- Keep `.lwbot roster list` and `.lwbot roster request <id>` usable in-game.
- Leave `.lwbot roster dismiss <id>` as a placeholder until despawn commit
  actions exist.

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

The current project has completed the first foundation pass:

- module scaffold exists
- model/planner foundations exist
- separation of concerns is proven with testable planner stubs
- Windows configure/build/test workflow is healthy

The next milestone is to move from compile-ready architecture into the first
real runtime-facing service boundary, with command-driven party bot planning as
the narrowest useful feature slice.

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

**Overall Status: Not Started**

#### Candidate tasks

D.1 Define addon-friendly command grammar and stable bot IDs

D.2 Keep roster, behavior, and combat-profile control surfaces separate

D.3 Define `CombatProfile` data model
- class
- role/type
- optional subtype/style
- level band
- loadout profile
- behavior flags
- combat/maintenance/utility rules
- racial rules
- trinket rules

D.4 Define structured rule/action/condition schema

D.5 Define level-band strategy
- Support 5 to 10 level chunks.
- Prefer 10-level bands first.

D.6 Define loadout-aware combat doctrine

D.7 Define player override model
- default profile
- bot-specific override
- optional session override
- reset path

D.8 Define addon MVP surfaces
- roster UI
- party control UI
- bot detail UI
- combat editor UI

D.9 Use external guide resources for combat doctrine only
- role identity
- rotation philosophy
- cooldown concepts
- buff/debuff priorities
- racial/trinket usage concepts
- loadout assumptions

D.10 Keep local runtime data authoritative for executable values

D.11 Define doctrine-to-profile authoring workflow
- read guide concepts
- convert to structured internal profile data
- validate against local runtime truth
- execute only valid actions
