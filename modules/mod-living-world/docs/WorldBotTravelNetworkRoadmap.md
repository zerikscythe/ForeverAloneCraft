# World Bot Travel Network Roadmap

This document tracks the planned work to move LivingWorld travel from mostly
straight-line point-to-point movement toward an authored travel-network system
that bots can follow across zones and, later, across the world.

It is intentionally written as a **cross-agent working plan**:

- durable enough for handoff between agents
- explicit about dependencies and deliverables
- split into small slices that can be validated independently

---

## Problem statement

Current world-bot travel can already:

- execute authored task/session travel steps
- use some transit/taxi concepts
- recover from same-map stuck/no-progress cases with watchdog teleport recovery

But it still lacks a strong **macro-navigation layer**.

Today bots do **not** yet have a reliable authored understanding of:

- roads
- preferred route corridors
- mount-friendly travel lanes
- attach-to-nearest-route behavior
- following a route toward a distant task instead of cutting through open terrain

That means travel can still look too direct, too terrain-blind, and too unlike a
real player moving through the world.

---

## Design goal

Build a travel pipeline where a bot can:

1. resolve a destination or task area
2. attach to a nearby authored travel network
3. follow route segments in the correct direction
4. branch toward local task/gather/quest areas when appropriate
5. later combine that with taxis, portals, boats, zeppelins, and mount policy

This should become the **strategic movement layer**, separate from:

- combat doctrine
- hazard escape
- local tactical repositioning

---

## Non-goals for the first wave

The initial system should **not** try to solve all travel realism at once.

Not first-wave goals:

- perfect continent-scale pathfinding
- full navmesh-driven road extraction
- live player-visible map renderer in the game client
- custom artistic cartography pipeline
- perfect obstacle avoidance in every off-road situation
- automatic semantic understanding of every doodad/bridge/path in the world

The first target is an **authored route network** with a practical editor and a
runtime that can use it consistently.

Additional scope guardrail:

- do **not** rebuild functionality AzerothCore already provides adequately via
  vmaps/mmaps and local pathfinding

---

## Architecture overview

The intended stack is:

1. **Map asset extraction layer**
2. **Coordinate transform layer**
3. **Overlay authoring/editor layer**
4. **Route export / storage layer**
5. **Runtime route attachment + traversal layer**
6. **Travel policy / transit integration layer**

Execution principle:

- **AzerothCore pathfinding remains the local movement executor**
- **LivingWorld adds the missing strategic route-selection and authored-network
  layer above it**

---

## Core design decisions

### 1. Use extracted map art as an authoring surface

We want local image assets for maps/zones so routes can be drawn visually.

Requirements:

- canonical naming per map / zone
- stable IDs for export/import
- predictable file layout for tooling

### 2. Explicit world ↔ local coordinate conversion

The system needs a deterministic transform so:

- drawn editor-space points become real in-game world positions
- in-game positions can also be projected back onto the authoring map

This transform is foundational. Do this early and keep it explicit.

### 3. Author routes as data, not code

Travel corridors, roads, branches, and task-area paths should become authored
data, not hardcoded C++ logic.

### 3.2 Keep one authoritative travel source per zone

For a given zone, there should be one durable authored source of travel truth.

That source may later contain multiple travel network sets when a zone needs
more than one mode, for example:

- `ground`
- `flight`
- later `taxi`, `boat`, `portal`, or other special transit classes

But the important rule is:

- do **not** scatter one zone's travel truth across unrelated files or
  hardcoded logic
- keep one authoritative editor/source artifact per zone
- derive runtime/export artifacts from that source

Current implementation direction:

- the zone editor/source file is the authoring truth
- runtime `__routes.json` files are derived export artifacts
- transition-node markup in the editor now generates `map_XXX__connectors.json`
  as an editor-owned artifact
- the server should primarily treat route/connectors JSON as input and only
  perform terrain/Z enrichment plus validation at startup

### 3.3 Keep route infrastructure separate from activity intent

The authored route network should answer:

- how to get from A to B
- where to attach/detach
- how long a route-followed trip should take

It should **not** be limited to one activity family such as questing.

The same route infrastructure should be reusable by:

- quest-hub flows
- gathering loops
- patrol behaviors
- road ambush / roadside waiting behaviors
- later escort, defense, or transit-heavy activity families

This is an important boundary:

- activity/session composition decides **why** a bot is moving somewhere
- route planning decides **how** it gets there
- local movement still executes the next reachable point

### 3.4 Use real quest data as an authoring source, not a full offscreen quest engine

LivingWorld already has cached quest and chain knowledge available for higher
level tooling. That data should be used to build more believable session flows.

First-pass direction:

- use quest givers, breadcrumb chains, target hubs, and likely field areas as
  authored session-composition inputs
- do **not** require the abstract runtime to simulate the complete canonical
  WoW quest log, every reward, and every objective edge case

So the intended model is:

- real quest data informs where bots should plausibly go
- bot activity sessions turn that into believable "quest-inspired" movement and
  presence
- route planning then carries those sessions across the world

### 3.5 Add a derived quest-hub progression layer above broad zone questing

Broad `quest_auto` / `quest_zone` resolution is a useful first pass, but it is
still too coarse to make bots feel like they are following WoW's natural quest
flow.

Next direction:

- derive a compact **quest-hub graph** from the richer extracted quest cache
- represent hubs as:
  - quest giver / hub anchor
  - level/faction band
  - approximate quest count / stay duration
  - weighted follow-on branches to other hubs
- let session composition choose:
  - an initial quest hub
  - then a weighted follow-on hub after a timed quest block ends

Important boundary:

