# Plan: World Bot Spawning — Bot Taxonomy and Build Phases

---

## Core Design Principle

There are **two distinct spawn technologies** in this system. Which one a bot
category uses is determined entirely by what it actually needs to do — not by
what is easiest to implement.

### Spawn Technology A — Player Sessions (accounts required)

Used when the bot must interact with systems that are gated behind a real
`Player*` object: guild bank deposits, real profession skill checks, real
inventory, appearing as an online guild member, gold transactions, quest
progression that syncs back to a source character.

Account pool: **BOTHOUSE** (account-alt clones only) and **GUILDBOT**
(guild bots only). These are completely separate pools and must never be
mixed.

### Spawn Technology B — Creatures (no accounts needed)

Used for every bot that is purely **smoke and mirrors** — it exists in the
world, moves, casts spells, and interacts with objects, but nothing it does
produces real persistent economic output. Items gathered are discarded. Nodes
despawn via their depletion state. No inventory is filled. No deposits are
made.

Creatures do not need WoW accounts. The AzerothCore session map constraint
(one account = one concurrent session, hard-enforced in `WorldSessionMgr`)
is entirely irrelevant for creature bots. You can spawn 500 creature bots
without creating a single WoW account.

### The full split

| Category | Implementation | Accounts needed |
|---|---|---|
| Account Alt Clones | Player session (BOTHOUSE pool) | Yes — 1 per concurrent bot |
| Guild Bots | Player session (GUILDBOT pool) | Yes — 1 per concurrent bot |
| Raid Pool Bots | Creature + `WorldBotCreatureAI` | **No** |
| Ambient World Bots | Creature + `WorldBotCreatureAI` | **No** |
| Hostile / Rival Bots | Creature + `WorldBotCreatureAI` | **No** |

**Future agents: do not revert creature categories to Player sessions.** The
reason every category was originally written as a Player session was an early
architectural assumption that has since been corrected. Creatures are the right
model for any bot that does not need real economic output.

---

## Spell System — Creatures vs Players

This is the key technical gap that must be solved before creature bots can use
the doctrine/combat profile system.

### Player sessions

A player bot reads its available spells directly from `GetSpellMap()` — the
character's learned spell list stored in `character_spell`. The doctrine
profile references spell IDs and the system checks whether the player knows
them. This works automatically because the character was built with a spell set.

### Creature bots

Creatures do not have a spell map in the same sense. Their abilities are
defined separately. For creature bots to use the same doctrine/profile system,
the server needs a way to answer: **"what spells does a level 32 Arms Warrior
know?"** without a character row.

The solution is a new DB table:

```sql
CREATE TABLE IF NOT EXISTS living_world_bot_spell_list (
    id          INT UNSIGNED     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    class_id    TINYINT UNSIGNED NOT NULL,
    spec_key    VARCHAR(32)      NOT NULL,  -- e.g. 'warrior_arms', 'mage_fire'
    min_level   TINYINT UNSIGNED NOT NULL,
    max_level   TINYINT UNSIGNED NOT NULL,
    spell_id    INT UNSIGNED     NOT NULL,
    KEY idx_lookup (class_id, spec_key, min_level, max_level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Spell availability bands for creature-based world bots';
```

`WorldBotCreatureAI` loads this table on spawn filtered by its class, spec,
and level. The resulting spell set is used identically to a player's
`GetSpellMap()` when evaluating doctrine profile entries. The profile does not
care whether the caster is a `Player*` or `Creature*` — it only cares whether
the spell ID is in the available set.

### Talent allocation

For Player sessions, talent points are loaded from `character_talent`. For
creature bots, a DB-authored talent band table (or the existing
`living_world_bot_talent_template` system) defines which talents apply at a
given level range. `WorldBotCreatureAI` applies these as passive spell
registrations at spawn time — the creature is not using the player talent UI,
it simply has the correct spells loaded.

---

## Session Design — Tier 2 Bots (Categories 2–5)

The intended behavior model for all world/system bots is **not** "pick exactly
one activity and idle there forever."

Each session bot should be assembled from:
- a bot template (class, spec, faction, level range)
- default combat doctrine for that spec
- a **random chained task list** for the session (3–10 tasks)

Example chained session:
1. Travel to a level-appropriate zone
2. Quest / grind mobs
3. Travel to a city
4. Visit auction house / mailbox / vendor / inn
5. Travel to a gathering zone
6. Gather ore or herbs
7. Patrol or idle in a hub
8. Despawn / return to pool

