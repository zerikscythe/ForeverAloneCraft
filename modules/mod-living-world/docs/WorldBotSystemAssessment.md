# World Bot System Assessment

This note is a plain-language status snapshot of the LivingWorld bot system as
it exists today.

It is meant to answer four questions:

1. what is already real
2. what is partial
3. what is still missing
4. what we should do next

---

## Overall status

The project is no longer in "toy prototype" territory.

The system already has a real foundation for:

- persistent bot identity
- world-session continuity
- route-aware travel
- taxi travel
- first real boat and zeppelin seams
- quest-inspired destination selection
- quest resume memory

The biggest remaining gap is not "can bots go places?"

The biggest remaining gap is:

- making bots **look busy and believable after arrival**
- especially for questing, gathering, city life, and combat behavior

---

## Current capability snapshot

### 1. Identity / generation / persistence

Status: `Mostly live`

What exists:

- persistent identity ledger in `living_world_bot_identity`
- named bots with:
  - race
  - class
  - spec
  - faction
  - personality
  - home city/zone anchors
- session counting and last-seen tracking
- online-time progression
- level progression over time
- successor/retirement concepts already in the broader identity flow

What is still thin:

- PvP/world personality should probably vary more from session to session
- social identity is incomplete:
  - no guild system yet
  - no rival/war state yet

### 2. Gear / talents / preparation

Status: `Live, but still maturing`

What exists:

- virtual/assigned loadout resolution
- spec-aware preparation flow
- passive spell application
- player-like stat baseline work
- gear refresh over time

What is still incomplete:

- many class/spec combat behaviors remain shallow
- visible/behavioral mobility prep is still in flux
- mount-preparation expectations are not fully reconciled with tests yet

### 3. Travel / route network

Status: `Strong foundation, partial content coverage`

What exists:

- editor-authored route files
- runtime route graph resolution
- attach-to-route behavior
- branch-aware local/cross-zone planning
- connector artifacts between zones
- explored-zone memory
- taxi knowledge derived from explored zones
- dynamic taxi vs ground comparison
- abstract and materialized travel integration

What is still incomplete:

- not every leveling zone has route coverage
- some route seams still depend on content authoring quality
- local micro-routes for interiors/stairs/portal-room access are not fully
  authored

### 4. Special transit

Status: `Partially live`

What exists:

- dynamic taxi travel
- authored generic transit leg model
- physical cross-map boat support proven for:
  - Ratchet <-> Booty Bay
- physical cross-map zeppelin support proven for:
  - Orgrimmar <-> Tirisfal
- hot-destination rematerialization after cross-map boat/zeppelin seams

What is still incomplete:

- not every boat/zeppelin is scripted
- portal-room behavior is not yet validated end to end
- broader transport coverage still needs content + harness passes

### 5. Questing

Status: `Promising, but still transitional`

What exists:

- broad zone quest task resolution
- quest-hub export pipeline from extracted quest data
- compact per-zone quest-hub JSON files
- filtering out event/daily/weekly/monthly quest noise
- quest resume memory in the ledger/session composer

What is still incomplete:

- runtime does not yet fully consume the exported quest-hub graph
- bots do not yet run rich local "hub task area" loops after arrival
- continuation is not yet fully driven by hub-to-hub branch logic

### 6. Gathering / skinning / fake work loops

Status: `Mostly missing`

What exists:

- resource/node metadata work
- gathered item display work in the viewer/editor
- exporter-side task-area shaping from quest/objective data

What is still incomplete:

- no full "go here and pretend to skin" loop
- no full herb/ore local behavior pass
- no convincing local gather cadence and return rhythm yet

### 7. City life

Status: `Early live scaffolding`

What exists:

- city task points
- city templates/playlists
- city reserve design note
- reserve-city population scaffolding in the ledger and population tick
- reserve bots can now bias toward city errands in Stormwind/Orgrimmar

What is still incomplete:

- no linger/cooldown release behavior yet
- no richer social noise/emote layer yet
- city reserve pools likely still need seeded identities and tuning
- more anchors will help density and variety

### 8. Combat

Status: `Biggest depth gap`

What exists:

- world bots already share a lot of player-like stats/loadout prep
- combat profile design direction exists
- movement doctrine roadmap exists

What is still incomplete:

- many class/spec combat runtimes are still shallow
- doctrine/profile system is not yet the main runtime truth
- world PvP/nemesis/guild-war behavior is not implemented

---

## What the user's summary got right

This is all basically correct:

- bots can be generated with race/class/spec/loadout identity
- gear refresh exists
- travel capability is far beyond straight-line wandering now
- questing can resume from prior work if it still makes sense
- gathering/skinning simulation is still not done
- route coverage is still incomplete
- not all boats/zeppelins/portals are validated
- guild/rival/war systems are still missing
- combat depth is still behind travel depth

The only nuance worth adding is:

- mounts/travel mobility exist as a directionally real slice, but that part is
  still not "finished and unquestioned" yet

---

## Recommended next work

### Immediate current slice to finish

Finish the city reserve slice cleanly:

1. add linger/cooldown dematerialization
2. seed/test reserve populations for Stormwind and Orgrimmar
3. verify city fill behaves naturally instead of like a light switch

Why:

- the scaffolding is already landed
- the remaining work is focused and close at hand
- finishing it gives a visible payoff quickly

### Next major milestone after that

Make questing look alive **after arrival**:

1. load quest-hub JSON into the server
2. resolve `quest_auto` into quest hubs instead of only broad zone rows
3. use exported `taskAreas` for local quest-work loops
4. use weighted branch continuation between hubs

Why:

- travel is no longer the main bottleneck
- the next realism gain comes from bots seeming busy in the right places
- this also unlocks a better base for gathering loops later

### Next major milestone after quest hubs

Build local fake-work loops:

- herb/ore routelets
- pretend skinning/grinding loops
- "work an area, return, move on" behavior

Why:

- this is the missing bridge between destination choice and believable presence

### Lower-priority but important parallel tracks

These should stay visible, but they do not feel like the best immediate next
mainline slice:

- portal-room validation
- remaining transport coverage
- guild/rival/war systems
- deeper combat doctrine/profile work

Combat is strategically important, but right now the highest leverage still
looks like:

- finish city presence
- then finish local quest/gather presence

That sequence gets the world looking more inhabited before we spend longer on
deep per-spec combat fidelity.