- the editor/tooling owns the rich quest extraction source
- runtime consumes a compact derived hub graph
- travel planning remains the hidden "magic" that figures out how to reach the
  chosen hub using roads, taxi, boat, zeppelin, portal, and ground travel

Related design note:

- see `WorldBotQuestHubProgressionDesign.md`

### 3.6 Add city reserve pools without breaking "bots have history"

Normal world bots should continue to:

- resume from their own last known place
- keep their own task/session continuity
- naturally drift around the world through travel and content

But major cities need a small amount of deliberate support so they do not feel
empty when a player approaches.

Direction:

- keep a dedicated `city_reserve` pool for cities like Stormwind and Orgrimmar
- let the population controller pull from those reserves when a hot city is
  underfilled
- bias those reserve bots toward city chores / city idles / short city walks
- release them back to the ledger after city heat drops and a linger window
  expires

Related design note:

- see `WorldBotCityReservePopulationDesign.md`

### 3.1 Reuse-first rule

Before introducing new travel/runtime code, prefer this order:

1. use existing AzerothCore pathfinding if it already solves the local movement
2. add authored strategic route selection when the missing problem is
   macro-navigation, not local obstacle avoidance
3. add micro-route overrides only for critical or unreliable local traversal

This keeps the feature focused on the true gap instead of duplicating existing
movement capabilities.

### 4. Keep runtime route following separate from combat movement

Travel network following is a **macro-navigation** concern.

Combat movement continues to own:

- hold / close / kite / retreat
- hazard escape
- local threat-aware steering

### 5. Prefer nearest-route attach over exact authored start points

Bots should be able to:

- start from arbitrary points in a zone
- find a reasonable nearby route segment/node
- join the network
- continue toward destination

That avoids brittle “must already be exactly on the path” behavior.

### 5.1 Branch choice is a graph-routing problem, not a steering guess

At runtime, a bot should not decide "left or right" at a branch by local
heading intuition alone.

Instead:

- authored paths and branch connections should be imported into a zone-local
  travel graph
- runtime should resolve an entry node, an exit node, and a graph path between
  them
- once that path is chosen, branch direction is already known before movement
  begins

This is a core design boundary:

- local movement executes the next step
- route graph search decides which branch leg is correct

### 5.2 Route ETA and progress should be distance-driven

The authored route graph is not just for prettier travel. It should become the
source of truth for **travel time** as well.

ETA should be computed from real yard distance, not from:

- raw node count
- straight-line point-to-point distance alone
- a generic "mounted yes/no" shortcut

For a route-followed trip, total travel distance should be modeled as:

- attach leg from current position to chosen route entry
- authored network leg from entry to exit across the route graph
- detach/final-approach leg from route exit to the exact task destination

Then runtime and abstract ETA can be derived from:

- total distance yards
- effective travel speed for the chosen travel capability tier

Important implication:

- adaptive point spacing is an export/runtime density concern only
- **distance** is the truth for ETA
- **node count** is not a stable timing measure once spacing varies by curve
  tightness

### 6. Travel must be interruptible and resumable

Bots will not traverse routes in a vacuum. While following a travel corridor,
they may:

- enter combat
- be attacked by mobs or players
- be displaced by fear/knockback/chase movement
- temporarily switch into hazard/combat doctrine

So route following must support:

- interruption
- suspension
- resume-from-nearest-valid-route-point/segment
- eventual fallback if the original route is no longer appropriate

The runtime should not assume that once a route is chosen it can be followed to
completion uninterrupted.

### 6.1 Respect the hot-zone materialization model

The travel/runtime design must fit the existing world-bot population model:

- zones around an interested real player become **hot**
- bots in those hot zones can materialize as real `Creature` actors
- offscreen/cold-zone bots continue as **abstract** runtime progress

Important implication:

- do **not** design surveillance, travel state, or observability around a
  "mixed zone" assumption where one zone is casually half-abstract and
  half-materialized for the same gameplay purpose
- instead treat materialization as a hot-zone handoff between:
  - abstract offscreen runtime progress
  - exact live in-world actor state

The abstract layer is already richer than a plain ledger tag:

- abstract bots can have an active session
- a current step index
- elapsed time in the current step
- and an interpolated offscreen position

So when a zone becomes hot, bots should materialize into the world already
mid-task/mid-travel rather than spawning from a blank zero-state.

### 7. Cross-zone travel should use explicit connector nodes

We should not treat neighboring zones as implicitly connected just because their
map art touches.

Instead, cross-zone travel should later use explicit authored connector concepts:

- `enter` nodes
- `exit` nodes
- directed or bidirectional route links
- optional transit handoff nodes

This gives us a stable way to connect:

- one zone route graph to another
- one sub-map/overlay to another
- one travel mode to another (road -> taxi, road -> boat, etc.)

Current implementation direction:

- connector seams are authored by marking transition anchors in neighboring zone
  route files
- the editor regenerates connector manifests from reciprocal transition-node
  markup on save
- server startup should no longer be the primary topology author; it should
  consume connectors, repair/bake `world_z` when needed, and validate
  continuity

### 8. Broad travel routes are not enough for vertical/local traversal

Some destinations are not solved by a broad zone corridor alone.

Examples:

- spiral staircases
- towers
- ramps
- bridges with elevation change
- caves
- tunnels
- multi-floor interiors
- building enter/exit routes

For those cases the system needs a **micro-navigation layer** that can express
"go up", "go down", "enter building", or "move through this interior path" as a
sequence of local authored route points/connectors.

The route system should therefore support both:

- **macro travel corridors** for world/zone movement
- **micro route segments** for precise local traversal through constrained
  geometry

### 9. Prefer native nav/pathfinding when it is reliable, but keep authored
fallbacks for critical vertical transit points

