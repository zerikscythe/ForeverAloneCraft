# World Bot Class Pet Roadmap

## Scope

This roadmap covers **class-controlled pets and summons** for Living World bots, with the main focus on:

- world-bot permanent pets
- owner-to-pet bonus propagation
- pet control semantics
- summon-specific glyph and talent interactions
- harness-driven validation

It does **not** try to solve every creature summon in one pass. The priority is player-class pets that materially affect gameplay quality.

## Core Model

There are two different bot substrates:

### Account / Companion Bots

These are real `Player` clones.

Strengths:

- real player spellbook
- real inventory and equipment
- real glyph slots if present on the source toon
- real pet ownership substrate

Weaknesses:

- Living World-specific overlays are not always explicitly assigned
- not every recommended build artifact is pushed onto the clone automatically

### World Bots

These are creature-backed bots.

Strengths:

- we can materialize exactly the effects we care about
- we can author spec-driven overlays directly
- they are easier to instrument and test in isolation

Weaknesses:

- no native player pet ecosystem
- no native inventory / glyph slot / stable system
- pet behavior and pet bonuses must be bridged deliberately

## Current State

### Proven

The following are already working on the world-bot side:

- Frost Mage Water Elemental summon maintenance
- `Glyph of Eternal Water` permanent-elemental behavior
- Unholy DK ghoul summon maintenance
- `Glyph of the Ghoul` owner-aura-driven pet scaling
- Hunter permanent-pet summon maintenance
- Warlock demon summon maintenance
- Shaman multi-summon support for `Feral Spirit` and `Fire Elemental`
- basic pet control actions:
  - summon
  - assist
  - defend owner
  - follow return
- parked-owner / active-pet behavior for world bots in forced `hold` mode:
  - owner offense suppressed
  - owner movement suppressed
  - pet summon/maintenance preserved
  - pet defense preserved under hostile opener

Proof artifacts:

- [mage-pet-harness-20260520-180713.report.txt](D:/src/azerothcore-wotlk/mage-pet-harness-20260520-180713.report.txt)
- [dk-pet-harness-20260520-203240.report.txt](D:/src/azerothcore-wotlk/dk-pet-harness-20260520-203240.report.txt)
- [dk-pet-harness-20260520-203459.report.txt](D:/src/azerothcore-wotlk/dk-pet-harness-20260520-203459.report.txt)
- [hunter-pet-harness-20260520-222854.report.txt](D:/src/azerothcore-wotlk/hunter-pet-harness-20260520-222854.report.txt)
- [warlock-pet-harness-20260520-223014.report.txt](D:/src/azerothcore-wotlk/warlock-pet-harness-20260520-223014.report.txt)
- [shaman-pet-harness-20260520-223410.report.txt](D:/src/azerothcore-wotlk/shaman-pet-harness-20260520-223410.report.txt)
- [hunter-pet-harness-20260520-225239.report.txt](D:/src/azerothcore-wotlk/hunter-pet-harness-20260520-225239.report.txt)
- [warlock-pet-harness-20260520-225911.report.txt](D:/src/azerothcore-wotlk/warlock-pet-harness-20260520-225911.report.txt)

### Remaining

- owner/pet command parity is still debug-driven on the world-bot side rather than exposed through the full `.lwbot` command surface
- pet stance vocabulary is still coarse; we are proving `hold/passive` semantics before building a richer command layer
- pet spell/autocast refinement remains a later combat-quality pass rather than a structural blocker

### Intentionally Simplified

- world-bot pet happiness will be treated as effectively max
- `Glyph of Mend Pet` is low priority and should not drive a full hunger simulation

## Main Remaining Gaps

### 1. Permanent Pet Ownership for World Bots

This was the largest structural gap and is now covered for the first reliable slices:

- Hunter permanent pet
- Warlock demon

Remaining work in this area is refinement, not missing substrate:

- richer per-spec pet selection if we want more flavor later
- longer-lived lifecycle polish
- command-surface exposure

### 2. Owner-to-Pet Bonus Propagation

We need clear coverage for:

- owner glyph auras
- owner talents
- owner pet-related passive auras
- owner stat scaling hooks that core pet scripts read

Some of this already works when scripts check `HasAura(...)` or `GetAuraEffect(...)` on the owner. We should reuse those paths wherever possible instead of recreating the math.

### 3. Pet Control Semantics

We now have explicit world-bot rules for:

- owner `follow`
- owner `stay`
- owner `passive`
- pet `assist`
- pet `defensive`
- pet `passive`

Goal:

- owner control state should not accidentally suppress pet maintenance or pet defense logic
- parked owners should still be able to have useful pets

Current status:

