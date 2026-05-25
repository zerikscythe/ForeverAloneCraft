# World Bot Self-State Roadmap

## Scope

This roadmap covers the shared **self-state** layer that sits underneath:

- Druid forms
- Warrior stances
- Death Knight presences
- Hunter aspects
- Shaman travel states such as `Ghost Wolf`
- Warlock armor maintenance
- existing Paladin seal maintenance as a nearby precedent

The goal is not to solve every class-specific nuance in one pass. The goal is to
replace one-off "keep this buff up" logic with a shared runtime model that can
express:

- preferred combat state
- preferred out-of-combat state
- preferred travel state
- simple active-state checks
- safe state application before higher-level doctrine work

This is the substrate Druid needs before its doctrine can be modernized
cleanly.

## Why This Exists

Core already has real shapeshift and stance mechanics:

- form auras
- stance/form legality checks
- model swaps
- power-type changes
- form-aware stat behavior

What Living World is still missing is the bot-side layer that decides:

- which self-state the bot wants right now
- whether it is already in that state
- when it should switch
- how to avoid state thrash

Today we still have a lot of special one-off helpers such as:

- `Battle Shout`
- `Horn of Winter`
- Paladin seal selection
- `Mark of the Wild`
- `Arcane Intellect`
- Warlock armor
- Rogue poison application

That works for simple maintenance, but it does not scale well into Druid.

## Current State

### Core Is Already Solid Enough

The AzerothCore substrate already handles the hard native mechanics once the
correct form/stance aura lands.

That means the main missing piece is not "teach the core what Bear Form is."
The missing piece is "teach the bot when and why to use Bear Form."

### Bot Runtime Is Still Thin Here

The world-bot runtime currently has no shared model for:

- preferred combat form
- preferred travel form
- preferred stance
- preferred presence
- preferred aspect

The runtime mostly knows how to maintain a few class-specific OOC spells, but
it does not yet have a shared self-state layer.

## Decision Rule

We now have a clearer split between two kinds of state work:

### Background Smart Switching

These are long-lived baseline states that a player normally keeps up without
micro-managing every few seconds:

- Druid baseline forms
- Death Knight presences
- Hunter aspects
- Shaman travel form (`Ghost Wolf`)
- Warlock armor

These belong in the shared self-state substrate.

### Doctrine Or Combat-State Switching

These are more tactical or action-shaped and should not be treated as a simple
"always keep this on" background loop:

- Warrior stances
- later Druid caster/form weave when specific actions require it
- any future temporary/tactical state swaps

These belong in doctrine or a later combat-state switching layer, not in the
first-pass OOC self-state maintainer.

## Execution Checklist

### Phase 1 - Shared Self-State Data Model

- [x] Add a dedicated prepared-build self-state model
- [x] Represent category and desired state cleanly:
  - [x] form
  - [x] stance
  - [x] presence
  - [x] aspect
  - [x] armor
  - [ ] seal
- [x] Allow separate preferences for:
  - [x] combat
  - [x] out of combat
  - [x] travel
- [x] Populate initial self-state preferences during world-bot preparation

Exit criteria:

- prepared build can describe what self-state the bot wants without needing
  doctrine rows yet

Status:

- complete for the first shared substrate slice
- Paladin seals still remain on the older dedicated helper path for now

### Phase 2 - Runtime Self-State Maintenance

- [x] Add a shared world-bot self-state maintenance function
- [x] Run self-state maintenance before normal OOC buff maintenance
- [x] Avoid stomping active spellcasts
- [x] Avoid repeated re-casting when the state is already active
- [x] Support simple travel-state preference selection

Exit criteria:

- runtime can evaluate and maintain preferred self-state without class-specific
  ad hoc checks for every case

Status:

- complete for first-pass OOC maintenance
- still not a full combat-state/stance choreography system yet
- utility spell families now get explicitly seeded into the prepared
  known-spell set when the normal combat-focused spell prep would otherwise miss
  them

### Phase 3 - First Class Coverage

- [x] Druid:
  - [x] Bear / Dire Bear preference seeding
  - [x] Cat preference seeding
  - [x] Moonkin preference seeding
  - [x] Tree preference seeding
  - [x] Travel preference seeding
- [x] Death Knight:
  - [x] Blood Presence preference seeding
  - [x] Frost Presence preference seeding
  - [x] Unholy Presence preference seeding
- [x] Hunter:
  - [x] Aspect maintenance on shared substrate
- [x] Shaman:
  - [x] Ghost Wolf travel preference seeding
- [x] Warlock:
  - [x] armor maintenance moved onto shared substrate

Exit criteria:

- the shared model is actually being used by at least one real stateful class
- Druid-ready groundwork is materially real

Status:

- first-pass background coverage is in code and partially proven
- Hunter `Aspect of the Dragonhawk`, Warlock armor, DK `Unholy Presence`, and
  Shaman `Ghost Wolf` travel behavior have all been probed with the self-state
  harness
- Druid is still blocked on missing doctrine/build prep
- Warrior stances were intentionally moved out of the background slice after
  harness results showed they behave more like tactical combat-state switching

### Phase 4 - State-Aware Combat Integration

- [ ] teach runtime to avoid impossible actions while in the wrong state
- [ ] optionally pre-shift before form-dependent actions
- [ ] avoid constant shift/unshift churn
- [ ] reintroduce Warrior stances through tactical combat-state switching instead
  of passive background maintenance

Exit criteria:

- state and doctrine no longer fight each other

### Phase 5 - Druid Doctrine Modernization

- [ ] modernize Balance
- [ ] modernize Feral tank
- [ ] modernize Feral DPS
- [ ] modernize Restoration
- [ ] validate the major form loops with harnesses

Exit criteria:

- Druid is no longer the last legacy doctrine family

## Proof Notes We Want

We should aim to prove these visibly rather than infer them:

- Druid world bot can maintain a preferred combat form
- Druid world bot can swap into travel form during travel
- Bear/Cat/Moonkin/Tree state checks are stable and do not thrash
- DK can maintain preferred presence through the shared substrate
- Hunter can maintain its baseline aspect through the shared substrate
- Shaman can enter travel form only when traveling
- Warlock can maintain its baseline armor through the shared substrate

## Immediate Next Slice

1. Keep the harness-driven validation loop going for the proven background-state
   classes
2. Use the same substrate to bring Druid forms online once Druid doctrine/build
   prep exists
3. Build the later combat-state switching layer for Warrior stances and other
   tactical swaps