We should not assume every staircase, tower, portal room, cave path, or raised
platform requires manual micro-routing from day one.

AzerothCore movement plus extracted vmaps/mmaps may already handle some of these
cases adequately, especially when:

- the target point is valid and reachable
- the staircase/ramp is navmesh-covered correctly
- local collision and pathfinding are stable in live testing

So the travel system should use this rule:

- **prefer native pathfinding where it is already sufficient**
- **add authored micro-route fallback for critical or unreliable locations**

This matters most for high-value travel handoff locations such as:

- zeppelin tower access
- portal-room access
- elevator/ramp/tower interiors
- narrow or multi-floor city structures

The roadmap should therefore treat these as **validation hotspots** rather than
assuming universal manual authoring or universal navmesh success.

### 10. The new code should solve routing, not replace movement execution

The intent of this feature is:

- **not** to replace AzerothCore's local mover/pathfinder
- **not** to rebuild a second generic navmesh system in module code
- **but** to provide:
  - route choice
  - route attachment
  - route progression state
  - route interruption/resume
  - route authoring surfaces

In other words:

- AzerothCore should answer: "how do I walk to this next reachable point?"
- LivingWorld should answer: "which sequence of points/corridors should I be
  following across the world?"

---

---

---

## Planned workstreams

### Workstream A — Map extraction and canonical map catalog

Goal:

- produce a clean local asset set for maps/zones that the tooling can consume

Deliverables:

- extracted zone/world map image assets where client assets allow it
- canonical naming convention for map images
- manifest file linking:
  - game map id
  - zone id where relevant
  - display name
  - source asset path
  - output image path
- validation notes for maps that need manual fixes or are unavailable

Open questions:

- which client asset format/source is easiest to extract from consistently?
- whether zone maps, continent maps, or both should be first-class assets
- whether a single manifest can cover world maps and sub-zone maps together

Recommended first output:

- manifest + extracted image set for a small pilot group of zones before trying
  to solve the whole world

---

### Workstream B — Coordinate transform system

Goal:

- define a robust conversion between local map image coordinates and AzerothCore
  world coordinates

Deliverables:

- transform model per map/zone asset
- import/export format for transform metadata
- conversion helpers for:
  - image-local -> world position
  - world position -> image-local
- validation checklist using known anchor points

Required properties:

- deterministic
- versionable
- easy to debug visually and numerically

Validation approach:

- choose several known points already present in LivingWorld task points / zone
  anchors
- project them onto the image and confirm alignment
- round-trip test local -> world -> local within acceptable error tolerance

---

### Workstream C — Python overlay authoring tool

Goal:

- extend or add a Python GUI tool that lets us draw route and area overlays on
  extracted map images

Likely tool home:

- use the dedicated `tools/lw-zone-editor/` app/tooling home for map authoring,
  route editing, and route save/load workflows

Core features:

- load map image
- load transform metadata
- pan / zoom
- display current mouse world/local coordinates
- draw/edit:
  - polylines
  - colored route classes
  - polygons/areas
  - anchor markers
- snap/select/move/delete control points
- save/export authored data

Suggested overlay categories:

- primary road corridor
- secondary path corridor
- mount-preferred route
- local town/hub circulation route
- gather loop
- questing sub-zone area
- danger/avoid region (later)

Important editor rule:

- keep the first version focused on route/shape authoring, not broad world
  simulation management

---

### Workstream D — Route data model and export format

Goal:

- define how drawn routes become durable authored data

Options:

- JSON export files first, DB import later
- direct DB-backed authoring
- hybrid: JSON as edit artifact + importer into DB tables

Recommended first approach:

- **JSON authoring artifact + importer**

Why:

- easier iteration/debugging
- easier review/diff in git
- simpler editor development initially

Recommended source/export split inside that JSON-first approach:

- one **editor/source artifact per zone** as the authoritative drawn-path file
- one **runtime/export artifact per zone** derived from the editor source
- editor/source preserves anchors, bezier handles, branch links, and draft vs
  finalized state
- runtime/export preserves finalized traversal-ready route data such as sampled
  5-yard movement points

For future expansion, the zone source should be able to hold more than one
network set when needed, e.g. `ground` and `flight`, without creating multiple
conflicting sources of truth for the zone.

Needed route concepts:

- route id / key
- map id / zone id scope
- zone-level source identity
- optional travel mode / route-set identity inside the zone source
- route class / color / semantic type
- ordered control points
- optional directed edges / branch metadata
- tags such as:
  - road
  - city
  - hub
  - mount_ok
  - faction_restricted
  - travel_priority

Later DB concepts may include:

- route table
- route point table
- route segment metadata table
- zone-to-route attachment hints

---

### Workstream E — Runtime nearest-route attach and traversal

Goal:

- let a bot use the authored route network at runtime

First-pass runtime behavior:

1. bot has current world position
2. destination/task has target position/anchor
3. resolve nearest suitable route node/segment to current position as the
   network entry point
4. resolve a suitable destination-side route node/segment as the preferred exit
   point
5. solve a zone-local graph path from entry to exit
6. traverse the sampled route points in the order implied by that graph path
7. detach when close enough to destination-local objective area and begin local
   final approach

Required runtime features:

- nearest-node or nearest-segment lookup
- entry-node and exit-node selection
- branch-aware graph construction from authored paths + explicit branch links
- same-zone graph search for route selection
- route-distance accumulation across chosen graph legs
- ETA calculation from route distance plus travel capability
- follow exported points in order
- leave-network / final-approach decision rules
- reattach if displaced or interrupted
- suspend route travel when combat/hazard runtime takes control
- resume by reattaching to the nearest valid route point/segment after survival

Reuse-first runtime rule:

