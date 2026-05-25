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

One major design assumption has changed, though:

- abstract/offscreen world bots are still ledger-driven first
- visible/materialized world bots are now pivoting toward leased
  `Player` shells rebuilt from that ledger truth
- the older creature-backed visible world-bot lane is no longer the end-state;
  it is now mostly:
  - a compatibility path
  - a source of shared behavior that still needs extraction
  - a fallback while the player-shell lane finishes proving itself

The system already has a real foundation for:

- persistent bot identity
- world-session continuity
- route-aware travel
- taxi travel
- first real boat and zeppelin seams
- quest-inspired destination selection
- quest resume memory

The biggest remaining gap is no longer "can bots go places?" and it is no
longer "can they carry believable baseline combat stats?"

The biggest remaining gaps are now:

- making bots **look busy and believable after arrival**
- finishing richer local work loops for questing, gathering, and city life
- deepening higher-order combat behavior such as party coordination, item use,
  and pet/autocast polish

---

## Current capability snapshot

### 1. Identity / generation / persistence

Status: `Mostly live, with player-shell materialization now the target visible runtime`

What exists:

- persistent identity ledger in `living_world_bot_identity`
- player-shell appearance bytes and rebuild metadata in the same ledger family
- named bots with:
  - race
  - class
  - spec
  - faction
  - personality
  - home city/zone anchors
- session counting and last-seen tracking
- session-scoped world-bot mood rolls at activation:
  - `65%` uninterested
  - `20%` opportunistic
  - `10%` aggressive
  - `5%` coward
  - level `75+` bots never roll coward and instead skew to:
    - `70%` uninterested
    - `20%` opportunistic
    - `10%` aggressive
- future world-PvP task families are eligible only for bots whose current
  session mood is `aggressive`
- world bots now carry a small generic potion budget in the ledger:
  - starts/caps at `5`
  - can be projected into real inventory for leased player shells
  - can still be auto-consumed through the simulated item-use lane by the older
    creature-backed runtime where needed
  - only refills through authored `city_errand` mailbox / auction-house stops
- combat doctrine item actions can now use symbolic selectors like `hp` / `mp`
  instead of only hard item ids:
  - world bots resolve them to the best potion for their level and spend one
    ledger charge
  - account/companion bots can resolve the same selector shape against real bag
    inventory
- runtime encounter behavior now has a first live pass:
  - `aggressive` bots can proactively attack opposing-faction player-like
    targets in the wild up to `bot level + 5`
  - `opportunistic` bots evaluate nearby opposing-faction player-like targets
    with a level-weighted attack roll
  - `uninterested` bots do not proactively start those fights
  - `coward` bots try to avoid same-level-or-higher opposing-faction
    player-like targets
- named neutral hub bubbles currently block new faction aggression in:
  - Gadgetzan
  - Booty Bay
  - Shattrath
  - Dalaran
  while still allowing already-active combat dragged into town to continue
- online-time progression
- level progression over time
- ledger-backed `30 min - 3 hour` session budgets for active world bots
- if a bot runs out of sensible chores before the budget is gone, it can clock
  out early or roll for another fresh shift immediately
- end-of-shift extension rolls for another fresh `30 min - 3 hour` activation:
  - `25%` after the first finished shift
  - `15%` after the second
  - `5%` thereafter
- resume breadcrumbs in the ledger for:
  - last immediate task/activity key
  - last quest hub key
  - elapsed time already spent in that hub
- assigned-gear refresh can now be honored at new session assignment time if a
  bot leveled into a new gear band between shifts
- a config-gated debug idle watchdog can now warn on long no-movement materialized
  stalls during active non-idle work and can optionally force a worldserver
  error-exit after logging session/runtime breadcrumbs for postmortem review
- successor/retirement concepts already in the broader identity flow
- leased `LedRes_*` player-shell pool and ledger-shell rebuild log
- startup stale-shell recovery and logout writeback for ledger shells

What is still thin:

- personality behavior is now real, but still first-pass:
  - opportunistic "concerned pause" flavor is not yet explicit
  - coward avoidance is a simple retreat lane, not a richer hide/evasion system
  - neutral hubs are hardcoded v1 circles, not a data-driven authoring system yet
- social identity is incomplete:
  - no guild system yet
  - no rival/war state yet

### 2. Gear / talents / preparation

Status: `Live, with a split between legacy creature parity and the new player-shell rebuild path`

What exists:

- virtual/assigned loadout resolution
- spec-aware preparation flow
- passive spell application
- player-like stat baseline work
- gear refresh over time
- world-bot glyph materialization
- stage-0 curated level-80 pre-raid gear templates for the modernized default
  families
- player-shell rebuild inputs for:
  - assigned gear
  - display loadouts
  - doctrine/talent/glyph/action-bar packages
  - persistent appearance bytes

What is still incomplete:

- later endgame staged gear sets are still not fully seeded past stage `0`
- some class/spec behavioral nuance still depends on combat/runtime polish
- startup prep for class-created consumables still needs more coverage in the
  shell lane
