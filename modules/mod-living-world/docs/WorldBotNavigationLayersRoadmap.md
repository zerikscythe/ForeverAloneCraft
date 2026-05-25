# World Bot Navigation Layers Roadmap

## Purpose

This roadmap defines the target navigation model for world bots:

1. local legal nav for ordinary nearby movement
2. learned PoI connector fallback for trusted local anchor chaining
3. small authored local assist routes for awkward geometry
4. macro travel planning for real journeys, connectors, and transit

The main goal is to stop treating all movement as one travel problem.
Bots should behave like players:

- inside a city or quest hub, usually just walk there
- if a direct walk fails, prefer trusted local anchors before hand-authored
  helpers
- for towers, ramps, docks, stairs, and similar awkward spots, use a short
  local authored assist route when needed
- only use the world travel network for longer distance movement

## Current State

Key current runtime pieces:

- [modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp](D:/src/azerothcore-wotlk/modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp)
- [modules/mod-living-world/src/service/WorldBotRoutePlanning.h](D:/src/azerothcore-wotlk/modules/mod-living-world/src/service/WorldBotRoutePlanning.h)
- [modules/mod-living-world/src/service/WorldBotTaxiPlanning.h](D:/src/azerothcore-wotlk/modules/mod-living-world/src/service/WorldBotTaxiPlanning.h)
- [src/server/game/Movement/MovementGenerators/PathGenerator.h](D:/src/azerothcore-wotlk/src/server/game/Movement/MovementGenerators/PathGenerator.h)

Important observations:

- `AmbientStepType::Travel` still mixes local movement and macro travel logic.
- strict legal path checking now exists and correctly rejects:
  - `PATHFIND_NOPATH`
  - `PATHFIND_NOT_USING_PATH`
  - `PATHFIND_SHORTCUT`
- route planner data already distinguishes:
  - `main_route`
  - `sub_route`
  - `area`
- the route editor already supports route `z_mode`:
  - `auto`
  - `manual`

That means most of the raw building blocks already exist. The missing piece is
policy: deciding when to use each movement layer.

## Desired Navigation Policy

### Layer 1: Local Nav

Use strict mmap/vmap-backed local pathing first when:

- source and destination are in the same city
- source and destination are in the same quest hub or local work pocket
- the travel is short and does not require special transit

Expected behavior:

- use legal `PathGenerator` only
- no route-network attach
- no connector lookup
- no taxi/transit planning

### Layer 2: PoI Connector Fallback

Use the learned local point graph when direct local navigation fails but the
movement still looks like a same-city or same-pocket problem.

Expected behavior:

- find the nearest trusted local anchor to the bot
- if that chain is not discoverable, try the second-nearest trusted anchor
- search short known-good directional link chains toward anchors near the
  destination
- if a connector chain succeeds, retry the final exact local leg
- after two anchor-root attempts, fail honestly and continue to later fallback
  layers

Primary data source:

- `living_world_task_point_link`

Expected runtime rules:

- use directional success history, not undifferentiated proximity
- prefer short chains with better success records
- cap local connector chain length
- do not brute-force endlessly when the graph is sparse

## Arrival Thresholds

Arrival thresholds now depend on what the bot is doing:

- authoring / harness proof: `3 yd`
- real service destination use: `5 yd`
- connector / bounce anchors: `15 yd`

This keeps learned routing data strict, keeps believable service behavior for
real PoIs, and still allows routing anchors to act as forgiving handoff points.

### Layer 3: Local Assist Route

Use a short local authored route when Layer 1 fails or the destination is known
to be awkward:

- tower stairs
- spiral ramps
- dock approaches
- cave mouths
- tower tops
- lift approaches

Expected behavior:

- treat the assist route as bidirectional
- decide direction from current position and destination intent
- use the lower end when climbing up toward the destination
- use the upper end when descending away from the destination
- follow the authored assist route
- resume local nav to the exact destination or next local target

Primary authored data source:

- `sub_route`

Expected authored metadata:

- `assist_kind`
  - `tower`
  - `portal_access`
  - `dock_access`
  - `elevator_access`
  - `stairs`
  - `cave_access`
- `lower_label`
  - lower/street/base side of the assist route
- `upper_label`
  - upper/platform/top side of the assist route
- `lower_context_keys`
  - optional hints for destinations or contexts usually served by the lower end
- `upper_context_keys`
  - optional hints for destinations or contexts usually served by the upper end

This intentionally replaces one-way `entry/exit` thinking. The same assist
route should support both directions.

These context-key lists are hints, not exhaustive whitelists. If strict local
nav already failed and the bot is stranded near one end of a known assist
route, the route may still be used as a reasonable "change layers and retry"
fallback even when the final destination is not individually listed.

### Layer 4: Macro Travel

Use the route graph, connectors, taxi logic, and special transit only when the
movement is truly a journey:

- zone transfer
- city-to-zone or zone-to-zone travel
- continent-scale travel
- explicit transit steps

Expected behavior:

- `main_route` graph
- connector/seam resolution
- taxi/transit planning
- boats/zeppelins/tram/lift later as special transit primitives

## Implementation Roadmap

### Phase 1: Separate Local and Macro Intent

Status: in progress

Add a small runtime decision layer before current travel planning that classifies
movement as one of:

- `LocalOnly`
- `LocalWithPoiConnector`
- `LocalWithAssist`
- `MacroTravel`

Initial classification rules:

- same-zone same-map movement starts `LocalOnly`
- explicit transit route or different zone -> `MacroTravel`
- local connector and assist fallbacks are chosen later only if direct local
  nav fails