- runtime route traversal should generally hand the next local target point to
  the existing AzerothCore movement/pathfinding layer
- only add bespoke travel logic for route-state management and authored-network
  semantics

Conflict-resolution expectations:

- if a bot is attacked while following a route, combat doctrine becomes
  authoritative until combat resolves
- if the bot survives, it should resume route travel rather than forgetting its
  original strategic destination
- if the bot ends too far from the current segment, it should re-snap to the
  nearest valid segment/node instead of trying to continue from stale route
  indices blindly

Detach expectations:

- the route network should own the **long leg** of travel
- the local mover should own the **final approach** to the exact task/objective
- bots should be able to break away from the road when the destination is close
  enough or when staying on-road is no longer cheaper/useful
- bots should also be able to reattach after combat, displacement, or failed
  local approach

First-pass simplifications allowed:

- same-map/zone only first
- same-zone graph search is worth doing early because branch correctness depends
  on it
- no dynamic reroute around combat initially beyond existing combat interruption

Recommended first resume model:

- persist a small route-travel runtime state:
  - active zone route source / route-set identity
  - chosen entry node/segment
  - chosen exit node/segment
  - current segment or point index
  - final destination anchor/point
- on interruption end, recompute nearest valid attach point against the same
  route first before abandoning the route entirely

Recommended first travel-progress model:

- persist route-aware progress for abstract and materialized travel:
  - route-set / route-plan identity
  - travel capability tier or chosen travel mode
  - current route edge/segment
  - distance traveled along route
  - total planned route distance
  - destination anchor/point
- when a cold/offscreen bot later materializes into a hot zone, use this stored
  progress to spawn it at the correct approximate route position instead of a
  generic zone anchor or a fresh zero-state start

#### Runtime integration contract with world-bot movement

The runtime handoff should look roughly like this:

1. session/task travel resolves a world destination
2. travel-network runtime asks whether a zone route source exists for the
   current zone / travel mode
3. if yes, it chooses:
   - entry attachment
   - exit attachment
   - ordered graph path
4. route runtime feeds compact next-step world points into AzerothCore local
   movement/pathfinding
5. once the bot reaches the planned leave-network point, normal local movement
   finishes the destination approach
6. if no route source exists, fallback remains the existing direct/local travel
   behavior

Materialization handoff expectation:

- route/session progress should survive abstract -> materialized and
  materialized -> abstract transitions cleanly
- if a bot was already traveling offscreen, the hot-zone spawn should continue
  that travel from the correct step/progress state instead of restarting
  travel/session logic from scratch
- first production goal is not perfect cinematic continuity, but reliable
  mid-route continuity that preserves believable elapsed travel time and
  approximate position

#### Travel capability and speed policy

ETA and abstract travel timing should respect a real mobility policy instead of
one flat mounted-speed assumption.

At minimum, the policy layer should distinguish:

- on foot
- lower-tier ground mount
- faster ground mount
- lower-tier flying mount
- faster flying mount
- taxi / transit speed classes

Route planning then asks two questions:

1. what kind of travel network or corridor is this leg using?
2. what travel capability tier is this bot allowed to use?

That allows:

- slower low-level bots to take longer on the same route than higher-capability
  bots
- abstract ETA to stay believable across level/capability differences
- future reuse for taxi, flight-route, and mixed transit planning

---

### Workstream E.1 — Route point spacing and movement granularity

Goal:

- ensure exported route lines are transformed into bot-usable intermediate move
  targets rather than excessively sparse control points

Design direction:

- drawn polylines should be resampled into a traversal series of shorter points
- adaptive export should tighten on curves and loosen on straights
- current working profile:
  - minimum spacing: **5 yards**
  - base spacing: **25 yards**
  - maximum spacing: **50 yards**

Why:

- long point-to-point jumps can cut corners or drift off intended corridors
- shorter segments make reattach/resume behavior more stable
- travel traces become easier to debug when movement advances through compact
  route steps

Open implementation choices:

- resample at export time
- resample at import time
- or preserve artist control points plus generate a derived runtime point list

Recommended first choice:

- keep authored control points as the editable source
- generate a derived runtime traversal point list during export/import

Current rationale:

- this keeps the route graph far smaller than the original 5-yard uniform
  sampling experiments
- long straight roads no longer waste hundreds of intermediate checkpoints
- tight bends and branch areas still preserve enough local shape for believable
  following and reattach behavior

---

### Workstream E.2 — Cross-zone connector graph

Goal:

- connect zone-local route networks into a broader world travel graph

Required concepts:

- zone-local route graph
- zone `exit` nodes
- neighboring zone `enter` nodes
- explicit link metadata between them
- optional travel-mode metadata on the link

Example connector classes:

- road continuation
- gate/pass/tunnel
- bridge crossing
- flight/taxi handoff
- boat/zeppelin handoff
- portal handoff

Important rule:

- connectors should be authored explicitly, not guessed from map borders alone

Current implementation notes:

- explicit connectors exist and are now the preferred production seam source
- runtime keeps a tolerant re-anchor fallback on zone transition because border
  tips and seam spacing can still be imperfect
- editor transition ghosts and transition-node markup exist specifically to
  tighten seam placement during authoring
- startup/server Z-bake should patch seam heights and route-point heights, but
  should not be relied on to invent missing topology

---

### Workstream E.3 — Micro-navigation and vertical connectors

Goal:

- represent constrained local traversal that requires specific ordered points,
  especially when elevation change matters

Examples:

- climb a spiral staircase to a tower top
- descend into or out of a cave
- traverse a ramp or switchback path
- enter a building, move to an upper floor, then exit again

Design direction:

- treat these as authored **micro route chains** or **vertical connector
  segments**
