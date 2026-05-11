# Living World Generic Task Intent Schema

## Goal

Move Living World task authoring from **hardwired concrete destinations** like:

- `Gather Ore in Grizzly Hills`
- `Fish in Feralas`
- `Idle in Stormwind`

to **generic reusable intents** like:

- `Gather Ore <Auto>`
- `Gather Ore <Zone>`
- `Gather Ore <Ore Name/ID>`
- `Quest <Auto>`
- `Quest <Zone>`
- `Grind <Monster Name/ID>`

while keeping the existing strengths of the current system:

- playlists as ordered "day in the life" sequences
- reusable tasks/templates
- automated travel via task points and transit routes
- faction / level / profession eligibility

---

## Current Architecture Summary

Today the world-bot pipeline is roughly:

`Playlist -> Task Template -> Task Template Step -> Zone/Point -> Travel/Activity Steps`

The playlist still remains a list of **built/authored tasks**.

That part is intentional and should stay explicit.

What changes is the shape of each built task: instead of hand-authoring every
possible concrete destination permutation, the built task can express a broader
intent that resolves from bot context such as:

- faction
- level band
- professions
- home base
- current zone / travel distance
- abstract vs materialized execution mode

This already works well for:

- authored city loops
- authored gathering circuits
- transit-aware movement between named hubs

The current limitation is that most task definitions are still authored as
**specific zone-specific seeds**, not as generic semantic intents.

---

## Design Direction

Do **not** replace playlists.

Do **not** replace the current transit or point system.

Instead, add a new **intent-resolution layer** between authored task steps and
their concrete zone/point destination.

### New conceptual flow

`Playlist -> Task Template -> Intent Step -> Resolver -> Concrete Zone/Point -> Travel/Activity/Return`

This keeps the current authored-session model, but makes the individual task
steps reusable across:

- ambient world bots
- future guild bots
- command-driven bot instructions
- future quest/grind automation

It also removes the need to hardcode most city-specific routines.

For example, `City Chores` should become a reusable intent that works for any
bot once the resolver knows:

- the bot's assigned home city / home hub
- how far away the bot currently is
- whether hearth, portal, taxi, boat, zeppelin, or local travel is the best way
  home

That means authored playlists can stay broad, such as:

- `City Chores`
- `Quest`
- `Gather`
- `City Chores`
- `Gather`
- `Fish`
- `Fish`
- `Logout`

while the resolver decides the concrete world behavior.

So the intended target model is:

- **Playlist** = ordered list of authored built tasks
- **Built task** = a reusable smart intent with background resolution

This preserves author control over the day structure while removing the need to
hand-build every faction/level/profession/geography solution separately.

---

## Home Base / Major City Assignment

To support generic city chores and believable long-distance routing, each bot
should have a persistent **home base**.

## New concept: bot home hub

Each bot should have an assigned:

- major city
- or major faction hub
- or neutral late-game hub when appropriate

Examples:

- low Alliance bot -> Stormwind or Ironforge
- low Horde bot -> Orgrimmar or Undercity / Thunder Bluff
- Outland bot -> Shattrath
- Northrend bot -> Dalaran

This should be treated as persistent identity/world-state metadata, not a
temporary task choice.

## Why it matters

When a bot is in a distant questing zone and gets the task `City Chores`, the
system should not need a separate hardcoded routine like `Stormwind chores` or
`Orgrimmar chores`.

Instead:

1. read bot home base
2. determine whether the bot is already near home
3. if far away, resolve best long-distance return method
4. arrive at home city
5. run generic city chores there

## Suggested data addition

This does not have to live in world DB if bot identity already lives elsewhere,
but conceptually each bot needs:

- `home_zone_id`
- `home_anchor_point_key` (optional)
- `home_bind_point_key` (optional, hearth destination)
- `home_faction_city_role` or similar semantic tag

For manager-owned world bots, this could later live either in:

- world-bot identity rows
- or a separate bot-home assignment table

---

## Generic City Chores Intent

`City Chores` should become a first-class generic task intent.

### Instead of

- `Stormwind City Services`
- `Orgrimmar City Services`
- `Dalaran City Services`

### Use

- `City Chores <Home>`
- `City Chores <Specific City>`
- `City Chores <Nearest Major Hub>`

### Suggested resolution

#### `City Chores <Home>`

Resolve to the bot's assigned home base.

#### `City Chores <Specific City>`

Resolve using a specific zone or anchor.

#### `City Chores <Nearest Major Hub>`

Resolve by ranking eligible city anchors by:

- faction compatibility
- level suitability
- proximity / travel cost
- authored weight

### What a city-chore step expands into

`City Chores` is not one location; it is a compact sequence, for example:

- return to city/hub
- mailbox
- auction house
- bank
- inn / idle

That suggests a second useful abstraction:

