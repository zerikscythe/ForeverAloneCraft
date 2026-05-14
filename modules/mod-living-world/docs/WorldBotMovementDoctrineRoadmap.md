# World Bot Movement Doctrine Roadmap

This roadmap tracks the move from simple world-bot combat chase behavior toward
an explicit, hazard-aware movement doctrine.

It is intentionally incremental. Each slice should be small enough to validate,
commit, and build on without destabilizing the live world-bot runtime.

## Design principles

- Keep **hazard sensing** as a separate safety subsystem.
- Make hazard escape the **highest-precedence movement override**.
- Keep **playstyle doctrine** separate from **moment-to-moment posture**.
- Reuse a **shared movement/posture engine** with data-authored archetypes later.
- Land the first slices as **pure helpers/value models** before changing live AI.

## Priority order

1. Safety / hazard precedence
2. Posture vocabulary and movement decision model
3. Combat-situation evaluation
4. World-bot combat integration
5. Action gating by posture
6. Shared hazard convergence
7. Data-authoring / archetype expansion

## Slice 1 — Shared posture model and hazard-aware scaffolding

Goal:

- Introduce explicit movement-doctrine value types without changing live combat
  behavior yet.

Deliverables:

- `WorldBotCombatPosture` enum
- `WorldBotMovementDecision` value type
- `WorldBotMovementDecisionSource` enum
- `WorldBotCombatSituation` value type shell
- `WorldBotHazardSnapshot` value type shell
- focused pure tests for precedence and basic defaults

Definition of done:

- new movement-doctrine model types compile
- tests prove hazard decisions outrank posture decisions
- no live AI behavior change yet

Status: `Complete`

## Slice 2 — Combat-situation evaluator and posture selection

Goal:

- Build a pure evaluator that maps combat state into posture recommendations.

Deliverables:

- `WorldBotMovementDoctrineEvaluator`
- initial archetype-oriented heuristics:
  - frontline tank
  - sticky melee
  - turret caster
  - mobile ranged
  - backline healer
- tests for:
  - too-close caster => `Kite` or `Reposition`
  - too-far melee => `Close`
  - low-health ranged => `Retreat`
  - healthy tank in range => `Hold`

Definition of done:

- evaluator is pure and unit-tested
- no pathing or MotionMaster work inside evaluator

Status: `Complete`

## Slice 3 — World-bot combat movement integration

Goal:

- Replace the simple `move_chase` fallback in `WorldBotCreatureAI::TickCombat`
  with posture-aware movement execution.

Deliverables:

- integrate situation snapshot creation in world-bot combat tick
- evaluate movement doctrine each combat tick
- execute posture-specific movement actions:
  - `Hold`
  - `Close`
  - `Reposition`
  - `Kite`
  - `Retreat`
- preserve existing chase fallback as emergency safety behavior only

Definition of done:

- world bots no longer default to unconditional chase when they fail to act
- combat movement traces include posture / decision source

Status: `Complete`

## Slice 4 — Hazard precedence and action gating

Goal:

- Make hazard escape a formal doctrine override and constrain spell/action
  execution by posture.

Deliverables:

- add a `HazardEscape` decision source into movement arbitration
- formalize the interface between hazard state and movement doctrine
- suppress unsafe hard-cast behavior while moving / kiting / retreating
- allow normal cast windows while holding or safely repositioned

Definition of done:

- hazards remain separate detectors but are woven into movement arbitration
- posture affects action legality

Status: `Complete` (first-pass)

Current implementation note:

- world bots now use posture-derived hard-cast gating in the combat runtime
- world bots now treat configured explicit hazard auras as a first-pass hazard
  override input during combat movement arbitration
- companion-style damage-pattern hazard sensing is **not** yet unified into the
  world-bot doctrine path; that remains a later refinement

## Slice 5 — Shared hazard convergence

Goal:

- Reuse more of the companion-style hazard runtime semantics inside world-bot
  combat doctrine without pretending the two runtimes already share identical
  party/owner adapters.

Deliverables:

- extract a shared repeated-damage / commitment-window helper
- reuse that helper from `BotHazardSensor`
- feed world-bot hazard snapshots with:
  - explicit hazard aura detection
  - repeated-damage detection while stationary
  - commitment-window persistence to reduce posture jitter
- enrich movement traces / hazard snapshot fields so later anchor-aware work can
  layer on top cleanly

Definition of done:

- world bots can enter hazard override from repeated-damage sensing, not just
  explicit auras
- world bots keep hazard override stable through the configured commitment window
- account/companion bots and world bots share the same repeated-damage hazard
  evaluator and tuning path

Status: `Complete` (first-pass)

Current implementation note:

- world bots now share the explicit-aura + repeated-damage hazard evaluation core
  with companion/account bots
- world-bot hazard snapshots now persist through the configured commitment window
  instead of dropping immediately when a single tick clears
- clean-anchor selection is still companion-only today because world bots do not
  yet expose equivalent party/owner context in their runtime adapter

## Slice 6 — Data-authored movement archetypes

Goal:

- Shift fixed playstyle parameters out of hardcoded C++ and toward authorable
  movement archetypes / variants.

Deliverables:

- movement archetype schema or config surface
- initial authored variants for:
  - tank
  - melee dps
  - healer
  - turret caster
  - mobile ranged

Definition of done:

- runtime can resolve authored movement profiles instead of only code defaults

Status: `Planned`

## Recommended implementation notes

- Prefer pure helpers under `src/model/` and `src/service/` first.
- Keep `WorldBotCreatureAI` orchestration thin.
- Keep `BotHazardSensor` as the detector; do not retire it.
- Treat hazard escape as a safety override, not a playstyle.