- they should be attachable from the broader route graph
- they should preserve ordered traversal rather than relying on the runtime to
  infer "upward" intent from only a destination point

Key idea:

- "go up" is not a special abstract command by itself
- instead it is communicated through a local ordered route sequence whose points
  climb the staircase/ramp/interior path correctly

Recommended representation:

- micro route id/key
- segment type such as:
  - `stairs_up`
  - `stairs_down`
  - `ramp`
  - `interior_corridor`
  - `doorway_passage`
- ordered traversal points with full world coordinates including meaningful `z`
- optional enter/exit attachment nodes linking micro route <-> macro route

Important runtime rule:

- once attached to a micro-navigation segment, the runtime should follow the
  authored local sequence closely rather than trying to aggressively optimize or
  skip intermediate points

Validation rule:

- before authoring a micro-route for a location, verify whether native
  pathfinding already handles the approach/traversal reliably enough
- reserve authored micro-routes first for locations where native traversal is
  inconsistent, brittle, or mission-critical

High-priority candidate validation sites:

- Orgrimmar zeppelin tower approach
- Stormwind portal-room access
- other known raised transit/service hubs discovered during testing

---

### Workstream F — Travel policy layer

Goal:

- decide how bots should use available travel options, not just where to go

Future policies:

- run vs mount
- prefer road vs shortcut
- faction-safe vs risky route preference
- handoff to taxi / portal / boat / zeppelin routes
- city-entry / city-exit anchor behavior
- role/personality specific travel style

Near-term concrete direction:

- foot and ground-mount tiers already affect ETA and visible materialized
  travel
- taxi should be the next real transit policy layer
- first-pass taxi knowledge should be derived from **explored zones**
- bots start with **zero explored zones**
- entering a zone by any meaningful gameplay path (abstract travel,
  materialized travel, questing, gathering, patrol, etc.) should unlock that
  zone
- unlocking a zone unlocks **all taxi points in that zone**
- ground/mount travel remains universally allowed; taxi becomes an earned
  convenience layer

This should come **after** the basic route network exists.

---

### Workstream F.2 - Explored-zone travel memory and taxi eligibility

Goal:

- give bots persistent world-memory that can unlock transit options over time

Core rule:

- if a bot has ever entered a zone, that zone is marked explored
- if a zone is explored, all taxi nodes in that zone are considered known

First-pass behavior:

- pregenerated bots begin with no explored zones
- early world traffic should therefore skew heavily toward road/ground travel
- over time, bots naturally unlock more efficient taxi options by moving
  through the world

Recommended persistence model:

- store explored zones in a normalized child table, not a comma-separated blob
- derive known taxi nodes from explored zones rather than storing taxi-node
  knowledge separately

Planned supporting pieces:

- explored-zone repository/schema
- zone-entry hooks for both materialized and abstract travel
- taxi node -> zone mapping
- planner comparison between:
  - pure ground
  - partial taxi + ground
  - full taxi

---

### Workstream F.1 - Activity overlays and believable destination composition

Goal:

- compose better bot travel intent on top of the shared route infrastructure

Key idea:

- the route graph is shared infrastructure
- different activity families choose different destinations, pauses, and branch
  behavior on top of it

First target activity families:

- quest-hub flows
- gathering loops
- road patrols
- road ambush / roadside waiting behaviors

#### Quest-derived hub flow direction

Use cached quest-chain and hub knowledge to compose more believable
"quest-inspired" sessions.

Examples:

- arrive at Goldshire
- linger near quest giver cluster
- travel to nearby field pocket
- simulate kill/gather/idle presence there
- return to hub
- branch to a believable next hub or next zone

Important rule:

- use real quest geography and breadcrumbs as the authored source
- do not require the offscreen runtime to become a full canonical quest engine

Preferred session behavior:

- hub-local chore budget such as 10-30 minutes
- weighted branch choice among believable next hubs/areas
- recent-history dampening so the exact same branch is not always chosen
- if the branch graph is exhausted or no valid next branch is found, allow an
  early safe logout so the next spawn/session can get a fresh chore list

#### PvP / patrol overlay direction

Not every bot should appear to be questing.

Some higher-level or personality-biased bots should instead receive
territory-driven behaviors such as:

- patrol this road corridor
- wait in this roadside stakeout pocket
- move between known traffic chokepoints
- watch for weaker travelers or opportunistic fights

Typical archetype examples:

- rogue: ambush pocket, hidden roadside wait, relocate between chokepoints
- warrior: visible road patrol, bounce between likely victim routes

These overlays still depend on the same route graph:

- to reach the patrol/ambush area
- to move between related hotspots
- to estimate believable abstract travel and reposition time

Future authored metadata may therefore include:

- quest hubs
- field activity pockets
- patrol spans
- ambush/stakeout points
- connector chokepoints

---

### Workstream G - World Bot Surveillance / observability

Goal:

- provide a runtime-facing world view that shows where bots actually are, where
  abstract bots are currently projected to be, and what they are doing

Purpose:

- debug world-bot spread and migration over time
- verify that hot-zone materialization preserves believable mid-task state
- inspect travel-network attach/follow/detach behavior once route runtime lands

First-pass view concepts:

- faction-colored bot dots
- exact live positions for materialized bots
- projected/interpolated offscreen positions for abstract bots when abstract
  runtime state is available
- generalized zone-presence fallback only when no richer offscreen runtime
  position exists
- click/select bot for task/session detail

Important semantic rule:

- do **not** present generalized abstract zone placement as if it were a real
  exact world coordinate
- distinguish:
  - exact live position
  - simulated offscreen/projected position
  - generalized zone-presence fallback

Potential detail surface:

- materialized vs abstracted
- current map/zone
- current activity / session source
- current step index and progress
- mounted / taxi / in-combat state for materialized bots
- active task summary / task list
- later route-runtime state such as attached route source, current node, and
  planned exit point

---

## Suggested slice plan

### Slice 1 — Canonical map asset pilot

Goal:

- prove we can extract a small initial set of map images and catalog them cleanly

Deliverables:

- extraction script or documented extraction pipeline
- canonical output folder structure
- manifest for a pilot set of maps/zones
- validation notes on naming and coverage gaps

Definition of done:

- at least a small pilot set of usable authoring images exists locally
- naming and IDs are stable enough for later tooling

Status: `Complete`

### Slice 2 — Coordinate transform v1

Goal:

- make local image points convertible to world coordinates and back

Deliverables:

- transform metadata schema
- conversion helper script/module
- validation against known task points/anchors

Definition of done:

- a pilot zone can round-trip known points with acceptable error

Status: `Complete`

### Slice 3 — Overlay editor v1

Goal:

- load a map and draw/edit polylines over it

Deliverables:

- Python GUI canvas
- zoom/pan
- point/line editing
- route save/load for the current zone
- save/export route overlays

Definition of done:

- a human can author a simple route corridor and save it durably
- if a saved route source already exists for the loaded zone, edit mode can
  load it back without manual file surgery
- editor design remains compatible with later micro-route fallback authoring

Status: `Complete`

### Slice 4 — Route export/import schema

Goal:

- move overlays into a stable machine-readable format

Deliverables:

- JSON schema and/or DB importer
- route validation tool
- initial sample route files
- route resampling rules for traversal-point generation
- explicit editor/source artifact and derived runtime/export artifact model
- connector representation for future cross-zone links
- micro-route / vertical-segment representation

Definition of done:

- editor/source route data can be saved and reloaded without drift
- exported route data can be consumed without manual editing

Status: `In Progress`

Current implementation note:

- zone editor save/load now supports a zone-level editor/source artifact and a
  derived runtime/export artifact
- sampled traversal points are generated at export time
- deployed worldservers should consume a copied server-local route bundle such
  as `data/worldbot_routes`, not assume the editor workspace exists beside the
  runtime
- DB importer, dedicated validation tooling, and broader schema finalization are
  still pending

### Slice 5 — Runtime nearest-route attach

Goal:

- let bots join the authored route network from arbitrary positions

Deliverables:

- nearest-route lookup helper
- route traversal service
- same-zone graph path selection across branch points
- route-distance and ETA calculator
- attach/detach behavior for task travel
- interruption/suspend/resume state model
- first route-progress persistence shell for abstract/materialized handoff

Definition of done:

- a pilot bot can join a route and follow it toward a destination instead of
  moving only in a direct line
- when a branch exists, the bot chooses the correct leg via route-graph
  selection instead of local guesswork
- route-followed travel exposes a true distance/ETA instead of only
  straight-line timing
- if interrupted by combat and the bot survives, it can resume route travel
  toward the original destination
- native pathfinding remains the first attempt for local final approach unless a
  specific authored micro-route override is present
- implementation does not duplicate generic local obstacle pathfinding already
  provided by AzerothCore

Status: `Completed`

Current implementation note:

- route-backed travel-plan resolution now exists for scheduled travel
- same-zone graph traversal and attach/detach distance/ETA calculation are in
- planner/debug harness commands exist for route-plan inspection and capability
  comparison
- remaining hardening moved into later slices:
  - richer travel-mode choice
  - explored-zone taxi eligibility
  - broader multi-zone graph routing

### Slice 6 — Route-followed session travel integration

Goal:

- integrate route following into the existing world-bot session/travel pipeline

Deliverables:

- route-aware travel step execution
- fallback rules when no route exists
- observability/traces for route attach/follow/detach
- travel-resume traces after combat interruption
- support for explicit micro-route traversal where local geometry requires it
- abstract progress fields that can resume travel mid-route on hot-zone
  materialization

Definition of done:

- authored session travel can prefer route network traversal where coverage exists
- a bot that was already partway through an abstract route can materialize into
  a hot zone at an approximate in-progress route position instead of a blank
  fresh start

Status: `In Progress`

### Slice 6.1 - Quest-derived activity composition

Goal:

- upgrade vague "quest here" behavior into believable hub/field/branch flows
  using cached quest and breadcrumb knowledge

Deliverables:

- quest-hub / next-hub graph derived from cached quest-chain data
- session-composer support for:
  - hub arrival
  - local field chore loops
  - return-to-hub behavior
  - outbound hub/zone branch choice
- early safe logout fallback when the branch graph is exhausted or no valid next
  chore branch can be found
- recent-history dampening so bots do not always pick the exact same next hub

Definition of done:

- low-level questing bots can appear to move through believable real quest-hub
  geography without requiring full offscreen quest-log simulation
- outbound travel targets for those bots come from authored quest-derived hubs
  and can be handed directly to the route planner

Status: `Planned`

### Slice 7 — Transit and mount policy integration

Goal:

- combine route following with mount/taxi/portal/transit decisions

Deliverables:

- policy resolution layer
- travel capability tiers and speed table
- mount-preferred route classes
- route handoff to transit nodes

Definition of done:

- bots can use authored route corridors as part of a broader travel policy
- ETA differs correctly across foot, ground-mount, flight, and transit
  capability tiers where applicable

Status: `In Progress`

Current implementation note:

- ground mobility tiers and visible materialized mount/form behavior are in
- taxi still needs the explored-zone memory and planner-choice layer

### Slice 7.1 - Road patrol and ambush overlays

Goal:

- support non-quest territorial behaviors that still rely on the shared route
  network

