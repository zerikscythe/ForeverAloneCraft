# Pre-Raid Gear Audit

This audit tracks which modernized class/spec families should receive distinct
stage-0 curated level-80 assigned gear templates, using the WotLK Classic Icy
Veins pre-raid gear pages as the source spine.

Notes:

- This audit is about **curated assigned gear templates** for fresh-80 stage `0`,
  not the broader virtual stat loadout tiers.
- Hunter and Warlock stage-0 curated sets are already seeded in
  [D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_058_hunter_warlock_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_058_hunter_warlock_preraid_gear_templates.sql).
- Druid is not included here yet because it is not part of the fresh default
  doctrine set after the legacy cleanup pass.

## Audit Result

### Paladin

- `Holy`: **spec-specific**
- `Protection`: **spec-specific**
- `Retribution`: **spec-specific**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_061_paladin_warrior_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_061_paladin_warrior_preraid_gear_templates.sql).

Reason: healer, tank, and melee DPS gear diverge heavily in armor, shield/libram,
weapon, trinket, and stat focus.

### Rogue

- `Assassination`: **spec-specific**
- `Combat`: **can share with Subtlety**
- `Subtlety`: **can share with Combat**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_059_rogue_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_059_rogue_preraid_gear_templates.sql).

Reason: Assassination has the clearest weapon preference split. Combat and
Subtlety pre-raid pages are close enough that a shared stage-0 template is a
reasonable first pass.

### Warrior

- `Arms`: **can share with Fury**
- `Fury`: **can share with Arms**
- `Protection`: **spec-specific**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_061_paladin_warrior_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_061_paladin_warrior_preraid_gear_templates.sql).

Reason: the tank page is a different gear world. Arms/Fury are close enough for
one DPS stage-0 set in a first pass, even if later stages diverge.

### Death Knight

- `Blood`: **spec-specific**
- `Frost`: **spec-specific**
- `Unholy`: **spec-specific**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_062_death_knight_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_062_death_knight_preraid_gear_templates.sql).

Reason: Blood tanking is separate, and Frost/Unholy differ enough in weapon,
sigil, trinket, and DPS weighting that they should each get their own stage-0
template.

### Shaman

- `Elemental`: **spec-specific**
- `Enhancement`: **spec-specific**
- `Restoration`: **spec-specific**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_063_shaman_priest_mage_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_063_shaman_priest_mage_preraid_gear_templates.sql).

Reason: caster DPS, melee DPS, and healer loadouts diverge too much to share a
useful stage-0 template.

### Priest

- `Shadow`: **spec-specific**
- `Discipline`: **spec-specific**
- `Holy`: **spec-specific**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_063_shaman_priest_mage_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_063_shaman_priest_mage_preraid_gear_templates.sql).

Reason: Shadow is clearly separate, and the two healer pages still diverge
enough in slot choices and stat emphasis that they should stay separate.

### Mage

- `Arcane`: **spec-specific**
- `Fire`: **spec-specific**
- `Frost`: **spec-specific**

Status: implemented in
[D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_063_shaman_priest_mage_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_063_shaman_priest_mage_preraid_gear_templates.sql).

Reason: these caster templates are close, but not actually identical. Neck,
rings, wand/back/chest/hand choices drift enough that we should keep them
separate rather than force one shared stage-0 set.

### Hunter

- `BeastMastery`: **can share across all Hunter specs**
- `Marksmanship`: **can share across all Hunter specs**
- `Survival`: **can share across all Hunter specs**

Status: already implemented as three identical stage-0 spec rows in
[rev_living_world_058_hunter_warlock_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_058_hunter_warlock_preraid_gear_templates.sql).

### Warlock

- `Affliction`: **can share across all Warlock specs**
- `Demonology`: **can share across all Warlock specs**
- `Destruction`: **can share across all Warlock specs**

Status: already implemented as three identical stage-0 spec rows in
[rev_living_world_058_hunter_warlock_preraid_gear_templates.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_058_hunter_warlock_preraid_gear_templates.sql).

## Remaining Template Work

Stage-0 curated pre-raid coverage is complete for every modernized default
class/spec family.

The remaining gear-template work is now:

1. stage `1-4` curated endgame progression sets for the 50-hour level-80 life
2. spot-check harness validation of representative stage-0 assignments in the
   live world-bot materialization path
3. Druid stage-0 templates after Druid doctrine modernization replaces the
   remaining legacy family

## Current Live Stage-0 Coverage

- Paladin: complete
- Warrior: complete
- Death Knight: complete
- Hunter: complete
- Warlock: complete
- Rogue: complete
- Shaman: complete
- Priest: complete
- Mage: complete

## Schema Support

Race-aware staged-template support now exists through:

- [D:\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_060_assigned_gear_template_race_mask.sql](D:/src/azerothcore-wotlk/data/sql/updates/pending_db_world/rev_living_world_060_assigned_gear_template_race_mask.sql)

This allows faction/race-paired catch-up items to coexist with shared neutral
fallback rows in the same stage template.
