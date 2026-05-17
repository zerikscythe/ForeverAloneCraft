# World Bot Quest Hub Progression Design

This note describes how LivingWorld should evolve from broad "quest somewhere in
this zone" behavior toward believable **quest-hub progression** driven by real
quest-chain follow-ons.

The goal is to keep the **travel magic** behind the scenes while making the
resulting bot behavior look more like an actual player moving from hub to hub.

---

## Problem

The current system can already do useful pieces of the job:

- pick eligible task templates/playlists by faction, level, and profession
- resolve `quest_auto` / `quest_zone` style steps into zone or point targets
- travel to those targets using:
  - ground travel
  - route-network planning
  - dynamic taxi
  - authored transit routes for boat / zeppelin / portal

But current quest-style selection is still too broad.

Today the scheduler mostly thinks in terms of:

- zone anchor
- generic zone content
- template step target

It does **not** yet think in terms of:

- quest hub in this zone
- how many quests are here
- where this hub naturally leads next
- whether the bot should continue the story, branch, or stop for now

That is the missing layer.

---

## Desired behavior

The intended questing loop is:

1. choose a valid quest hub for the bot
2. travel to the hub's quest-giver anchor
3. spend a believable amount of time there
4. inspect authored follow-on branches
5. choose the next hub with weighted randomness
6. hand control back to the travel planner
7. repeat until the session winds down
8. return the bot to abstract ledger state when appropriate

This keeps responsibilities clean:

- **quest-hub graph** decides where the bot should plausibly go next
- **travel planner** decides how to get there
- **local activity runtime** decides what the bot looks like while "working" the
  current hub

---

## Runtime data shape

We do **not** want the server reading the full extracted editor quest cache as
its primary runtime format.

Instead, we want a smaller derived runtime graph.

Suggested shape:

```json
{
  "version": 1,
  "zones": [
    {
      "zoneId": 40,
      "zoneName": "Westfall",
      "mapId": 0,
      "hubs": [
        {
          "hubId": "westfall_sentinel_hill",
          "faction": 1,
          "levelMin": 10,
          "levelMax": 18,
          "questCount": 10,
          "estimatedMinutesMin": 35,
          "estimatedMinutesMax": 65,
          "npcIds": [234],
          "pointKey": "quest_hub_westfall_sentinel_hill",
          "position": { "mapId": 0, "x": -10684.2, "y": 1033.5, "z": 34.9 },
          "branches": [
            {
              "targetHubId": "westfall_moonbrook",
              "targetZoneId": 40,
              "weight": 5,
              "leadCount": 5
            },
            {
              "targetHubId": "redridge_lakeshire",
              "targetZoneId": 44,
              "weight": 2,
              "leadCount": 2
            }
          ]
        }
      ]
    }
  ]
}
```

Core fields:

- `zoneId`, `zoneName`, `mapId`
- `hubId`
- `faction`
- `levelMin`, `levelMax`
- `questCount`
- `estimatedMinutesMin`, `estimatedMinutesMax`
- `npcIds`
- `pointKey` and/or explicit world position
- `branches[]`

Important design rule:

- branch **hub-to-hub**, not only zone-to-zone

That lets the travel planner head toward a real NPC/world anchor instead of a
vague zone center.

---

## Weight semantics

Branch weights are used by **weighted random selection**.

Example:

- branch A weight `6`
- branch B weight `3`
- branch C weight `1`

Total weight = `10`

Roll `1..10`:

- `1..6` => A
- `7..9` => B
- `10` => C

That means:

- higher weight = more likely
- lower weight = still possible

Recommended first-pass weight rule:

- `baseWeight = number of quest leads from this hub to that target hub`

Optional later modifiers:

- boost same-zone continuation slightly
- reduce hubs visited very recently
- zero out hubs outside faction/level range
- reduce routes requiring special transit if desired
- personality modifiers:
  - story-follower boosts strongest branch
  - wanderer flattens weights

For v1, keep it simple:

- use extracted lead count
- clamp to a practical range such as `1..10`
- do one weighted roll across valid branches

---

## Time spent in a hub

`questCount` should influence how long a bot stays in the hub.

This does **not** need to simulate the exact live quest log.

Suggested first-pass rule:

- use `questCount` to derive a min/max stay window
- or precompute `estimatedMinutesMin/Max` during export

Examples:

- tiny hub with 2-3 quests: `10-20` minutes
- medium hub with 6-10 quests: `25-60` minutes
- large hub with 12+ quests: `45-90` minutes

This gives us believable pacing without a full quest engine.

---

## Relationship to the editor

The lw-editor should remain focused on **world truth**, not visible travel task
logic.