- some PvE rescue/tank-handoff behavior is still trapped in the old
  creature-first orchestration layer and needs extraction

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

Status: `Live and materially improving, with polish still needed`

What exists:

- broad zone quest task resolution
- quest-hub export pipeline from extracted quest data
- compact per-zone quest-hub JSON files
- filtering out event/daily/weekly/monthly quest noise
- quest resume memory in the ledger/session composer
- runtime quest-hub loading and weighted hub follow-on chaining
- low-level `quest_auto` home-turf bias for levels `1-15`
- budget-aware quest sessions:
  - task duration is set by the scheduler/template
  - hub duration is tracked separately as cumulative "time spent here"
  - resumed quest tasks can keep spending the same hub across multiple quest
    chunks and later activations until it is exhausted
  - if a resumed quest chunk outlives the current hub, the remaining time can
    carry into the next eligible hub instead of forcing a fresh unrelated pick

What is still incomplete:

- richer local objective behavior inside a task area is still first-pass
- broader authored validation across many zone chains is still needed
- very deep playlist-aware quest resume semantics still have room to improve

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

Status: `Large remaining depth area, but much healthier baseline`

What exists:

- world bots already share a lot of player-like stats/loadout prep
- modernized doctrine families exist for every non-Druid default class family
- combat profile/doctrine runtime is live for those modernized families
- movement doctrine roadmap exists
- ambient group scaffolding now exists for leader/follower party metadata
- creature-backed ambient bots can now suspend travel for ground combat and
  resume afterward
- ambient grouped followers can now join nearby creature fights instead of
  leaving the tank completely alone
- glyph materialization is live for world bots, including validated Frost Mage
  and Unholy DK pet-affecting glyph cases
- reactive gear proc support now covers equip-aura, direct-damage enchant, and
  landed-hit combat-proc lanes
- class-pet first-pass support is proven for:
  - Frost Mage Water Elemental
  - Unholy DK ghoul
  - Hunter permanent pet maintenance
  - Warlock demon maintenance
  - Shaman multi-summon cases such as Feral Spirit and Fire Elemental
- parked-owner / active-pet behavior is proven in forced hold-mode harnesses

What is still incomplete:

- world PvP/nemesis/guild-war behavior is not implemented
- party-wide combat coordination is still missing:
  - no shared interrupt claim system
  - no party blackboard / shared danger state
  - no true tank/off-tank assignment model yet
- tank awareness is still weaker than it needs to be for creature packs and
  surprise adds:
  - primary-target threat exists
  - pack-level threat and nearby dangerous-cast awareness do not
- healer/tank/DPS cooperation is improving, but still not yet "dungeon smart"
- on-use item breadth and weird scripted item validation still trail passive and
  common reactive gear fidelity
- pet spell/autocast refinement and stance/control exposure still need polish

---

## What the user's summary got right

This is all basically correct:

- bots can be generated with race/class/spec/loadout identity
- gear refresh exists
- travel capability is far beyond straight-line wandering now
- questing can resume from prior work if it still makes sense
- if a bot has clearly outgrown the last quest hub or zone, the composer can
  fall forward to a new level-appropriate quest area instead of hard-resuming
  stale content
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

### Current strongest remaining tracks

There is no single mandatory "finish this one last subsystem" slice anymore.
The strongest remaining tracks are:

1. quest-hub runtime consumption and believable local post-arrival work loops
2. local fake-work loops for gathering/skinning/resource behavior
3. party combat coordination and rescue/interrupt blackboard work
4. command/stance parity and deeper pet/item combat polish

Those are now higher leverage than more baseline stat-sheet work.

### Next major combat milestone after the current creature-pack stabilization

Build shared party combat coordination:

1. add a shared party combat awareness layer / blackboard
2. let grouped bots publish:
   - primary target
   - danger casts
   - interrupt claims
   - tank/healer/distress state
3. let roles consume that shared state differently:
   - DPS interrupt first
   - healer heal/escape first
   - tank fallback interrupts and peel decisions
4. extend the same layer later into:
   - raid icon orchestration
   - tank/off-tank swap rules
   - encounter-specific tactical assignments

Why:

- current single-bot combat improvements are finally good enough that the next
  realism jump comes from coordination, not only rotation cleanup
- the same layer will be needed for:
  - 2-3 bot quest parties
  - dungeon pulls
  - raid tank swaps
  - coordinated interrupts and crowd control

Implementation note for the current ambient runtime:

- do not try to make the task/session planner own this layer
- do not bury it inside one bot's combat profile rows
- and do not model it as literal chat

The correct shape is a small shared group-combat context that lives between:

- the existing grouped-combat sensing helpers
- and the existing per-bot movement / spell doctrine

The current best insertion points in `WorldBotCreatureAI.cpp` are:

- `JustEngagedWith(...)`
- `DamageTaken(...)`
- `SuspendCurrentStepForCombat(...)`
- `TryJoinNearbyAmbientCombat(...)`
- `RequestGroupedCombatTarget(...)`
- `TryAdoptGroupedCombatTarget(...)`
- `TrySustainAmbientCombat(...)`
- `TickCombat(...)`