Deliverables:

- authored patrol spans, chokepoints, and roadside stakeout points
- composer support for:
  - road patrol
  - roadside wait/ambush
  - hotspot rotation
- simple archetype weighting such as:
  - rogue -> ambush/stakeout bias
  - warrior -> visible patrol bias

Definition of done:

- a higher-level or PvP-minded bot can be assigned to patrol or lurk near road
  traffic rather than always looking like a questing character
- those sessions still reuse route-based travel, ETA, and abstract progress

Status: `In Progress`

Current implementation note:

- explicit connectors exist and are usable
- editor transition nodes now generate connector manifests on save
- runtime seam handling now relies on explicit connectors first plus
  re-anchor/grounding tolerance rather than pure border guessing

### Slice 8 — Cross-zone route connectors

Goal:

- allow route-followed travel to continue cleanly from one zone graph into
  another through explicit connector nodes

Deliverables:

- connector node schema
- link/export representation
- runtime handoff across zone graphs

Definition of done:

- a pilot cross-zone route can be followed via explicit enter/exit linkage

Status: `Planned`

### Slice 9 — Micro-route and vertical traversal support

Goal:

- let bots follow authored local route chains through constrained vertical or
  interior geometry that broad corridor routing cannot describe well enough

Deliverables:

- micro-route segment schema
- editor support for authoring local stair/ramp/interior chains
- runtime traversal rules that respect ordered local points
- explicit fallback policy: use micro-route when a location is marked critical
  or native path traversal is unreliable

Definition of done:

- a pilot case such as a tower or multi-floor interior can be traversed by a
  bot using authored local route points instead of only a broad destination

Status: `Planned`

### Slice 10 - World Bot Surveillance view

Goal:

- add a dedicated observability view for world-bot population state, projected
  abstract activity, and later route-runtime debugging

Deliverables:

- surveillance data feed or poll surface
- materialized bot rendering with exact live positions
- abstract-runtime bot rendering with projected/interpolated offscreen
  positions
- generalized zone-presence fallback for bots that only have ledger-level
  presence information
- click/select detail panel with task/session summary
- basic filters for faction and materialized vs abstract state

Definition of done:

- a developer can inspect believable bot spread and migration over time without
  confusing generalized abstract presence with exact live world coordinates
- a hot-zone materialization can be observed as a continuation of in-progress
  activity rather than a fresh zero-state spawn

Status: `Planned`

---

## Data model ideas

These are not final schema commitments, but they are the main concepts we likely
need.

### Map asset manifest

- `map_id`
- `zone_id` nullable
- `asset_key`
- `display_name`
- `image_path`
- `source_kind`
- `pixel_width`
- `pixel_height`

### Transform metadata

- `asset_key`
- `world_min_x`
- `world_max_x`
- `world_min_y`
- `world_max_y`
- optional axis inversion flags
- optional anchor calibration points

### Route overlay entity

- `route_key`
- `asset_key`
- `zone_route_source_key` or equivalent zone-level owner identity
- `travel_mode` or route-set key, defaulting to ground/primary for first wave
- `route_kind`
- `tags`
- `color`
- `faction_mask` optional
- `min_level` / `max_level` optional
- `mount_allowed`
- `priority`

### Activity overlay metadata

- `zone_id`
- `activity_overlay_key`
- `activity_family`
- `level_band`
- `faction_mask`
- optional archetype/personality tags
- references to one or more of:
  - `quest_hub_key`
  - `field_activity_key`
  - `patrol_span_key`
  - `stakeout_point_key`
  - `connector_chokepoint_key`

### Quest-hub metadata

- `quest_hub_key`
- `zone_id`
- `display_name`
- `world_x`
- `world_y`
- `world_z`
- `level_min`
- `level_max`
- candidate outbound hub/zone branches
- nearby field pockets / chore areas

### Patrol / ambush metadata

- `patrol_span_key`
- `stakeout_point_key`
- `zone_id`
- `world_x`
- `world_y`
- `world_z`
- `corridor_route_key` optional
- `traffic_priority`
- `recommended_level_band`
- optional class/archetype bias tags

### Runtime route traversal points

- `route_key`
- `derived_point_index`
- `distance_from_start_yards`
- `world_x`
- `world_y`
- `world_z` optional
- `segment_kind` optional

### Zone route source artifact

- `zone_id`
- `zone_name`
- `map_id`
- `world_map_area_id` optional
- `travel_mode` or later `route_sets`
- authored path list with anchors/handles/connections
- optional transition-node metadata for seam generation
- derived runtime export metadata

### Bot explored-zone memory

- `bot_identity_id`
- `zone_id`
- `first_seen_at`
- `last_seen_at`

Recommended rule:

- the first meaningful entry into a zone unlocks it forever for travel memory
- taxi-node knowledge should be derived from this explored-zone set

### Bot guild identity (parallel social ledger work)

- `guild_id` on bot identity
- guild table for shared guild definitions
- later relationship tables for:
  - `neutral`
  - `rival`
  - `at_war`

### Surveillance snapshot model

- `bot_id`
- `name`
- `faction`
- `presence_mode`:
  - `materialized`
  - `abstract_runtime`
  - `abstract_generalized`
- `position_confidence`:
  - `exact_live`
  - `projected_offscreen`
  - `generalized_zone_presence`
- `map_id`
- `zone_id`
- `world_x`
- `world_y`
- `world_z`
- `activity`
- `session_source_kind`
- `session_source_key`
- `current_step_index`
- `step_elapsed_ms`
- optional route-travel fields:
  - `route_key`
  - `travel_mode`
  - `mobility_tier`
  - `distance_traveled_yards`
  - `distance_remaining_yards`
  - `eta_seconds`