## Suggested reusable routine profile table

`living_world_routine_profile`

This would let a generic intent like `city_chores` expand into a named sequence
of semantic subtasks without binding it to one specific city.

Then per-city anchors determine which concrete mailbox / bank / inn gets used.

---

## Long-Distance Travel Resolution

The resolver should explicitly support **return-home travel strategy**, not just
point-to-point transit.

## New concept: travel mode selection policy

When a bot receives a broad task, the system should classify the trip roughly as:

- local
- same-zone
- same-continent long distance
- cross-continent
- cross-expansion / portal-class long distance

Then choose from:

- walk / mount
- taxi
- boat
- zeppelin
- portal
- hearth / bind-home shortcut

### Example

If a bot is questing in The Hinterlands and receives `City Chores <Home>` and
home is Orgrimmar:

1. determine that the bot is very far from home
2. if hearth is valid and off cooldown, prefer hearth
3. otherwise resolve the long-distance transit graph
4. arrive at home hub
5. begin city chores

### Suggested new travel action concept

Add a semantic travel action such as:

- `travel_home`
- `travel_to_home_hub`
- `travel_to_anchor_role`

This keeps playlists broad while letting the system choose the concrete route.

---

## Zone Questing / Fake Quest Loop Support

Your example also points to a broader need:

`Quest <Auto>` should not mean “teleport to a quest row.”

It should mean:

1. choose an appropriate questing zone for level/faction/context
2. choose a quest hub / outpost anchor in that zone
3. travel there intelligently
4. simulate an out-and-back loop:
   - leave hub
   - perform fake questing / grinding / gathering actions
   - return to hub
   - optionally repeat

This means the schema needs not just zone content, but **hub semantics**.

That is exactly why `living_world_zone_anchor` should carry roles like:

- `quest_hub`
- `outpost`
- `town`
- `flight_master`

and why `return_anchor_role` belongs on the step.

---

## Abstract vs Materialized Execution

This design should explicitly respect the world-bot simulation split already
described in `bot_expansion.md`.

## Key rule

Not every bot needs full path-by-path resolution all the time.

### When bot is abstract/offscreen

The system can resolve a broad task at a coarse level and then advance by timers:

- `City Chores` -> reserve N minutes
- `Quest` -> reserve N minutes
- `Gather` -> reserve N minutes
- `Fish` -> reserve N minutes

In this mode, the bot may only need:

- source zone
- destination zone/hub
- estimated travel method
- elapsed step timer

### When bot is materialized in a player-interest zone

The system should do the more detailed version:

- full route resolution
- concrete anchor selection
- visible travel
- visible local behavior
- visible return-to-hub loops

## Important consequence

The resolver should support **two fidelity levels**:

1. **abstract resolution** - enough to know where the bot conceptually is and
   how long the step should take
2. **materialized resolution** - enough to spawn believable movement and local
   action in front of players

This keeps the system scalable while still allowing “smart” broad playlists.

## Proposed Schema

## 1. Extend `living_world_task_template_step`

Keep the existing table, but add generic resolution fields.

### Existing fields kept

- `template_id`
- `step_order`
- `step_type`
- `target_zone_id`
- `target_point_key`
- `duration_min_sec`
- `duration_max_sec`
- `label`

### New fields

- `resolver_kind` VARCHAR(32) NOT NULL DEFAULT `'point'`
- `subject_kind` VARCHAR(32) NULL
- `subject_id` INT UNSIGNED NULL
- `subject_key` VARCHAR(64) NULL
- `return_anchor_role` VARCHAR(32) NULL
- `cycle_count` TINYINT UNSIGNED NOT NULL DEFAULT `1`

### Meaning

#### `resolver_kind`

How the step chooses its destination.

Suggested values:

- `point` - exact named point (`target_point_key`)
- `zone` - exact zone (`target_zone_id`)
- `auto_zone` - choose best eligible zone automatically
- `resource_zone` - use a specific zone, but resolve a matching resource there
- `resource_auto` - choose the best eligible resource zone automatically
- `creature_zone` - use a specific zone, but resolve a creature/grind target there
- `creature_auto` - choose the best eligible grind zone automatically
- `quest_zone` - use a specific zone quest hub
- `quest_auto` - choose the best eligible quest hub automatically

#### `subject_kind`

What the step is really trying to do.

Suggested values:

- `ore`
- `herb`
- `fish`
- `quest`
- `creature`
- `city_service`

#### `subject_id`

Optional specific DB/game entry.

Examples:

- ore node entry id
- herb node entry id
- creature entry id
- quest id

#### `subject_key`

Optional stable string identifier when a numeric ID is not the best authoring key.

Examples:

- `copper_vein`
- `peacebloom`
- `starter_quest_hub`

#### `return_anchor_role`

Tells the runtime where to return after the step if the step implies an
out-and-back loop.