- `hold/stay` suppresses owner movement
- `hold/passive` suppresses owner offense
- pet summon maintenance still runs
- a newly created pet inherits live combat context and can defend immediately

### 4. Pet Spell / Autocast Fidelity

Later-stage refinement:

- class-specific pet spell usage
- autocast toggles
- taunt / threat moves for tank-style pets
- defensive and utility pet buttons

This is important, but it should come after stable ownership and bonus propagation.

## Execution Order

### Phase 1 - Stabilize the Shared World-Bot Pet Base

Goal:

- treat summon/assist/defend/follow as a stable shared substrate

Tasks:

1. keep the current controlled-guardian bridge as the base
2. preserve pet maintenance before owner early-return states
3. standardize follow-return behavior
4. standardize defend-owner behavior

Exit criteria:

- a parked owner can still maintain an existing pet
- the pet can defend the owner
- the pet returns to follow state after combat

Status: complete

### Phase 2 - Hunter Permanent Pet Support

Goal:

- get one reliable Hunter world-bot pet path working end to end

Tasks:

1. pick a basic Hunter pet summon path
2. keep happiness effectively max
3. validate:
   - summon
   - ownership
   - assist
   - defend owner
   - follow return
4. verify important owner bonus reads

Exit criteria:

- a Hunter world bot can keep a pet active through normal combat
- the pet behaves sanely without manual babysitting

Status: complete

### Phase 3 - Warlock Permanent Demon Support

Goal:

- get one reliable demon path working end to end

Recommended first slice:

- Demonology `Felguard`

Fallback simpler first slice if needed:

- `Felhunter` or `Voidwalker`

Tasks:

1. add summon/maintain path for the selected demon
2. validate owner glyph/talent bonus reads
3. validate assist/defend/follow behavior

Exit criteria:

- one Warlock spec can maintain a demon reliably
- the demon receives the important owner-side modifiers that materially affect gameplay

Status: complete for Demonology/Felguard first slice

### Phase 4 - Shaman Summon Validation

Goal:

- validate temporary class companions after Hunter/Warlock permanent pets are stable

Targets:

- `Feral Spirit`
- `Fire Elemental`

Tasks:

1. confirm summon reliability
2. confirm owner glyph/talent effect reads where relevant
3. confirm limited-duration lifecycle is sane

Exit criteria:

- temporary summons behave predictably and benefit from the owner state we expect

Status: complete for `Feral Spirit` and `Fire Elemental`

### Phase 5 - Control and Stance Parity

Goal:

- make bot and pet control semantics predictable across world and account substrates

Tasks:

1. define owner stance behavior
2. define pet stance behavior
3. decide which owner commands suppress offense vs movement vs maintenance
4. expose the minimum stable command set for testing

Exit criteria:

- `stay` / `passive` / `follow` behave predictably
- owner and pet state do not fight each other

Status: structurally complete for the world-bot debug control lane; command-surface exposure remains future polish

### Phase 6 - Pet Spell Refinement

Goal:

- raise combat quality after the ownership substrate is trustworthy

Tasks:

1. validate class-specific pet casts
2. tune threat-style pet actions
3. tune defensive pet actions
4. revisit lower-value minor glyph interactions if still useful

Exit criteria:

- pet classes feel meaningfully closer to real player behavior in combat

Status: remaining refinement pass

## Harness Plan

Each class-pet phase should have a harness or controlled repro path.

### Existing

- Mage pet harness
- DK ghoul harness

### Needed Next

- parked-owner / active-pet control harness for broader command-surface validation
- optional Shaman parked-owner spot check if we want the same explicit proof for temporary summons

Each harness should ideally prove:

1. owner spawned with expected build state
2. summon occurred
3. pet owner GUID is correct
4. pet aura/bonus-sensitive stats changed when expected
5. pet assisted owner target
6. pet defended owner when owner was engaged first
7. pet returned to follow mode after combat

## What We Will Not Simulate Early

These are explicitly deferred unless they become blockers:

- pet hunger simulation
- full stable / pet persistence UX
- exhaustive pet autocast configuration UI parity
- every minor glyph edge case

## Definition of Done

Class-pet support is in good shape when:

1. Frost Mage and Unholy DK stay green after changes
2. Hunter has one stable permanent-pet world-bot path
3. Warlock has one stable demon world-bot path
4. owner-to-pet bonus propagation is proven on the meaningful gameplay hooks
5. parked-owner / active-pet behavior is predictable
6. the remaining pet gaps are refinement, not structural missing systems

Current read: achieved for the current roadmap scope

## Immediate Next Slice

The next implementation target should be:

1. expose world-bot pet/owner control semantics through the real command surface if we want live GM/user control instead of debug forcing
2. broaden pet spell/autocast refinement if combat-quality polish becomes the next priority