- optional materialized-only flags such as `mounted`, `taxi`, and `in_combat`

### Connector metadata

- `connector_key`
- `from_asset_key`
- `from_route_key`
- `from_point_index` or node key
- `to_asset_key`
- `to_route_key`
- `to_point_index` or node key
- `connector_kind`
- `travel_mode`
- `generated_from_transition_nodes`
- `z_baked`

### Micro-route metadata

- `micro_route_key`
- `asset_key`
- `segment_kind`
- `entry_node_key`
- `exit_node_key`
- `ordered_points`
- optional `floor_from` / `floor_to`

### Route points

- `route_key`
- `point_index`
- `local_x`
- `local_y`
- `world_x`
- `world_y`
- `world_z`

Implementation note:

- route authoring is still effectively 2D-first, but runtime exports and
  connector manifests should be terrain-enriched through server-side Z-bake
  passes

### Area overlays (later)

- questing polygons
- gather regions
- danger regions
- hub regions

---

## Validation strategy

### Map extraction validation

- image exists
- naming matches manifest
- correct map/zone association

### Transform validation

- known anchors land in correct places visually
- round-trip error is acceptable

### Editor validation

- route saved and reloaded without drift
- line editing stable across sessions
- transition-node markup survives save/reload
- editor-generated connector manifests update on save and preserve unrelated
  manual connectors

### Runtime validation

- bot attaches to nearest route from arbitrary starting point
- bot can attach to a route entry near a city edge even when city interiors are
  not densely noded
- bot follows points in correct order
- bot chooses the correct branch leg for a destination instead of taking a dead
  leg or the wrong fork
- bot detaches near destination and continues local objective travel
- bot can leave the road at a sensible point and finish the last stretch with
  local movement
- fallback behavior remains sane where no route exists
- if attacked mid-route and survives, bot resumes route travel cleanly
- cross-zone connectors hand off to the intended next route graph
- zone-transition re-anchor fallback behaves sensibly when seam geometry is
  imperfect
- startup/server Z-bake updates missing route and connector heights without
  rewriting already baked files unnecessarily
- micro-route traversal preserves the intended local up/down/interior path
- critical vertical transit hubs are tested to determine whether native
  pathfinding is sufficient or needs an authored micro-route override
- when a cold zone becomes hot, bots materialize into believable in-progress
  activity rather than blank newly-started state
- explored-zone unlocks occur for both abstract and materialized zone entry
- taxi eligibility reflects explored-zone knowledge rather than global omniscience

### Surveillance / observability validation

- surveillance distinguishes materialized live position from abstract projected
  position and from generalized zone-presence fallback
- abstract runtime bots can be inspected with current session/step progress
- faction spread and migration over time are visible without pretending abstract
  fallback markers are exact world coordinates

---

## Recommended first implementation slice

If work begins immediately, the best first slice is:

## **Slice 1 — Canonical map asset pilot**

Why this should be first:

- every later step depends on having stable map assets
- coordinate transforms are meaningless without the asset surface
- the editor cannot exist until the asset/naming problem is solved

Concrete first deliverable:

- a script or repeatable documented workflow that extracts a pilot set of map
  images
- a manifest file with canonical names/IDs
- a short validation note describing what extracted cleanly and what still needs
  manual handling

Good pilot candidates:

- one capital/hub area
- one leveling zone
- one travel-heavy zone with visible roads

First implementation rule:

- choose pilot zones where we can quickly compare:
  - authored broad route guidance
  - existing AzerothCore local path execution
  - any need for later micro-route overrides

---

## Cross-agent continuity notes

Future agents should preserve these boundaries:

- do **not** blend combat doctrine and travel-network logic together
- do **not** skip the coordinate transform layer
- do **not** hardcode authored routes directly into C++ if a data path is
  feasible
- prefer reusable authoring surfaces over one-off zone-specific hacks
- do **not** ignore interruption/resume semantics when designing runtime route
  traversal
- do **not** rely on implicit zone-border adjacency where explicit connector
  nodes are more robust
- do **not** assume a single broad route line is sufficient for vertical or
  interior local traversal; use authored micro-route segments where needed
- do **not** assume every stair/tower/interior requires manual authoring first;
  validate native pathfinding and add micro-routes only where reliability or
  importance justifies it
- do **not** write replacement local pathfinding logic unless AzerothCore's
  existing movement/pathfinding has a proven gap that cannot be solved more
  cheaply through authored routing or micro-route fallback

Preferred implementation order remains:

1. map extraction
2. transform calibration
3. overlay editor
4. route export/import
5. route-plan resolution and runtime attach/follow
6. route-followed session integration
7. explored-zone memory and taxi eligibility
8. transit/mount policy
9. quest-derived and patrol/ambush activity composition

---

## Status summary

- overall system: `In Progress`
- completed live capability groups:
  - route-followed session travel
  - explored-zone memory and taxi eligibility
  - authored connector / seam support
  - dynamic taxi planning and execution
  - first physical cross-map boat seam
  - first physical cross-map zeppelin seam
  - quest-hub export tooling
  - quest resume memory
- in-progress capability groups:
  - city reserve population behavior
  - quest-hub runtime integration
  - local task-area behavior after hub arrival
- current active implementation focus:
  - `city reserve population scaffolding -> linger/release behavior`
- next recommended engineering slice:
  - finish city reserve linger/cooldown behavior
  - validate Stormwind / Orgrimmar reserve fill
  - keep reserve bots biased toward believable city errands
- next major realism slice after that:
  - server-side quest-hub runtime loading
  - hub-driven `quest_auto`
  - weighted hub continuation
  - local task-area loops

Related status note:

- see `WorldBotSystemAssessment.md`