That gives a clean first milestone:

1. publish party state on chaotic engage / pull start
2. let a targetless or distressed bot query shared state instead of only local
   scans
3. let tank and interrupt-capable roles claim actions once
4. let the rest of the party react to those claims

The initial goal is not "raid AI." The initial goal is:

- planned pulls feel staged instead of abrupt
- surprise pulls stabilize instead of three bots improvising separately
- grouped bots do not resume travel just because one local state flickered
- healer and DPS understand when the tank has or has not actually anchored the
  fight

Delivery order should stay disciplined:

1. make this solid for ambient/world-bot parties first
2. let companion/account bots in smart mode consume the same runtime layer
3. add player-intent inference ("body language") for human-led groups
4. add explicit addon controls only for cases where inference is too ambiguous

This keeps one combat-coordination brain instead of:

- one for world bots
- one for smart companions
- and a third one for player-led parties

The reusable target is:

- one shared combat-state runtime
- multiple signal sources

Signal sources later become:

- AI-owned publication for ambient/world bots
- inferred player-state publication for human-led groups
- optional explicit player commands for precision cases

The next responsibility immediately after that is **back-line rescue / peel
ownership**:

- when a fresh add or patrol joins
- and that hostile chooses a healer or ranged ally
- the party needs one explicit owner for the peel

This should not default to "tank abandons anchor."

Preferred policy:

1. shared state marks the new hostile and distressed ally
2. a suitable DPS role claims peel ownership
3. healer and other DPS can see that claim and avoid duplicate retargeting
4. tank only becomes fallback when no good peel claimant exists, or when the
   tank has a class-specific remote rescue tool that does not break anchor

Distress severity should also be role-aware and time-aware, not hit-spam:

- DPS distress:
  - initial alert may be self-managed
  - only persistent pressure should trigger helper roll call
  - use an attacker-HP gate (for example `> 50%`) before spending more party
    attention on a DPS-side add

- healer distress:
  - immediate help signal
  - no attacker-HP gate
  - escalates faster and should override normal free-DPS assumptions

- distress should clear after a short quiet window from that same attacker,
  instead of lingering until full combat end

So the first useful distress ladder is:

1. publish once on first meaningful contact
2. escalate only if the same attacker remains on the same ally over time
3. clear after roughly 3 quiet ticks from that attacker

That makes the shared combat layer useful for both:

- opening stabilization
- and mid-fight add rescue / healer protection

### Need Soon: make session/task control mostly bot-polled instead of manager-pushed

The combat/travel boundary has repeatedly shown a fragile pattern:

- combat flickers
- a higher-level task/session system notices "not in combat"
- the session layer pushes a resume/update immediately
- the bot is still effectively busy, interrupted, or about to relatch combat
- travel/task logic and combat logic start fighting over the same body

The near-term fix direction should be:

1. keep the task/session system as the source of truth for plans and records
2. stop treating it like a constant commander that may push "resume now"
3. let the bot ask for its next assignment only when it is actually ready

Preferred mental model:

- task manager = ledger / planner / record holder
- bot AI = executor and readiness gate
- bot asks:
  - I spawned; what should I be doing?
  - I finished my step; what next?
  - combat is really over and all-clear passed; what now?
  - I reached the destination; what now?
- manager answers:
  - resume suspended travel
  - continue route
  - start grind step
  - hold here
  - switch to a new session step

Important nuance:

- this does **not** require a pure pull-only system everywhere
- hard invalidation may still be pushed into state:
  - explicit GM/admin override
  - task cancellation
  - leader/session invalidation
  - dematerialization/despawn constraints
- abstract/offscreen bots can still be advanced primarily by the ledger,
  because there is no live physical body for combat/travel systems to fight
  over
- materialized bots are the important exception:
  once they are real creatures in the world, route/task resume should be
  reduced to pending intent until the bot itself reaches an idle/all-clear
  state and asks what comes next
- but even then the preferred shape is:
  - manager marks current assignment dirty/invalid
  - bot reconciles on the next safe tick

Implementation direction:

1. convert "resume now" style session updates into pending intent/state
2. make the bot consume that intent only when:
   - not in combat
   - not engaged
   - no active combat interruption
   - post-combat all-clear window passed
3. preserve existing suspend/resume plumbing, but move step continuation behind
   the bot's readiness gate

Why this is important:

- it matches the same "ask when needed" lesson already learned in party combat
  target handoff
- it should reduce route/combat tug-of-war and false mid-combat resumes
- it lets the scheduler remain authoritative without becoming noisy or brittle

### Lower-priority but important parallel tracks

These should stay visible, but they do not feel like the best immediate next
mainline slice:

- portal-room validation
- remaining transport coverage
- guild/rival/war systems
- deeper combat doctrine/profile work

Combat is still strategically important, but the remaining work is now less
"make the bot fundamentally combat-capable" and more:

- coordinated parties
- richer item behavior
- refined pet behavior
- niche combo/timing sophistication

That is a healthier place for the system to be in than the earlier snapshots
this document was written from.