The editor's job is to own:

- route corridors
- seams / connectors
- landmarks
- extracted quest/NPC data
- possibly quest-hub export helpers later

The editor does **not** need to expose:

- full bot playlists
- explicit travel-leg authoring for every quest step
- runtime scheduling decisions

The server/runtime should consume a **derived quest-hub graph** exported from
the richer quest data already available to tooling.

---

## Relationship to travel planning

Quest-hub progression should plug into the existing travel stack, not replace
it.

The travel pipeline remains:

1. current location
2. destination hub anchor
3. resolve best travel mode
4. execute ground / road / taxi / boat / zeppelin / portal legs
5. arrive at hub

That means the quest-hub graph only needs to answer:

- what hub are we going to next?

It does **not** need to answer:

- how do we physically traverse the world?

That stays in the route/transit planner.

---

## Integration with the current scheduler

The current scheduler already has a lot of the plumbing we need.

### What already exists

`BotActivitySessionComposer` can already:

- load eligible templates and playlists
- resolve a template step by:
  - point
  - zone
  - `home_city`
  - `resource_auto`
  - `resource_zone`
  - `quest_auto`
  - `quest_zone`
  - `creature_auto`
  - `creature_zone`
- add explicit `Travel` steps
- add authored or dynamic transit legs
- chain multiple tasks into one session

Relevant files:

- `modules/mod-living-world/src/service/BotActivitySessionComposer.cpp`
- `modules/mod-living-world/src/integration/SqlTaskPointRepository.cpp`
- `modules/mod-living-world/src/integration/SqlTaskTemplateRepository.cpp`

### What is still too broad

Current `quest_auto` resolution behaves more like:

- "pick some eligible quest content for the current level/faction"

It does not yet carry:

- previous hub identity
- branch candidates from that hub
- weighted follow-on continuation
- NPC-anchor follow-up destination selection

### What we should add

We should introduce a **quest hub resolver layer** that sits between template
intent and final target resolution.

Recommended first-pass components:

1. `QuestHubGraphRepository`
   - loads the small derived hub graph JSON
   - exposes lookup by:
     - faction
     - level
     - current zone
     - current hub

2. `QuestHubResolver`
   - choose initial hub for `quest_auto`
   - choose follow-on hub after a timed quest block ends
   - compute weighted branch selection

3. session/runtime state additions
   - current hub id
   - current quest chain branch source
   - recent hub history

4. optional point registration
   - each quest hub should ideally have a `living_world_task_point` row so the
     travel planner can keep using point-based destination logic

---

## Recommended implementation strategy

### Slice 1: export and load the hub graph

Deliverables:

- small runtime JSON format
- exporter from existing extracted quest data
- server-side repository loader

No session logic changes yet beyond loading/validation.

### Slice 2: initial `quest_auto` chooses a hub, not a broad zone row

Deliverables:

- `quest_auto` resolves to a quest hub anchor
- duration derived from `questCount` / estimated stay time
- `AmbientSessionTask` stores selected hub metadata

This alone will make quest destinations more grounded.

### Slice 3: end-of-block branch continuation

Deliverables:

- after a quest block, look up follow-on hubs
- weighted branch roll
- prefer story continuation over random zone reselection
- fall back cleanly when no branch is valid

This is the slice that makes hub-to-hub questing feel alive.

### Slice 4: ledger/session continuity

Deliverables:

- persist current/last hub id in ledger/runtime
- avoid immediate bounce-back loops
- enable offscreen continuation across materialization boundaries

### Slice 5: richer local hub behavior

Deliverables:

- optional use of quest-giver / field-area anchors
- localized movement among sub-anchors
- more believable "working the hub" behavior

---

## Fallback rules

We should keep the system resilient when the graph is incomplete.

If no suitable branch is found:

1. try another eligible hub in the same zone
2. try a nearby same-level hub
3. fall back to broad `quest_auto` zone selection
4. if still nothing useful, return to ledger

This lets us adopt the hub graph incrementally without breaking questing for
unfinished areas.

---

## Practical implications for current code

To mold the current scheduler into this design, the main changes will likely be:

1. add a new hub-graph data source and repository
2. extend session task metadata with selected hub identity
3. teach `quest_auto` resolution to use the hub resolver
4. teach post-step/session continuation to inspect hub branches before rolling a
   generic next task
5. preserve the existing travel planner as-is, only changing the destination
   selection inputs

The key point is that this is **not** a rewrite of travel.

It is mostly a rewrite of:

- destination choice
- continuation choice
- quest-session context

That is a good sign. It means the road/taxi/boat/zeppelin work we already have
can be reused directly.
