# Glyph Audit

## Current State

World bots now have a first-pass glyph pipeline.

- [WorldBotPreparedBuild.h](D:/src/azerothcore-wotlk/modules/mod-living-world/src/model/WorldBotPreparedBuild.h) carries prepared glyph entries.
- [WorldBotPreparationService.cpp](D:/src/azerothcore-wotlk/modules/mod-living-world/src/service/WorldBotPreparationService.cpp) loads glyph template rows into the prepared build.
- [WorldBotCreatureAI.cpp](D:/src/azerothcore-wotlk/modules/mod-living-world/src/ai/WorldBotCreatureAI.cpp) materializes those glyph spell auras onto world bots.
- [rev_living_world_065_full_glyph_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_065_full_glyph_templates.sql) is the full live seed for the modernized specs.

The local Icy Veins mirror still only preserves a `glyph_hash`, so the relink source now lives in:

- [icy_veins_wotlk_builds.json](D:/src/azerothcore-wotlk/tools/lw-editor/data/icy_veins_wotlk_builds.json)
- [icy_veins_wotlk_glyph_relink.json](D:/src/azerothcore-wotlk/tools/lw-editor/data/icy_veins_wotlk_glyph_relink.json)
- [generate_lw_bot_glyph_templates.py](D:/src/azerothcore-wotlk/tools/lw-editor/helpers/generate_lw_bot_glyph_templates.py)

That relink layer exists because Icy Veins calculator ids do not line up cleanly with the local DB/DBC ids. We now treat:

`build_key + glyph_hash + glyph names -> local item/DBC resolution -> glyph aura spell ids`

For real Players, glyphs are applied through the core Player path:

- [SpellEffects.cpp](D:/src/azerothcore-wotlk/src/server/game/Spells/SpellEffects.cpp) `EffectApplyGlyph`
- [Player.h](D:/src/azerothcore-wotlk/src/server/game/Entities/Player/Player.h) `SetGlyph`
- [PlayerStorage.cpp](D:/src/azerothcore-wotlk/src/server/game/Entities/Player/PlayerStorage.cpp) `_LoadGlyphAuras`

That path resolves:

`glyph id -> GlyphPropertiesEntry.SpellId -> CastSpell(glyph spell aura)`

This is encouraging because most glyph behavior is aura-driven and can likely be reused.

## Bot Plan

### Account / Companion Bots

Use the normal Player glyph path:

1. resolve recommended glyph IDs
2. assign real glyph slots
3. let core apply and persist glyph auras through `character_glyphs`

### World Bots

Use the materialized glyph-aura path:

1. load the relinked glyph names for the local `build_key`
2. resolve each name through local item + DBC data
3. map to `GlyphPropertiesEntry.SpellId`
4. apply those glyph spell auras during world-bot materialization

This should let most glyph scripts work without inventing a second glyph engine.

### Account / Companion Bots Nuance

Account / companion bots are near-1:1 Player clones, so they can already
inherit real glyph state when the source character has glyphs populated through
the normal clone dump/load path.

The remaining gap is narrower:

1. there is still no dedicated Living World "recommended glyph package" overlay
   for account/companion bots
2. there is still no explicit Living World glyph-slot authoring/sync domain for
   account/companion runtime updates

So the current world-bot glyph pipeline is the only fully explicit
spec-recommended glyph path inside `mod-living-world`, but account/companion
bots are not glyph-empty by default.

## Safe Majority

Most recommended glyphs look like they should work once the correct glyph aura is present on the bot.

Examples:

- `Glyph of Arcane Blast`
- `Glyph of Fireball`
- `Glyph of Frostbolt`
- `Glyph of Divine Plea`
- `Glyph of Seal of Vengeance`
- `Glyph of Penance`
- `Glyph of Holy Light`
- `Glyph of Beacon of Light`
- `Glyph of Life Tap`
- `Glyph of Steady Shot`
- `Glyph of Serpent Sting`
- `Glyph of Explosive Shot`
- `Glyph of Kill Shot`

These are mostly stat, cooldown, proc, or spell-modifier effects and should fit the existing passive-aura materialization model.