Checklist:

- [x] add a movement-policy resolver near `TickStep` travel handling
- [x] keep the policy decision explicit in logs
- [x] stop calling macro travel resolution automatically for short same-zone travel

### Phase 2: Formalize Local Strict Nav

Status: partial

We already have strict path validation helpers. We need to promote them into a
first-class local navigation planner.

Checklist:

- [ ] extract local legal-nav planning into a named helper/service
- [ ] accept only real legal paths
- [ ] preserve detailed diagnostics:
  - path flags
  - point count
  - path length
- [ ] support optional partial-path diagnostics for authoring/debug

### Phase 3: Add PoI Connector Fallback Resolution

Status: in progress

Teach the runtime to use a short trusted anchor graph before it reaches for
hand-authored helper routes.

Expected resolution order:

1. try strict local nav
2. if it fails, try the nearest trusted local anchor
3. if no discoverable chain exists, try the second-nearest trusted local
   anchor
4. if a chain is found, follow the anchor sequence and retry the final local
   leg

Checklist:

- [x] add a directional local-link table for trusted PoI-to-PoI edges
- [x] harvest first Stormwind local-link batches from strict harness runs
- [x] consume learned local links in `WorldBotCreatureAI` before helper routes
- [x] cap anchor-root attempts at two
- [ ] improve connector-chain scoring and diagnostics
- [ ] add better author tooling for promoting manual verified links

### Phase 4: Add Local Assist Route Resolution

Status: partial

Teach the runtime to use `sub_route` as an assist lane for tricky short
movement.

Expected resolution order:

1. try strict local nav
2. if direct local nav and PoI connector fallback both fail, search for a
   suitable nearby assist route
3. if found:
   - choose `lower -> upper` or `upper -> lower`
   - local nav to the chosen end if reachable
   - follow assist route
   - local nav from the opposite end to exact destination

Checklist:

- [x] define first-pass assist-route selection heuristics
- [x] prefer destination-linked `sub_route` assists over macro graph attach
- [x] support bidirectional `lower` / `upper` assist traversal
- [x] add runtime support for chaining local nav + assist route + local nav
- [ ] log which assist route was chosen and why

### Phase 5: Add Destination Metadata for Local Awkwardness

Status: planned

Some destinations are known trouble spots and should advertise that fact.

Examples:

- Stormwind portal tower
- flight tower tops
- Booty Bay docks
- Thunder Bluff lift tops/bottoms
- Undercity elevator approaches

Suggested metadata:

- `local_nav_mode=direct`
- `local_nav_mode=assist_preferred`
- `local_nav_mode=assist_required`

Checklist:

- [ ] extend task-point or PoI metadata for local navigation hints
- [ ] allow named destinations to request assist behavior explicitly
- [ ] allow named destinations to match assist-route `lower_context_keys` / `upper_context_keys`
- [ ] keep fallback behavior data-driven, not hardcoded per city

### Phase 6: Narrow Macro Travel to Real Journey Cases

Status: planned

After local policy exists, macro travel should become more selective and more
trustworthy.

Checklist:

- [ ] only use route-network planning for longer journeys
- [ ] keep same-city errands out of the network
- [ ] keep quest-hub local work out of the network by default
- [ ] continue using taxi/transit for explicit long-distance travel

### Phase 7: Better Diagnostics for Failed Local Paths

Status: planned

We need better answers than "no path."

Desired debug additions:

- `PATHFIND_FARFROMPOLY_START`
- `PATHFIND_FARFROMPOLY_END`
- partial path dump for `PATHFIND_INCOMPLETE`
- last valid point reached
- nearest reachable point near destination

Checklist:

- [ ] include more `PathGenerator` flags in local-nav failure logs
- [ ] dump partial points when available
- [ ] add a helper probe for "destination nearby point is reachable"

### Phase 8: Transit Primitives for Vertical and Platform Travel

Status: planned

This is related, but should be built after the local-vs-macro split is cleaner.

Targets:

- Deeprun Tram
- Thunder Bluff lifts
- Undercity elevator
- future doors/platform-based local transit

Checklist:

- [ ] define lift/elevator transit step type
- [ ] use platform position/Z as the primary readiness signal
- [ ] optionally use door state where available

## Immediate Next Slice

The next concrete implementation slice should be:

1. explicit movement-policy split in `WorldBotCreatureAI`
2. local strict-nav as default for short same-zone movement
3. no automatic route-network attach for short local city/hub travel
4. first PoI connector fallback consumer for trusted local anchors
5. assist-route fallback after PoI connectors fail

That slice is the minimum needed to make bots behave naturally for:

- city errands
- quest-hub local work
- tower/dock/stair approaches

without weakening the existing anti-cheat path restrictions.

Current preferred local resolution order:

1. same-zone local nav attempt
2. if local nav fails, try the nearest trusted PoI connector anchor
3. if no connector chain is discoverable, try the second-nearest trusted anchor
4. if connector fallback still fails, lookup assist `sub_route` by destination
   or context key
5. choose `lower` or `upper` end based on whether the bot needs to go up or down
6. traverse assist route
7. retry strict local nav near the destination

## Success Criteria

We can consider this navigation style "real" when:

- a Stormwind mailbox-to-bank task uses local nav only
- a Stormwind tower/portal task uses local nav, then learned connectors, then a
  tiny assist route if needed, not the world network first
- a quest hub grind/gather step mostly uses local nav within the hub
- a zone transfer still uses macro travel/network/transit correctly
- path failures explain whether the issue is:
  - bad destination
  - navmesh attachment problem
  - incomplete legal path
  - missing assist route
