## World Bot City Reserve Population Design

### Goal

Let the normal world-bot population continue to feel like a living server:

- most bots resume from their own last known location
- most bots continue the task/session history they were already on
- population naturally drifts because bots travel like players do

At the same time, major cities need a small amount of deliberate cheating so
they do not feel hollow when a player arrives.

The city reserve system adds that deliberate liveliness without turning the
whole population model into teleport spam.

### World model

There are two broad bot populations:

1. `world`
   - the default ledger population
   - resumes from last known position
   - performs questing, gathering, travel, PvP, city stops, and general world life

2. `city_reserve`
   - kept in reserve for a specific major city
   - primarily used when that city is underfilled while a player is nearby
   - returns to the ledger after city heat drops and a linger window expires

Important boundary:

- reserve bots are an exception for city liveliness
- they do not replace the normal "bots have a life history" model

### Existing content we can already reuse

The current content stack already gives us a useful first-pass city behavior
surface:

- `living_world_task_point`
  - `stormwind_mailbox`
  - `stormwind_ah`
  - `stormwind_bank`
  - `stormwind_inn`
  - `orgrimmar_mailbox`
  - `orgrimmar_ah`
  - `orgrimmar_bank`
  - `orgrimmar_inn`

- `living_world_task_template`
  - `stormwind_city_services`
  - `orgrimmar_city_services`
  - `dalaran_city_services`

- `living_world_playlist`
  - `stormwind_city_errands_routine`
  - `orgrimmar_city_errands_routine`
  - `dalaran_city_errands_routine`

- `living_world_zone_anchor` / `living_world_zone_content`
  - broad `city_service` anchor/content rows for the same cities

This means the first implementation focus is not "invent city behavior from
scratch." The first focus is:

- selecting reserve bots
- deciding when a city needs them
- keeping them around while the city is hot
- releasing them cleanly afterward

### Data model

Add identity metadata to `living_world_bot_identity`:

- `population_role`
  - `world`
  - `city_reserve`

- `reserve_city_zone_id`
  - the city this reserve bot primarily belongs to
  - examples:
    - `1519` Stormwind
    - `1637` Orgrimmar

Rules:

- normal ambient selection should prefer `population_role='world'`
- reserve bot selection should explicitly query `population_role='city_reserve'`
- reserve bots can still have normal identity data:
  - class
  - level
  - faction
  - home zone
  - last seen zone
  - resume memory

### City heat and target population

Each major city gets a target population policy:

- `softMinVisible`
- `targetVisible`
- `softMaxVisible`
- `lingerMsAfterHeatDrop`

Example intent:

- if a player nears Stormwind and visible city population is below target,
  materialize more reserve bots
- when the player leaves, do not instantly despawn them
- keep them around for a short linger window, then return surplus to the ledger

### Behavior intent for reserve bots

Reserve bots should look like players spending time in town, not field bots
wearing a city costume.

First-pass behavior families:

- `city_errand`
  - mailbox -> AH -> bank -> inn
  - bank -> mailbox -> vendor
  - inn -> mailbox -> AH

- `city_idle`
  - stand near mailbox / bank / inn / gate / district connectors

- `city_walk`
  - short cross-town walks between service anchors

Later flavor layers:

- crafting idles
- emote-at-bot / mild social noise
- short "annoying player" loops

### Population tick behavior

The ambient population tick should become a two-stage selector:

1. normal ambient fill
   - continue loading general available `world` bots

2. city reserve fill
   - if a hot major city is under target
   - pull eligible reserve bots for that city first
   - then optionally let normal world bots naturally contribute as well

Important boundary:

- we should not spread city reserve bots randomly across the world
- reserve bots should only be selected for the city they belong to

### Session composition direction

Reserve bots should bias toward city routines automatically.

That likely means:

- when `population_role='city_reserve'`
- and the bot is being pulled specifically to satisfy a city shortfall
- session composition should strongly prefer:
  - city playlists
  - city service templates
  - city content rows

This should be a bias, not a permanent hard lock.

### Linger and release

When city heat drops:

- do not immediately dematerialize reserve bots
- let them linger for a small cooldown window
- if the city remains cool and population is above the desired floor, release
  surplus reserve bots back to the ledger

This keeps the city from looking like a light switch.

### First implementation slices

#### Slice 1: identity and repository support

Files likely touched:

- `modules/mod-living-world/data/sql/characters/living_world_bot_identity.sql`
- `modules/mod-living-world/src/integration/SqlBotIdentityRepository.h`
- `modules/mod-living-world/src/integration/SqlBotIdentityRepository.cpp`

Deliverables:

- add `population_role`
- add `reserve_city_zone_id`
- exclude reserve bots from generic `LoadAvailable(...)`
- add repository helpers for loading reserve bots for a city

#### Slice 2: city reserve population controller

Files likely touched:

- `modules/mod-living-world/src/script/LivingWorldWorldScript.cpp`

Deliverables:

- define initial city policies for Stormwind / Orgrimmar
- detect underfilled hot cities
- pull reserve bots before generic world bots when a city needs them

#### Slice 3: session bias for reserve bots

Files likely touched:

- `modules/mod-living-world/src/service/BotActivitySessionComposer.h`
- `modules/mod-living-world/src/service/BotActivitySessionComposer.cpp`

Deliverables:

- bias reserve bots toward city playlists/templates/content
- preserve existing travel/task composition path

#### Slice 4: linger and release

Files likely touched:

- `modules/mod-living-world/src/script/LivingWorldWorldScript.cpp`

Deliverables:

- keep reserve bots around briefly after heat drops
- release surplus reserve bots back to ledger after cooldown

### Why this shape is worth it

This design keeps two important truths at the same time:

- the world mostly feels alive because bots have continuity
- the capitals feel alive because we deliberately support them

That balance is the point.