The session composer thinks in terms of:
- **task chaining** — believable day-in-the-life sequence
- **travel between tasks** — explicit travel legs connecting each step
- **zone-level matching** — zones appropriate to the bot's level ±5
- **session expiration** — bot despawns or returns to pool on completion

Task types are DB-authored in `living_world_activity_library`. The planner
assembles sessions from that library; no task logic is hardcoded.

### Faction conflict tasks

Some Tier 2 bots can receive aggressive tasks:
- a level 50+ rogue goes to Stranglethorn to hunt weaker opposite-faction
  targets
- a hostile bot patrols a contested road looking for trouble

These are normal planner-authored task types, not special one-off hacks.

### Scale target

Intended live direction: **300–500 active bots per faction** at peak, spanning
a broad mix of level ranges and activities. This is why creature bots (no
accounts) are the right model — you cannot realistically maintain 500+ WoW
accounts for smoke-and-mirrors world population.

---

## Bot Taxonomy

There are five distinct bot categories. They differ in who owns them, how
players interact with them, and whether they have real economic persistence.
**Never mix categories in the same command namespace.**

---

### Category 1

**What they are:** The player's own account characters, cloned to bot house
accounts as persistent avatars/aliases. When you roster-request your mage
"Fireball", a clone of Fireball logs in on a pool account and follows you.
The clone's progression syncs back to the source character.

**Spawn technology: Player session. Account pool: BOTHOUSE.**

**Key rule: `.lwbot roster list` shows ONLY the requesting player's own account
characters. Pre-built, guild, and world bots NEVER appear here.**

Commands:
- `.lwbot roster list` — your account characters only
- `.lwbot roster request <id|name>` — spawn the clone companion
- `.lwbot roster dismiss <id|name>` — log the clone out

DB backing: `living_world_account_alt_runtime` (existing).

Status: **Already works.** No changes needed in this category.

---

### Category 2 — Raid Pool Bots (`.lwbot raid`)

**What they are:** On-demand combat companions. A player calls one by role; it
appears at their location, joins the group, and fights using doctrine combat
logic. No ownership, no sync, no persistence between sessions.

**Spawn technology: Creature (`WorldBotCreatureAI`). No accounts needed.**

`creature_template` provides model, level, and base stats. `WorldBotCreatureAI`
loads the doctrine profile for the bot's class/spec and the spell list from
`living_world_bot_spell_list` filtered to the bot's level. When dismissed it
despawns and the template slot returns to `is_available = 1`.

Commands:
- `.lwbot raid request <class> <spec> <level>` — spawns a matching creature bot
  at the player, adds it to the raid group.
- `.lwbot raid dismiss <name|#>` — despawns the creature, frees the slot.

Pool table:
```sql
CREATE TABLE IF NOT EXISTS living_world_world_bot_template (
    id           INT UNSIGNED     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    class_id     TINYINT UNSIGNED NOT NULL,
    spec_key     VARCHAR(32)      NOT NULL,
    spec_role    VARCHAR(16)      NOT NULL,  -- 'tank','healer','dps'
    faction      TINYINT UNSIGNED NOT NULL,  -- 1=Alliance 2=Horde
    level        TINYINT UNSIGNED NOT NULL,
    display_id   INT UNSIGNED     NOT NULL,  -- creature_template displayid
    is_available TINYINT UNSIGNED NOT NULL DEFAULT 1,
    KEY idx_role (class_id, spec_role, level, faction, is_available)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**Implementation note — Phase 1 historical context:** The original Phase 1
implementation used Player sessions (`BotSessionFactory::SpawnBotPlayerOnAccount`
+ `living_world_pool_character`). That is functional but architecturally wrong.
Migration to `WorldBotCreatureAI` is pending as part of Phase 5.

---

### Category 3 — Guild Bots (`.lwbot guild`)

**What they are:** Pre-generated characters that are registered members of the
player's in-game guild. When a guild bot gathers resources, **those items
actually exist** — real items in real inventory, deposited to the real guild
bank.

**Spawn technology: Player session. Account pool: GUILDBOT (completely separate
from BOTHOUSE). Needs real Player sessions because it requires guild bank
access, real inventory, profession skill checks, and appearing as an online
guild member.**

Guild bots are persistent named characters — same name, same look, same guild
membership over time.

#### Operating modes

- **Autonomous** — bot selects a session from the activity library, travels,
  gathers, deposits.
- **Forced-task** — player assigns a task via guild pedestal or command.
  Overrides wandering until complete or timed out.

#### Autonomy flow

1. `LivingWorldWorldScript::OnUpdate` wakes a guild bot offline long enough.
2. `BotActivitySessionComposer` selects activity for this bot's level/professions.
3. Session: `[TRAVEL → destination, ACTIVITY, DEPOSIT → guild bank]`.
4. Gather steps use the standard loot path — real items, real node depletion.
5. Deposit step: travel to nearest guild bank NPC, deposit items and gold above
   floor. Log row written.
6. Bot logs out. `is_available` flips back.

#### Commands

- `.lwbot guild list` — guild bots with current activity and location.
- `.lwbot guild request <name|#>` — call a guild bot to the player's location.
- `.lwbot guild dismiss <name|#>` — release back to autonomous mode.
- `.lwbot guild deposit` — force immediate deposit to guild bank.

