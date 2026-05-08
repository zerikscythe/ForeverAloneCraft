# Plan: World Bot Spawning — Bot Taxonomy and Build Phases

---

## Core Design Principle

Every bot in this system is a real WoW Player session running on a pool account
(`living_world_bot_account_pool`). They log in, exist in the world as Player
objects, and use the same CompanionAI / AmbientBotAI / GuildBotAI that drives
the clone system. The taxonomy below is about **ownership, persistence, and
command namespace** — not about different spawn technology.

### Intent clarification: two tiers, one spawn technology

Future agents must not confuse **account-owned** with **account-backed**.

There are **two gameplay tiers** in this project:

#### Tier 1 — Account Bots
These are the player's own characters, materialized as persistent clones on
BOTHOUSE/pool accounts.

Key traits:
- tied to a real player account and source character
- spawned by the custom roster/account-alt command flow
- persistent identity over time
- clone progress syncs back to the original source character
- this is the "your alt, but active as a bot right now" system

#### Tier 2 — Non-Account Bots
These are all other bots in the world: ambient bots, hostile faction bots, and
specialized queue/instance bots.

Key traits:
- **not** tied to a player's personal alt roster
- **not** part of the clone-sync-back-to-source pipeline
- authored from DB/config templates rather than from an owned source character
- assigned a session/task plan (travel, gather, patrol, fight, quest, etc.)
- meant to **pretend to be players** in the world

#### Intent clarification: Tier 2 bots are task-chain/session bots

The intended behavior model for non-account bots is **not** "pick exactly one
activity and idle there forever."

Instead, each Tier 2 bot should be assembled from:
- a bot template or generated identity
- faction / class / role / level-range data
- default combat doctrine/profile selection
- a **random chained task list** for the session

Initial design target:
- each bot session should receive roughly **3 to 10 tasks**
- tasks may be completed in sequence as a believable "day in the life"
- travel is part of the chain, not an afterthought

Example chained session:
1. Go quest in a level-appropriate zone
2. Travel there by walking or taxi
3. Visit auction house
4. Visit mailbox
5. Travel to a mining zone
6. Gather ore
7. Travel to another hub
8. Idle, patrol, or look for faction conflict

So the planner should think in terms of:
- **session composition**
- **task chaining**
- **travel between tasks**
- **bot lifetime/session expiration**
- not just single isolated destination picks

#### Intent clarification: task types should be DB-authored

The long-term plan is to keep a database/library entry for many different task
types and let the session planner assemble a believable chain from them.

Examples of task families:
- questing in a level-appropriate zone
- city errands
  - auction house
  - mailbox
  - vendor
  - inn
  - trainer
- gathering
  - ore
  - herbs
  - fishing
- travel
  - walk
  - taxi
- patrol / roam
- faction conflict / ambush / hunt-weaker-enemy behavior
- dungeon / battleground / raid queue behavior for specialized subsets later

The "thing" inside a task, such as:
- a level-appropriate questing zone
- a mining zone
- a city hub
- a contested PvP zone

should come from lookup tables rather than hardcoded one-off choices.

#### Intent clarification: zone lookup should be level-band aware

A core planner rule is that zone/task selection should use lookup tables that
support level-band matching.

Initial expectation:
- choose zones that are appropriate for the bot's level
- allow a tolerance of about **±5 levels** around the bot where that makes
  sense
- use faction, continent, travel feasibility, and zone type as additional
  filters later

This supports believable variety:
- low-level bots stay in starter/early zones
- mid-level bots circulate through classic leveling regions
- high-level bots can enter contested or expansion zones
- specialized hostile bots can prefer dangerous cross-faction regions

#### Intent clarification: scale target

The long-term world-population target is much larger than the first validation
slice.

Early local validation can start small, but the intended live direction is:
- a broad mix of level ranges
- potentially **300–500 active bots per faction**
- enough variety that the world feels inhabited rather than scripted

That scale goal is why task composition, zone lookup, and cheap session logic
must stay data-driven.

#### Intent clarification: faction conflict tasks are valid session content

Some Tier 2 bots should be able to receive more dangerous or aggressive tasks.

Example:
- a level 50+ rogue might get a task like:
  - go to Stranglethorn
  - hunt weaker opposite-faction targets
  - create tension/spice in the zone

These should still be planner-authored tasks, not special one-off hacks.
They are part of making the world feel active, unpredictable, and occasionally
hostile.