Suggested values:

- `none`
- `town`
- `outpost`
- `quest_hub`
- `flight_master`
- `home`

#### `cycle_count`

How many gather / grind / quest / return loops to simulate before advancing.

---

## 2. Add `living_world_zone_anchor`

The current `living_world_zone_index` only has one anchor position per zone.
That is useful, but too coarse for flexible behavior.

Instead of duplicating coordinates again, reuse `living_world_task_point` as the
canonical coordinate table and add semantic zone roles separately.

### Table

`living_world_zone_anchor`

Columns:

- `anchor_id` INT UNSIGNED AUTO_INCREMENT PRIMARY KEY
- `zone_id` INT UNSIGNED NOT NULL
- `point_key` VARCHAR(64) NOT NULL
- `anchor_role` VARCHAR(32) NOT NULL
- `required_faction` TINYINT UNSIGNED NOT NULL DEFAULT 0
- `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1
- `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80
- `weight` TINYINT UNSIGNED NOT NULL DEFAULT 1
- `notes` VARCHAR(255) NULL

### Example roles

- `town`
- `outpost`
- `quest_hub`
- `flight_master`
- `resource_hub`
- `return_point`
- `portal_arrival`

### Why this matters

This gives bots semantic places to:

- start from
- return to
- use for flight-path search
- simulate quest loops from
- simulate guild-bot deposit / home routing later

---

## 3. Add `living_world_zone_content`

This is the lookup table that makes generic authoring possible.

It becomes the authoritative per-zone catalog of what the zone supports.

### Table

`living_world_zone_content`

Columns:

- `content_id` INT UNSIGNED AUTO_INCREMENT PRIMARY KEY
- `zone_id` INT UNSIGNED NOT NULL
- `content_kind` VARCHAR(32) NOT NULL
- `subject_id` INT UNSIGNED NULL
- `subject_key` VARCHAR(64) NULL
- `display_name` VARCHAR(100) NOT NULL
- `required_faction` TINYINT UNSIGNED NOT NULL DEFAULT 0
- `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1
- `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80
- `min_skill` SMALLINT UNSIGNED NOT NULL DEFAULT 0
- `max_skill` SMALLINT UNSIGNED NOT NULL DEFAULT 0
- `weight` TINYINT UNSIGNED NOT NULL DEFAULT 1
- `anchor_point_key` VARCHAR(64) NULL
- `return_anchor_role` VARCHAR(32) NULL
- `notes` VARCHAR(255) NULL

### Example `content_kind` values

- `ore`
- `herb`
- `fish`
- `quest`
- `grind`

### Example usages

#### Gather Ore <Auto>

Find all `living_world_zone_content` rows where:

- `content_kind = 'ore'`
- profession / skill / faction / level match

Then rank by:

- zone level fit
- travel suitability
- weight
- current zone proximity

#### Gather Ore <Zone>

Filter the same table by `zone_id` first.

#### Gather Ore <Ore Name/ID>

Filter by:

- `content_kind = 'ore'`
- `subject_id = ?` or `subject_key = ?`

#### Grind <Monster Name/ID>

Use:

- `content_kind = 'grind'`
- `subject_id = creatureEntry`

#### Quest <Auto>

Use:

- `content_kind = 'quest'`
- `anchor_point_key` or `return_anchor_role = 'quest_hub'`

---

## 4. Keep `living_world_zone_index`, but narrow its role

`living_world_zone_index` should stay as the **coarse zone metadata table**.

It already contains useful fields:

- zone id / map id / name
- faction lean
- zone type
- level band
- basic resource booleans
- fallback anchor

### Recommended role after the redesign

Use it for:

- fast coarse filtering
- default fallback anchor when no better anchor exists
- zone type / level band / faction compatibility

Use `living_world_zone_content` as the more detailed answer to:

- what exactly can be gathered here
- what exactly can be killed here
- what kind of quest loop this zone supports

Use `living_world_zone_anchor` as the more detailed answer to:

- where a bot should start
- where a bot should return
- where flight-path / hub routing should attach

---

## Runtime Resolution Model

## Resolution order for a generic step

### A. Resolve the target

From the step:

- read `step_type`
- read `resolver_kind`
- read `subject_kind`
- optionally read `target_zone_id`
- optionally read `subject_id` / `subject_key`

### B. Build candidate rows

From:

- `living_world_zone_index`
- `living_world_zone_content`
- `living_world_zone_anchor`
- `living_world_task_point`
- `living_world_transit_route`

### C. Rank candidates

Rank with a weighted score using:

- faction compatibility
- level suitability
- profession / skill suitability
- travel cost from current zone
- authored `weight`
- preferred anchor availability

### D. Pick concrete destination

Resolve to:

- a concrete `zone_id`
- a concrete `point_key` or anchor point
- an optional return anchor