Longer-term: a **pedestal near the guild bank** for forced task assignment.

#### DB Schema

```sql
CREATE TABLE IF NOT EXISTS living_world_guild_bot (
    id                INT UNSIGNED     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_account_id    INT UNSIGNED     NOT NULL,
    character_guid    BIGINT UNSIGNED  NOT NULL,
    guild_id          INT UNSIGNED     NOT NULL,
    display_name      VARCHAR(24)      NOT NULL,
    class_id          TINYINT UNSIGNED NOT NULL,
    level             TINYINT UNSIGNED NOT NULL,
    faction           TINYINT UNSIGNED NOT NULL,
    profession_1      VARCHAR(32)      NULL,
    profession_2      VARCHAR(32)      NULL,
    is_available      TINYINT UNSIGNED NOT NULL DEFAULT 1,
    current_activity  VARCHAR(64)      NULL,
    last_seen_zone_id INT UNSIGNED     NULL,
    last_deposit_at   DATETIME         NULL,
    UNIQUE KEY uq_char (character_guid),
    KEY idx_guild (guild_id, is_available)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS living_world_guild_bot_deposit_log (
    id           BIGINT UNSIGNED  NOT NULL AUTO_INCREMENT PRIMARY KEY,
    guild_bot_id INT UNSIGNED     NOT NULL,
    guild_id     INT UNSIGNED     NOT NULL,
    deposited_at DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    item_entry   INT UNSIGNED     NULL,
    item_count   INT UNSIGNED     NULL,
    gold_copper  INT UNSIGNED     NULL,
    zone_id      INT UNSIGNED     NOT NULL,
    KEY idx_bot   (guild_bot_id),
    KEY idx_guild (guild_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Spawn path: `SpawnBotPlayerOnAccount` (GUILDBOT pool). `OnPlayerLogin` detects
guild bot flag → schedules `GuildBotAI`.

---

### Category 4 — Ambient World Bots (server auto-spawn, no commands)

**What they are:** Background population. Walk through cities, travel roads,
sit in inns, patrol zones. Visually real but **economically inert** — gathering
triggers node depletion but awards no items, nothing is deposited.

**Spawn technology: Creature (`WorldBotCreatureAI`). No accounts needed.**

`WorldBotCreatureAI` loads doctrine profile and spell list at spawn. The bot
follows its session task list: travel to zone, execute activity steps (patrol,
idle, simulate gathering), despawn on session complete. Node depletion is
triggered via state change — no loot path invoked.

No player command spawns or controls these. The server manages them via the
`LivingWorldWorldScript::OnUpdate` population tick.

Session structure: `[TRAVEL → destination, ACTIVITY, ...]` — travel-first,
multi-task chained sessions of 3–10 steps.

**Implementation note — Phase 3 historical context:** The original Phase 3
implementation used Player sessions (`SpawnHostileBotPlayerOnAccount` +
`living_world_ambient_bot` table). Functional but wrong technology. Migration
to `WorldBotCreatureAI` is pending as part of Phase 5.

---

### Category 5 — Hostile / Rival Bots (server auto-spawn or event trigger)

**What they are:** Rival faction bots in contested zones or city raid events.
When attacked they fight back using doctrine combat logic. Do not join any
player's party.

**Spawn technology: Creature (`WorldBotCreatureAI`). No accounts needed.**

`WorldBotCreatureAI` in hostile mode: no task list, no travel. Spawns in a
contested zone, engages valid targets via doctrine profile. Despawns or resets
when encounter ends.

**Implementation note — Phase 2 historical context:** Original implementation
used `SpawnHostileBotPlayerOnAccount` + `CompanionAI` hostileless mode. Should
be migrated to `WorldBotCreatureAI` hostile mode in Phase 5.

---

## Command Namespace Summary

| Command prefix | Bot category | Who can call it |
|---|---|---|
| `.lwbot roster …` | Account alt clones only | Owner of the account |
| `.lwbot raid …` | Raid pool bots | Any player |
| `.lwbot guild …` | Guild bots for this guild | Guild member |
| *(none — server auto)* | Ambient world bots | No player command |
| *(none — server auto)* | Hostile rival bots | No player command |

**The roster namespace must never query world bot or guild bot tables.**

---

## Shared Infrastructure

### All categories

- `BotActivitySessionComposer` — destination-first chained session builder
- `SqlZoneIndexRepository` / `SqlActivityLibraryRepository`
- `living_world_zone_index` + `living_world_activity_library` seed data
- `BotActivityLog` — structured event logging (works for Player* and Creature*)
- Doctrine profile system — combat priority rules keyed on class/spec
- `living_world_bot_talent_template` — talent bands by class/spec/level

### Player session categories only (1 and 3)

- `BotSessionFactory::SpawnBotPlayerOnAccount`
- `LivingWorldPlayerScript::OnPlayerLogin` — bot type detection and AI routing
- `BotPlayerRegistry` — active Player session bot tracking
- `living_world_bot_account_pool` — account reservation/release

### Creature categories only (2, 4, and 5)

- `WorldBotCreatureAI` — single `CreatureAI` for all creature bots; mode
  (raid/ambient/hostile) set at spawn time
- `living_world_bot_spell_list` — class/spec/level-band spell availability
  (replaces `GetSpellMap()` from Player sessions)
- `living_world_world_bot_template` — archetypes: class, spec, faction, level,
  model display ID

---

## WorldBotCreatureAI — Design Sketch

```
WorldBotCreatureAI
  OnInitialize(creature, mode, spec_key)
    load class_id, spec_key, level from spawn args
    query living_world_bot_spell_list → _availableSpells
    register talent passives from living_world_bot_talent_template
    load doctrine profile from SqlBotCombatDefaultProfileRepository
    if mode == AMBIENT or RAID: compose session from BotActivitySessionComposer
    if mode == HOSTILE: combat only, no session

  UpdateAI(diff)
    if in combat:
      TryExecuteDoctrineRotation(creature, target, _availableSpells, _profile)
    else if mode == AMBIENT:
      TickSessionStep(creature)
    else if mode == RAID:
      FollowOwner(creature)
    else if mode == HOSTILE:
      ScanForTargets(creature)