## Special Cases To Validate

These glyphs are the ones most likely to need extra care when we bolt glyphs onto world bots.

### Frost Mage

- `Glyph of Eternal Water`

Why it matters:

- recommended by Icy Veins for Frost Mage
- changes summon behavior from temporary elemental to permanent elemental
- summon code checks `HasAura(SPELL_MAGE_GLYPH_OF_ETERNAL_WATER)`

Relevant code:

- [spell_mage.cpp](D:/src/azerothcore-wotlk/src/server/scripts/Spells/spell_mage.cpp)

Notes:

- the main benefit should work if the world bot has the glyph aura
- removal cleanup currently has a `ToPlayer()` branch, so the cleanup edge may need a small world-bot bridge
- validated live with
  - [mage-pet-harness-20260520-180713.report.txt](D:/src/azerothcore-wotlk/mage-pet-harness-20260520-180713.report.txt)
- proof point:
  - the glyph aura `70937` materialized on the owner
  - summon behavior switched to the permanent elemental path (`entry=37994`)

### Unholy Death Knight

- `Glyph of the Ghoul`

Why it matters:

- recommended by Icy Veins for Unholy
- changes ghoul scaling by reading `owner->GetAuraEffect(...)`

Relevant code:

- [spell_dk.cpp](D:/src/azerothcore-wotlk/src/server/scripts/Spells/spell_dk.cpp)

Notes:

- validated live after wiring direct world-bot ghoul summon maintenance
- proof runs:
  - with glyph:
    - [dk-pet-harness-20260520-203240.report.txt](D:/src/azerothcore-wotlk/dk-pet-harness-20260520-203240.report.txt)
  - without glyph:
    - [dk-pet-harness-20260520-203459.report.txt](D:/src/azerothcore-wotlk/dk-pet-harness-20260520-203459.report.txt)
- proof points:
  - owner aura state flips cleanly:
    - `owner_glyph_of_the_ghoul=1` vs `0`
  - both runs summon the same ghoul entry:
    - `Risen Ghoul (26125)`
  - the glyph materially increases pet stats on summon:
    - HP `19325` vs `12655`
    - AP `3131` vs `2549`
    - STR `2542` vs `1960`
    - STA `1827` vs `1160`

### Hunter Minor Pet Utility

- `Glyph of Mend Pet`

Why it matters:

- recommended by Icy Veins for Hunter minor glyphs
- proc script explicitly grants a happiness effect

Relevant code:

- [spell_hunter.cpp](D:/src/azerothcore-wotlk/src/server/scripts/Spells/spell_hunter.cpp)

Notes:

- this is lower priority than combat throughput glyphs
- if world-bot pets do not model pet happiness meaningfully, this glyph may be harmless but low-value

### Summon / Pet Validation Bucket

These do not currently show up as clearly scripted edge cases in the same way as `Eternal Water` or `Glyph of the Ghoul`, but they should be validated once glyph auras are live:

- `Glyph of Feral Spirit`
- `Glyph of Fire Elemental Totem`
- `Glyph of Felguard`

These are not automatic blockers. They are simply the next most likely summon-owner interaction points.

## Recommended Next Validation Order

1. re-run Frost Mage proof whenever summon logic changes
2. validate Hunter `Glyph of Mend Pet`, mostly to confirm it stays harmless with forced-max pet happiness
3. pick one or two throughput spell glyphs from the safe-majority bucket for numeric spot checks
4. later add the explicit Living World glyph-slot overlay path for account /
   companion bots when we want recommended builds to override or seed clone
   glyph state

## Icy Veins Spot Checks

The following spec pages were explicitly checked during this audit:

- Frost Mage
- Holy / Protection / Retribution Paladin
- Unholy Death Knight
- Beast Mastery / Marksmanship / Survival Hunter
- Demonology Warlock
- Discipline Priest
- Combat Rogue
- Enhancement Shaman

The intent here was not to lock every glyph choice into this document, but to separate:

- glyphs that should work through shared aura reuse
- glyphs that involve pets, summons, or owner-state interactions