Important terminology rule:
- **"non-account bot" does _not_ mean "no AzerothCore account/session exists"**
- it means **"not owned by a player's alt/account-bot sync model"**

In other words:
- Tier 1 and Tier 2 are different in **ownership and persistence rules**
- but they may still share the same underlying Player-session spawn machinery

This distinction exists to prevent a common misunderstanding:
- Tier 1 bots are persistent player-owned clone bots
- Tier 2 bots are world/system bots driven by DB-authored templates and task
  sessions
- both may still be spawned through the same lower-level factory/session path

### Intent clarification: BotSessionFactory is infrastructure, not taxonomy

`BotSessionFactory` is the shared low-level spawn/session mechanism.

It should not be treated as proof that every gameplay bot category has the same
ownership model.

Examples:
- Tier 1 account-alt clones may use `BotSessionFactory`
- Tier 2 ambient/hostile/instance bots may also use `BotSessionFactory`

That shared infrastructure does **not** collapse the design into one tier.
What differs is:
- where the bot identity comes from
- whether it syncs to a source character
- whether it is persistent or session-authored
- how its task list/session is chosen
- what command namespace or autonomous system owns it

---

## Bot Taxonomy

There are five distinct bot categories. They differ in who owns them, how
players interact with them, and whether they have real economic persistence.
**Never mix categories in the same command namespace.**

---

### Category 1 — Account Alt Clones (EXISTING — `.lwbot roster`)

**What they are:** The player's own account characters, cloned to bot house
accounts as persistent avatars/aliases. When you roster-request your mage
"Fireball", a clone of Fireball logs in on a pool account and follows you.
The clone's progression syncs back to the source character.

**Key rule: `.lwbot roster list` shows ONLY the requesting player's own account
characters. Pre-built, guild, and world bots NEVER appear here.**

Commands:
- `.lwbot roster list` — your account characters only
- `.lwbot roster request <id|name>` — spawn the clone companion
- `.lwbot roster dismiss <id|name>` — log the clone out

DB backing: `living_world_account_alt_runtime` (existing).

Status: **Already works.** No changes needed in this category.

---

### Category 2 — Raid Pool Bots (NEW — `.lwbot raid`)

**What they are:** Server-side pre-generated characters. No player-account
ownership or clone-sync relationship. A GM/admin creates these characters once
(leveled, geared) and registers them in the pool. When a player needs a warm
body for a raid, they call one by role and gear level. The bot joins the group,
uses CompanionAI and doctrine, and fights alongside the player.

**No progression sync. No guild affiliation. Pure companion utility.**

Commands:
- `.lwbot raid request <class> <spec> <level> <min_ilvl>` — pulls a matching
  bot from the pool, spawns it at the player, adds it to the raid group.
- `.lwbot raid dismiss <name|#>` — logs the bot out and returns it to the pool.

Pool selection query (example — Horde warrior tank, level 70+, ilvl 100+):
```sql
SELECT bot_account_id, character_guid
FROM living_world_pool_character
WHERE class_id = 1 AND spec_role = 'tank'
  AND level >= 70 AND avg_ilvl >= 100
  AND faction = 2 AND is_available = 1
ORDER BY RAND() LIMIT 1;
```