### E. Expand into concrete runtime steps

Produce the same kind of steps the runtime already understands:

- travel
- taxi / portal / boat / zeppelin travel if needed
- activity duration step
- optional return step

This means the existing session-composition pipeline survives; the new work is
mostly in the step resolver.

## Additional resolution pass: fidelity selection

Before step expansion, the runtime should decide whether the bot is:

- `abstract`
- `materialized`

### If abstract

Expand into coarse state:

- origin zone
- destination zone / anchor role
- chosen travel mode class
- estimated duration

### If materialized

Expand into full concrete steps:

- travel to flight master / portal / boat / zeppelin / hearth destination
- execute local city / quest / gather / fish actions
- optional return to home or hub

This makes the same authored intent work in both simulation modes.

---

## Command/Authoring Examples

## Gather Ore <Auto>

Suggested step row:

- `step_type = 'gather_ore'`
- `resolver_kind = 'resource_auto'`
- `subject_kind = 'ore'`
- `target_zone_id = 0`
- `subject_id = NULL`

## Gather Ore <Hillsbrad>

- `step_type = 'gather_ore'`
- `resolver_kind = 'resource_zone'`
- `subject_kind = 'ore'`
- `target_zone_id = 267`

## Gather Ore <Copper Vein>

- `step_type = 'gather_ore'`
- `resolver_kind = 'resource_auto'`
- `subject_kind = 'ore'`
- `subject_id = <copper vein entry>`

## Quest <Auto>

- `step_type = 'quest'`
- `resolver_kind = 'quest_auto'`
- `subject_kind = 'quest'`
- `return_anchor_role = 'quest_hub'`

## City Chores <Home>

- `step_type = 'city_chores'`
- `resolver_kind = 'home_city'`
- `subject_kind = 'city_service'`
- `return_anchor_role = 'town'`

Expected runtime behavior:

- if far away and hearth is valid -> hearth home
- else resolve long-distance route to home city
- then run mailbox / AH / bank / inn sequence using city anchors

## Grind <Murloc ID>

- `step_type = 'grind'`
- `resolver_kind = 'creature_auto'`
- `subject_kind = 'creature'`
- `subject_id = <creature entry>`

---

## Why This Fits Future Guild Bots

This design is good for guild bots because it separates:

- **intent** (`Gather Ore`)
- **selection policy** (`Auto`, `Zone`, `Specific target`)
- **world knowledge** (what zones contain what)
- **routing knowledge** (how to get there and where to return)

That means the same resolution machinery can be reused by:

- ambient bot playlists
- guild resource farming jobs
- command-driven gathering orders
- future quest/grind job boards

---

## Recommended Phased Migration

## Phase 1 - Add metadata only

No behavior change yet.

- add `living_world_zone_anchor`
- add `living_world_zone_content`
- define bot home-base assignment storage
- keep existing seeds fully working
- continue using current concrete `target_zone_id` / `target_point_key`

## Phase 2 - Extend task-template-step schema

- add `resolver_kind`
- add `subject_kind`
- add `subject_id`
- add `subject_key`
- add `return_anchor_role`
- add `cycle_count`

Default all old rows to `resolver_kind = 'point'` or `resolver_kind = 'zone'`
depending on what they already use.

## Phase 3 - Add resolver code in composer/runtime

- resolve generic steps into concrete destinations
- add home-city / home-hub resolution
- add hearth-aware travel-home logic
- preserve current travel/transit logic
- preserve current playlist/template compatibility

## Phase 3.5 - Add dual-fidelity execution

- abstract bots use coarse destination + timer progression
- materialized bots use full route/anchor/action expansion
- both share the same authored high-level intent

## Phase 4 - Start migrating authored content

Examples:

- `Gather Herbs in Ashenvale` -> `Gather Herbs <Zone: Ashenvale>`
- `Gather Ore in Grizzly Hills` -> `Gather Ore <Zone: Grizzly Hills>`
- later -> `Gather Ore <Auto>`

## Phase 5 - Editor / tooling support

Update the LW editor so authors work with:

- resolver kind dropdowns
- subject kind dropdowns
- zone content rows
- zone anchors

instead of manually typing mostly raw concrete coordinates and one-off seeds.

---

## Bottom Line

The cleanest maintainable path is:

1. **keep playlists**
2. **keep automated travel**
3. **keep task points and transit routes**
4. **assign each bot a home base / major city**
5. **extend task steps to express generic intent**
6. **add zone content + zone anchors as shared world knowledge**
7. **let abstract bots run coarse timers while nearby bots run full visible routes**

That gives you one shared system for:

- ambient routines
- guild bot jobs
- player commands
- future quest/grind loops
- generic city chores / home-return behavior

without having to keep authoring separate hardcoded versions of the same task
for every zone.