```

---

## What Already Works

- AccountAlt clone system (Category 1) — ✅ complete
- `BotSessionFactory` Player session spawn paths — ✅ complete
- `CompanionAI` doctrine loop for Player session bots — ✅ complete
- Doctrine profiles for all specs — ✅ complete
- `.lwbot raid` commands — ✅ complete *(Player session; creature migration pending)*
- Hostile bot fight-back AI — ✅ complete *(Player session; creature migration pending)*
- Ambient world population tick + `AmbientBotAI` chained sessions — ✅ complete *(Player session; creature migration pending)*
- `BotActivitySessionComposer` — ✅ complete
- `SqlZoneIndexRepository` / `SqlActivityLibraryRepository` — ✅ complete
- `living_world_zone_index` + `living_world_activity_library` seed data — ✅ complete

### Pending for creature migration (Phase 5)

- `living_world_bot_spell_list` table + seed data — ⬜
- `living_world_world_bot_template` table — ⬜
- `WorldBotCreatureAI` (`CreatureScript`) — ⬜
- `TryExecuteProfileRotation` abstracted to `Unit*` — ⬜
- Phase 1/2/3 spawn paths migrated from Player sessions to creatures — ⬜

---

## Build Phases

### Phase 1 — Raid Pool Bots (`.lwbot raid`)
**Status: ✅ COMPLETE (Player session implementation — creature migration pending)**

Commands and DB schema done. Currently uses `BotSessionFactory` +
`living_world_pool_character`. Migration to `WorldBotCreatureAI` in Phase 5.

### Phase 2 — Hostile / Rival Bots
**Status: ✅ COMPLETE (Player session implementation — creature migration pending)**

`SpawnHostileBotPlayerOnAccount` + `CompanionAI` hostileless mode working.
Migration to `WorldBotCreatureAI` hostile mode in Phase 5.

### Phase 3 — Ambient World Population
**Status: ✅ COMPLETE (Player session implementation — creature migration pending)**

`AmbientBotAI`, session composer, zone index, activity library, population tick
all working. Migration to `WorldBotCreatureAI` ambient mode in Phase 5.

### Phase 4 — Guild Bots (`.lwbot guild`)
**Status: ⬜ Pending | Effort: large (~300 lines + DB)**

1. Create GUILDBOT account pool (separate from BOTHOUSE).
2. `living_world_guild_bot` and `living_world_guild_bot_deposit_log` schemas.
3. `GuildBotAI` — Player session AI with DEPOSIT step at session end.
4. GATHER steps with real node interaction and real loot.
5. DEPOSIT step: travel to nearest guild bank NPC, deposit items and gold.
6. `OnPlayerLogin` detects guild bot flag → `GuildBotAI`.
7. `.lwbot guild list/request/dismiss/deposit` command handlers.
8. Population tick for autonomous guild bot wake.
9. Guild pedestal object for forced task assignment.

**Verification:** Guild bot with herbalism gathers herbs (nodes disappear, items
in real inventory), travels to guild bank, deposits, log row written. `.lwbot
guild request` calls it to the player's location.

### Phase 5 — Creature Bot Infrastructure (`WorldBotCreatureAI`)
**Status: ⬜ Pending | Effort: large (~400 lines + DB seed)**

1. `living_world_bot_spell_list` schema + seed data for all class/spec/level bands.
2. `living_world_world_bot_template` schema + initial archetype rows.
3. Abstract `TryExecuteProfileRotation` to accept `Unit*` caster.
4. Implement `WorldBotCreatureAI` as a `CreatureScript`.
5. Migrate Phase 1 (raid), Phase 2 (hostile), Phase 3 (ambient) from Player
   sessions to creature spawns.
6. Retire `living_world_ambient_bot` table, remove dependence on
   `SpawnHostileBotPlayerOnAccount` for world bots.

**Verification:** Ambient creature bot spawns with correct class/spec visual,
travels to a zone, executes session steps, node depletion fires without loot,
despawns on session complete. Raid creature bot follows player, uses correct
class spells, engages player targets.

---

## Critical Files

| File | Phase | Status | Notes |
|------|-------|--------|-------|
| `script/LivingWorldCommandScript.cpp` | 1 | ✅ | `.lwbot raid` handlers |
| `integration/BotSessionFactory.h/.cpp` | 1,2 | ✅ | Player session spawn paths |
| `script/LivingWorldPlayerScript.cpp` | 1,2,3 | ✅ | Bot type detection on login |
| `ai/CompanionAI.cpp` | 2 | ✅ | Hostileless target fallback |
| `ai/AmbientBotAI.h/.cpp` | 3 | ✅ | Ambient session step executor |
| `script/LivingWorldWorldScript.cpp` | 3 | ✅ | Population tick |
| `service/BotActivitySessionComposer.h/.cpp` | 3 | ✅ | Chained session builder |
| `integration/SqlZoneIndexRepository.h/.cpp` | 3 | ✅ | Zone lookup |
| `integration/SqlActivityLibraryRepository.h/.cpp` | 3 | ✅ | Activity lookup |
| `integration/BotActivityLog.h/.cpp` | 3 | ✅ | Event logging |
| `data/sql/…/living_world_ambient_bot.sql` | 3 | ✅ | Current Player session pool table |
| `ai/GuildBotAI.h/.cpp` | 4 | ⬜ | Guild AI + DEPOSIT step |
| `data/sql/…/living_world_guild_bot.sql` | 4 | ⬜ | Schema |
| `data/sql/…/living_world_guild_bot_deposit_log.sql` | 4 | ⬜ | Schema |
| `ai/WorldBotCreatureAI.h/.cpp` | 5 | ⬜ | Creature AI for Cat 2/4/5 |
| `data/sql/…/living_world_bot_spell_list.sql` | 5 | ⬜ | Spell bands for creatures |
| `data/sql/…/living_world_world_bot_template.sql` | 5 | ⬜ | Archetypes |