DB schema needed:
```sql
CREATE TABLE IF NOT EXISTS living_world_pool_character (
    id              INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_account_id  INT UNSIGNED NOT NULL,
    character_guid  BIGINT UNSIGNED NOT NULL,
    class_id        TINYINT UNSIGNED NOT NULL,
    spec_role       VARCHAR(16) NOT NULL,   -- 'tank','healer','dps','support'
    spec_key        VARCHAR(32) NOT NULL,   -- e.g. 'warrior_prot', 'mage_fire'
    level           TINYINT UNSIGNED NOT NULL,
    avg_ilvl        SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    faction         TINYINT UNSIGNED NOT NULL,  -- 1=Alliance, 2=Horde
    is_available    TINYINT UNSIGNED NOT NULL DEFAULT 1,
    UNIQUE KEY uq_char (character_guid),
    KEY idx_role (class_id, spec_role, level, avg_ilvl, faction, is_available)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Spawn path: `BotSessionFactory::SpawnBotPlayerOnAccount(botAccountId, charGuid,
ownerGuid)`. On login, `LivingWorldPlayerScript::OnPlayerLogin` detects the
pool-character flag (no clone runtime record, bot_type = RaidPool) and schedules
CompanionAI, joins the player's raid group.

---

### Category 3 — Guild Bots (NEW — `.lwbot guild`)

**What they are:** Pre-generated characters that are registered members of the
player's in-game guild. They are part of the living world — they log on
autonomously, travel to appropriate zones, and perform activities. The critical
distinction: **when a guild bot gathers resources, those items actually exist.**
The bot adds real items to its inventory; nodes are removed from the world as
if a real player gathered them. At the end of an activity session the bot
deposits gathered items and earned gold to the guild bank. This creates real
economic value for the guild.

Players can also call guild bots to their location on demand.

**Guild bots are persistent characters.** They are not drawn from a generic
pool — each one is a named, ongoing guild member with its own history.

#### Intent clarification: guild bots are a persistent Tier 2 subtype

Guild bots are an important special case inside Tier 2.

Unlike more disposable session-authored ambient bots, many guild bots should be
treated as **permanent flavor characters**:
- same name
- same look
- same guild membership
- same broad identity over time

These are still Tier 2 non-account bots:
- not tied to a player's account-alt sync pipeline
- not part of the roster/account clone system

But they are meant to feel like recurring guild members rather than temporary
faceless world population.

#### Intent clarification: guild bots as "task rabbits"

A primary use case for permanent guild bots is as controllable **task rabbits**.

The intended gameplay loop is:
- the player has guild bots registered to the guild
- a custom in-world object, likely a **pedestal near the guild bank**, is used
  to assign or force tasks
- the selected guild bot is told to perform a task such as:
  - gather ore
  - gather herbs
  - complete a delivery/run
  - return and deposit into the guild bank

This means guild bots need two operating modes:
- **autonomous mode** — they choose/receive normal activity sessions
- **forced-task mode** — the player explicitly assigns a task through the guild
  control object or command surface

Forced-task mode should override normal wandering behavior until the task is
complete, cancelled, or times out.

#### Intent clarification: guild bots may need real inventory/economy state

Guild bots are the Tier 2 subtype most likely to need real gameplay state.

Expected direction:
- they may carry real items in bags
- they may gather real resources from the world
- they may deposit real items and/or gold into the guild bank

If full player-like inventory is too heavy in an early slice, some parts may be
faked temporarily, but the intended long-term direction is that guild task
rabbits should be able to behave like real player gatherers with meaningful
resource output.

#### Guild Bot Autonomy Flow

1. Server `OnUpdate` tick wakes a guild bot that has been offline long enough.
2. `BotActivitySessionComposer` selects an activity eligible for this bot's
   class, level, and professions.
3. Session is always `[TRAVEL → destination, ACTIVITY, DEPOSIT → guild bank]`.
   The bot never works in its spawn zone by coincidence — it travels first.
4. **During GATHER steps:** bot interacts with nodes normally. Items are added
   to the bot's inventory via the standard loot path. Nodes are removed from
   the world exactly as a real player would remove them.
5. **DEPOSIT step:** when inventory is full OR the session ends, the bot travels
   to the nearest guild bank NPC. All gathered profession items and earned gold
   above a configured floor are deposited. A log row is written.
6. Bot logs out. `is_available` flips back. The deposit log accumulates history.

#### Player Commands

- `.lwbot guild list` — show guild bots registered to this guild, their current
  activity and location (or "offline").
- `.lwbot guild request <name|#>` — call a guild bot to the player's location.
  Bot suspends its current autonomous session, travels to the player, and runs
  CompanionAI until dismissed.
- `.lwbot guild dismiss <name|#>` — release the bot back to autonomous mode.
- `.lwbot guild deposit` — force an immediate deposit of whatever the bot
  currently carries to the guild bank.

Longer-term control surface:
- a custom guild-side in-world object, likely a **pedestal near the guild
  bank**, should allow forced assignment of rabbit/work tasks to permanent guild
  bots without needing to rely only on chat commands.

#### DB Schema

```sql
-- One row per guild bot character. Each is a persistent named guild member.
CREATE TABLE IF NOT EXISTS living_world_guild_bot (
    id                  INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_account_id      INT UNSIGNED NOT NULL,
    character_guid      BIGINT UNSIGNED NOT NULL,
    guild_id            INT UNSIGNED NOT NULL,
    display_name        VARCHAR(24) NOT NULL,
    class_id            TINYINT UNSIGNED NOT NULL,
    level               TINYINT UNSIGNED NOT NULL,
    faction             TINYINT UNSIGNED NOT NULL,
    profession_1        VARCHAR(32) NULL,   -- e.g. 'herbalism','mining','skinning'
    profession_2        VARCHAR(32) NULL,
    is_available        TINYINT UNSIGNED NOT NULL DEFAULT 1,
    current_activity    VARCHAR(64) NULL,
    last_seen_zone_id   INT UNSIGNED NULL,
    last_deposit_at     DATETIME NULL,
    UNIQUE KEY uq_char (character_guid),
    KEY idx_guild (guild_id, is_available)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Audit trail of every guild bank deposit.
CREATE TABLE IF NOT EXISTS living_world_guild_bot_deposit_log (
    id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    guild_bot_id    INT UNSIGNED NOT NULL,
    guild_id        INT UNSIGNED NOT NULL,
    deposited_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    item_entry      INT UNSIGNED NULL,     -- NULL for gold deposits
    item_count      INT UNSIGNED NULL,
    gold_copper     INT UNSIGNED NULL,     -- NULL for item deposits
    zone_id         INT UNSIGNED NOT NULL,
    KEY idx_bot (guild_bot_id),
    KEY idx_guild (guild_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Spawn path: `SpawnBotPlayerOnAccount`. On login, `OnPlayerLogin` detects the
guild bot flag (has a row in `living_world_guild_bot`), schedules `GuildBotAI`
instead of CompanionAI.

---

### Category 4 — Ambient World Bots (NEW — server auto-spawn, no commands)

**What they are:** Background population. Bots that walk through cities, travel
roads, sit in inns, and stand at the AH. They make the server feel alive to a
new player logging in. They are **visually real but economically inert** — they
simulate gathering (remove nodes) but items are discarded; they do not deposit
anything anywhere.

These are Tier 2 world/system bots:
- not owned by a player
- not source-clone bots
- session/task driven rather than alt-sync driven

No player command spawns or controls these. The server manages them entirely.

Autonomous loop (same structure as guild bots, minus the deposit step):
1. `LivingWorldWorldScript::OnUpdate` wakes ambient bots on a schedule.
2. `BotActivitySessionComposer` picks an eligible activity.
3. `[TRAVEL → destination, ACTIVITY]` — always travel-first.
4. Gather steps interact with nodes and discard items (no inventory fill).
5. Bot logs out after the session.

Intended expansion of this loop:
- ambient/world bots should eventually receive **multi-step chained sessions**
  instead of only a single destination/activity pair
- a session should be assembled from a task library and may include:
  - questing
  - city errands
  - mailbox/AH visits
  - gathering
  - patrol
  - faction-tension tasks
- travel legs should explicitly connect the tasks
- session length should feel like a believable wandering player routine rather
  than a single scripted stop

---

### Category 5 — Hostile / Rival Bots (NEW — server auto-spawn or event trigger)

**What they are:** Rival faction bots present in contested zones or triggered
during city raid events. When attacked, they fight back using CompanionAI in
hostileless mode (no owner → pick attacker as primary target). They do not join
any player's party.

These are also Tier 2 world/system bots:
- faction/world-presence driven
- not tied to a player's account-alt lifecycle
- may be spawned from DB-authored templates and encounter/session rules

No player command controls these. Spawned by population events or city raid
triggers.

---

## Command Namespace Summary

| Command prefix | Bot category | Who can call it |
|---|---|---|
| `.lwbot roster …` | Account alt clones only | Owner of the account |
| `.lwbot raid request …` | Raid pool bots | Any player |
| `.lwbot guild …` | Guild bots for this guild | Guild member |
| *(none — server auto)* | Ambient world bots | No player command |
| *(none — server auto)* | Hostile rival bots | No player command |

**The roster namespace must never query `living_world_pool_character`,
`living_world_guild_bot`, or any non-player-account table.**

---

## Shared Infrastructure

All five categories share:
- `BotSessionFactory::SpawnBotPlayerOnAccount` — the single spawn entry point
- `living_world_bot_account_pool` — pool account reservation / release
- `LivingWorldPlayerScript::OnPlayerLogin` — detects bot type from DB and
  schedules the correct AI event
- `BotPlayerRegistry` — tracks active bot sessions
- `BotActivitySessionComposer` — destination-first activity sequencing
  (Phases 3/4; Categories 4 and 3 use it; Categories 1 and 2 do not)

**Destination-first rule (applies to Categories 3 and 4):**
The session composer always resolves the destination from zone index criteria,
never from spawn proximity. Step list is always `[TRAVEL → zone, ACTIVITY, ...]`.
A bot never works in its spawn zone by accident.

---

## What Already Works

- AccountAlt clone system (Category 1) — complete
- `BotSessionFactory::SpawnBotPlayerOnAccount` — general purpose
- `BotSessionFactory::SpawnHostileBotPlayerOnAccount` — ownerless spawn (shared by hostile and ambient bots)
- `CompanionAI` — doctrine loop, follow, hazard sensor, OOC buffs; null-hardened for ownerless hostile bots
- `ScheduleHostileCompanionAI` — fight-back AI with no owner or group
- Pool account infrastructure
- Doctrine profiles for all specs
- Raid pool bot commands (`.lwbot raid request / dismiss`) — Category 2
- Ambient world population tick + `AmbientBotAI` chained sessions — Category 4
- `BotActivitySessionComposer` — destination-first multi-task session builder
- `SqlZoneIndexRepository` / `SqlActivityLibraryRepository` — DB-backed zone and activity lookup
- `living_world_zone_index` + `living_world_activity_library` schemas and seed data

---

## Build Phases

### Phase 1 — Raid Pool Bots (`.lwbot raid`)
**Effort: medium (~60 lines + DB)**

**Status: ✅ COMPLETE**

1. Create `living_world_pool_character` schema.
2. Add `RaidPoolBot` bot-type flag (account tag or separate table lookup).
3. New command handler `HandleRaidRequest` in `LivingWorldCommandScript.cpp`:
   - Query pool by class/spec/level/ilvl/faction/is_available.
   - Mark `is_available = 0`, call `SpawnBotPlayerOnAccount`.
   - On `OnPlayerLogin`: detect `RaidPoolBot`, schedule CompanionAI, join raid.
4. `HandleRaidDismiss`: log bot out, restore `is_available = 1`.
5. DB: seed at least 2-3 pre-built pool characters per common class/role for testing.

**Verification:** `.lwbot raid request warrior tank 70 100` → a warrior bot
appears at the player's location, joins the raid group, uses doctrine. Dismiss
logs it out and `is_available` returns to 1.

### Implementation notes

- `RaidRequestCommand` and `RaidDismissCommand` structs added to
  `LivingWorldCommandGrammar.h` and the `ParsedCommand` variant.
- `ParseRaidVerb` added to `LivingWorldCommandGrammar.cpp` with a
  `ClassNameToId` helper. Dispatched from `ParseLivingWorldCommand` under the
  `"raid"` subsystem token.
- `HandleRaidRequest` / `HandleRaidDismiss` added to
  `LivingWorldCommandScript.cpp`. Both reserve/release `living_world_pool_character`
  and `living_world_bot_account_pool` with `DirectExecute` before queuing the
  session; rollback on failure.
- `OnPlayerLogout` in `LivingWorldPlayerScript.cpp` now resets
  `living_world_pool_character.is_available = 1` unconditionally — this is a
  no-op (0 rows) for AccountAlt clone bots.
- No changes to the roster pipeline. `.lwbot roster list` continues to show only
  the player's own account characters.
- `living_world_pool_character.sql` schema created in
  `data/sql/characters/`. Table applied to `acore_characters` via migration
  script `tools/lw-editor/db_migrate_phase1_corrected.py`.

### Files changed

| File | Change |
|------|--------|
| `script/LivingWorldCommandGrammar.h` | `RaidRequestCommand`, `RaidDismissCommand` structs + added to variant |
| `script/LivingWorldCommandGrammar.cpp` | `ClassNameToId`, `ParseRaidVerb`, `"raid"` dispatch |
| `script/LivingWorldCommandScript.cpp` | `HandleRaidRequest`, `HandleRaidDismiss`, dispatch in main handler |
| `script/LivingWorldPlayerScript.cpp` | `OnPlayerLogout` resets pool character slot |
| `data/sql/characters/living_world_pool_character.sql` | Schema + setup notes |

**Verification:** `.lwbot raid request warrior tank 70 100` → a warrior bot
appears at the player's location, joins the raid group, uses doctrine. Dismiss
logs it out and `is_available` returns to 1.

### Phase 2 — Hostile / Rival Bots
**Effort: medium (~50 lines)**

**Status: ✅ COMPLETE**

1. Add hostile spawn mode to `BotSessionFactory` — no owner registration.
2. `CompanionAI` hostileless mode: `owner == nullptr` → use `GetVictim()` or
   nearest attacker as `context.primaryTarget`.
3. Guard `AddBotToOwnerGroup` in `OnPlayerLogin` behind owner-present check.
4. Population event trigger: server spawns hostile bots in contested zones or
   on a city raid trigger. No player command needed.
5. DB: hostile bot characters on opposite faction.

### Implementation notes

**`BotSessionFactory`** — new `SpawnHostileBotPlayerOnAccount(botAccountId, characterGuid)`:
- No `ownerCharacterGuid` parameter.
- Registers with `ObjectGuid::Empty` as the owner sentinel in `BotPlayerRegistry`.
- Does not pre-seed the bot's DB position (hostile bots spawn where their
  character record places them).

**`ai/CompanionAI.cpp`** — four null-hardening changes + new path:
- `LoadCombatDoctrine`: `ownerAccountId = 0` when `owner == nullptr`.
- `GetPreparedCombatProfile`: same guard.
- `TryExecuteProfileRotation`: removed `!owner` from the early-return guard.
  All downstream uses of `context.owner` in the evaluator already handle null
  (`PushThreatAddonMessage`, `FindLowestHealthPartyTarget`, `FindTrashTarget`
  are all null-safe).
- `TickHostile`: new function — loads doctrine with `owner=nullptr`, picks
  target from `GetVictim()` then `getAttackers()`, calls
  `TryExecuteProfileRotation(bot, nullptr, target)`. No follow, no OOC buffs.
- `CompanionAIEvent::Execute`: ownerless branch checks `_ownerGuid.IsEmpty()`
  before the existing null-check. Handles the in-world retry loop for the bot
  alone, then calls `TickHostile` and re-schedules at 500ms.
- `ScheduleHostileCompanionAI(Player*)`: schedules event with
  `ObjectGuid::Empty` as ownerGuid.

**`script/LivingWorldPlayerScript.cpp` — `OnPlayerLogin`**:
- After `RegisterBotPlayer`, checks `ownerGuid->GetCounter() == 0` (the
  hostile sentinel).
- If hostile: logs, calls `ScheduleHostileCompanionAI`, returns — no spell
  copy, no group join.
- Existing companion path unchanged.

**Verification:** Hostile bot spawned in a city → player attacks it → bot
fights back with class spells → bot is not in player's party UI.

### Files changed

| File | Change |
|------|--------|
| `integration/BotSessionFactory.h` | Added `SpawnHostileBotPlayerOnAccount` declaration |
| `integration/BotSessionFactory.cpp` | Implemented `SpawnHostileBotPlayerOnAccount` |
| `ai/CompanionAI.h` | Added `ScheduleHostileCompanionAI` declaration |
| `ai/CompanionAI.cpp` | Null-hardened doctrine/profile/rotation; added `TickHostile`; ownerless branch in `Execute`; `ScheduleHostileCompanionAI` |
| `script/LivingWorldPlayerScript.cpp` | Hostile-bot detection in `OnPlayerLogin` |

**Verification:** Hostile bot in Orgrimmar → player attacks it → bot fights back
with class spells → bot is not in player's party UI.

### Phase 3 — Ambient World Population
**Effort: large (~200 lines + DB)**

**Status: ✅ COMPLETE**

See zone index, activity library, and session composer design above (Categories
4 and 5 share this infrastructure). The first implementation may start with
simple activity selection, but the intended target is a **multi-task chained
session composer** that can build 3–10 step bot routines from DB-authored task
types and level-appropriate lookup tables.

Key deliverables:

1. `living_world_zone_index` schema + ~70-row seed.
2. `living_world_activity_library` schema + starter rows.
3. `BotActivitySessionComposer` service — destination-first step-list builder.
4. `AmbientBotAI` event — step executor, no combat, no owner.
5. `LivingWorldWorldScript::OnUpdate` — population tick wires up ambient spawn
   and despawn.

### Implementation notes

**Spawn path:** Ambient bots reuse `SpawnHostileBotPlayerOnAccount` with
`ObjectGuid::Empty` as the owner sentinel (same as hostile bots). The
distinction is made at login: `OnPlayerLogin` checks for a row in
`living_world_ambient_bot`; if found → `ScheduleAmbientBotAI`; if not →
`ScheduleHostileCompanionAI`.

**`BotActivitySessionComposer`** (`service/BotActivitySessionComposer.cpp`):
- Loads eligible activities from `living_world_activity_library` filtered by
  faction, level, and profession flags.
- Cross-references `living_world_zone_index` to enforce zone-type and
  level-band (±5 levels) constraints.
- Selects 3–5 tasks via weighted random pick without replacement, subject to:
  - per-family cap (`max_per_session`)
  - per-zone cap (≤2 visits to the same zone per session)
  - chain rules (same task family not back-to-back, except `city_errand`)
  - relaxed chain-rule fallback so a session is never starved
- Emits one `Travel` step + one activity step per task.
  Travel: `MovePoint` for same-map; `TeleportTo` for cross-map.
  Activity: time-elapsed simulation for Phase 3 (`GatherHerb`, `GatherOre`,
  `Fish`, `Patrol`, `Idle`). Real node interaction deferred to Phase 4.

**`AmbientBotAIEvent`** (`ai/AmbientBotAI.cpp`):
- Fires every 500 ms via `m_Events`.
- Handles `NotInWorld` retries (up to 8, exponential back-off).
- Drives `TickTravel` (arrival-threshold check, re-issues `MovePoint` only
  when not already heading there) and `TickActivity` (duration countdown,
  60-second heartbeat log).
- On session complete: writes `is_available = 1` / `last_activity_at = NOW()`
  to `living_world_ambient_bot` and calls `LogoutPlayer`.

**`LivingWorldWorldScript::OnUpdate`** (`script/LivingWorldWorldScript.cpp`):
- `LivingWorld.AmbientPopulation` config key (default 3) sets target pop.
- `LivingWorld.AmbientPopulationTickMs` (default 5 min) controls tick frequency.
- Each tick: counts `is_available = 0` rows; spawns up to `toSpawn` available
  bots; marks each `is_available = 0` immediately to prevent double-spawn.

**`OnPlayerLogin`** (`script/LivingWorldPlayerScript.cpp`):
- Ownerless sentinel path (`ownerGuid.GetCounter() == 0`) checks
  `living_world_ambient_bot` first → `ScheduleAmbientBotAI`, else →
  `ScheduleHostileCompanionAI`.

**`OnPlayerLogout`** (`script/LivingWorldPlayerScript.cpp`):
- Unconditionally executes `UPDATE living_world_ambient_bot SET is_available = 1`
  (no-op for non-ambient bots).

### Files changed

| File | Change |
|------|--------|
| `ai/AmbientBotAI.h` | `ScheduleAmbientBotAI` declaration |
| `ai/AmbientBotAI.cpp` | Step executor event: Travel, activity types, session completion, logout |
| `service/BotActivitySessionComposer.h` | `AmbientStep`, `AmbientSessionTask`, `AmbientSession`, `BotActivitySessionComposer` |
| `service/BotActivitySessionComposer.cpp` | Chained session builder: zone filter, weighted pick, chain rules |
| `integration/SqlZoneIndexRepository.h/.cpp` | Reads `living_world_zone_index` |
| `integration/SqlActivityLibraryRepository.h/.cpp` | Reads `living_world_activity_library` |
| `integration/BotActivityLog.h/.cpp` | Structured activity event logging to `living_world_bot_activity_log` |
| `script/LivingWorldPlayerScript.cpp` | Ownerless login → ambient vs hostile routing; logout cleanup |
| `script/LivingWorldWorldScript.cpp` | `TickAmbientPopulation` population tick |
| `data/sql/characters/living_world_ambient_bot.sql` | Schema: `bot_account_id`, `character_guid`, profile fields, `is_available` |
| `data/sql/characters/living_world_bot_activity_log.sql` | Schema: per-event log rows |
| `modules/…/pending_db_world/rev_living_world_009_zone_index.sql` | Schema + seed rows for `living_world_zone_index` |
| `modules/…/pending_db_world/rev_living_world_010_activity_library.sql` | Schema + starter rows for `living_world_activity_library` |
| `modules/…/pending_db_world/rev_living_world_011_task_library_fields.sql` | Added `task_family`, `max_per_session` columns |
| `modules/…/pending_db_world/rev_living_world_012_tier2_seed_pass.sql` | Expanded activity seed: city idles, patrols, gathering, fishing |

**Verification:** Ambient bot spawned anywhere → assigned a chained session
(e.g. "Herb Run – Durotar → Patrol Stonetalon") → travels to first
destination, never works in its spawn zone. World tick maintains configured
number of ambient bots online. Bots log out on session completion and
`is_available` returns to 1.

### Phase 4 — Guild Bots
**Effort: large (~300 lines + DB)**

1. `living_world_guild_bot` and `living_world_guild_bot_deposit_log` schemas.
2. `GuildBotAI` event — extends `AmbientBotAI` with a DEPOSIT step at session end.
   DEPOSIT step: travel to nearest guild bank NPC, deposit all gathered items
   and gold above floor via `Guild::DepositItem` / `Guild::DepositMoney`.
3. GATHER step type (step_type = 6) — new primitive in the activity library.
   C++ side: bot interacts with the nearest node matching `target_type`
   (herb/ore/fish), loot is added to inventory normally.
4. `OnPlayerLogin` detects guild bot flag → schedules `GuildBotAI`.
5. `.lwbot guild list/request/dismiss/deposit` command handlers.
6. Autonomous wake tick in `LivingWorldWorldScript::OnUpdate` — respects
   `last_deposit_at` and `is_available` to avoid double-spawning.
7. Add a guild-side forced-task surface for permanent guild bots:
   - custom pedestal/object near the guild bank
   - assign rabbit tasks like ore/herb runs
   - force return/deposit behavior
   - later allow richer task assignment without turning guild bots into generic
     account-alt companions

Additional design note:
- guild bots are the main Tier 2 subtype that should keep a stable identity and
  may justify real bags/inventory state rather than purely fake economic output.

**Verification:**
- Guild bot with herbalism spawns, travels to Felwood, gathers herbs (nodes
  disappear from world), inventory fills with real herb items, bot travels to
  Orgrimmar guild bank, deposits herbs, log row written.
- `.lwbot guild request <name>` → bot appears at player location, follows,
  fights if player engages combat.
- `.lwbot guild list` shows guild bots with their current activity/zone.

---

## Critical Files

| File | Phase | Status | Change |
|------|-------|--------|--------|
| `script/LivingWorldCommandScript.cpp` | 1 | ✅ done | Add `.lwbot raid` command handlers |
| `integration/BotSessionFactory.h/.cpp` | 1,2 | ✅ done | Raid pool + hostile spawn modes |
| `script/LivingWorldPlayerScript.cpp` | 1,2,3 | ✅ done | Detect bot type on login, route to correct AI; ambient/logout cleanup |
| `ai/CompanionAI.cpp` | 2 | ✅ done | Hostileless target fallback |
| `data/sql/…/living_world_pool_character.sql` | 1 | ✅ done | Schema + seed characters |
| `ai/AmbientBotAI.h/.cpp` | 3 | ✅ done | Ambient AI step executor |
| `script/LivingWorldWorldScript.cpp` | 3,4 | ✅/pending | Population tick — ambient done; guild bot tick pending |
| `service/BotActivitySessionComposer.h/.cpp` | 3 | ✅ done | Destination-first step-list builder |
| `integration/SqlZoneIndexRepository.h/.cpp` | 3 | ✅ done | Read `living_world_zone_index` |
| `integration/SqlActivityLibraryRepository.h/.cpp` | 3 | ✅ done | Read `living_world_activity_library` |
| `data/sql/…/living_world_zone_index.sql` | 3 | ✅ done | Schema + seed rows |
| `data/sql/…/living_world_activity_library.sql` | 3 | ✅ done | Schema + starter/expanded rows |
| `integration/BotActivityLog.h/.cpp` | 3 | ✅ done | Structured activity event logging |
| `ai/GuildBotAI.h/.cpp` | 4 | ⬜ pending | Guild AI — extends AmbientBotAI + DEPOSIT step |
| `data/sql/…/living_world_guild_bot.sql` | 4 | ⬜ pending | Schema |
| `data/sql/…/living_world_guild_bot_deposit_log.sql` | 4 | ⬜ pending | Schema |
| `script/LivingWorldCommandScript.cpp` | 4 | ⬜ pending | Add `.lwbot guild` command handlers |
