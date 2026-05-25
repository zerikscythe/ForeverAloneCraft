# WotLK Doctrine Modernization Backlog

This file maps the current WotLK Classic class/spec guide surface to the
world-bot doctrine/talent work that exists in this repo today.

Primary external source hub:
- `https://www.icy-veins.com/wotlk-classic/class-guides`

Current modernization rule of thumb:
- `modernized`
  - profile was rebuilt around current WotLK Classic guide material
  - usually has a dedicated reset migration with class-specific profile rows
  - usually has refreshed talent templates and/or modernized role targeting
- `partial`
  - some targeted upgrade exists, but the full class/spec family has not been
    reauthored end to end, or a major class-defining runtime dependency is still
    missing
- `legacy`
  - still depends on the early starter/default doctrine scaffolding from
    `rev_living_world_003/004/005/007/027`

## Current status

Note:
- legacy profile IDs below remain useful as rewrite targets, but the old
  starter/default live rows were pruned in
  `data/sql/updates/pending_db_world/rev_living_world_049_legacy_default_profile_cleanup.sql`
  so we only keep the fresh-generation doctrine families in the live default
  profile set.

| Class | Spec | Role | Profile ID | Talent Template | Status | Notes |
|---|---|---:|---:|---:|---|---|
| Death Knight | Blood | TANK | 14 | 11 | modernized | Rebuilt in `047` |
| Death Knight | Frost | DPS | 34 | 23 | modernized | Rebuilt in `047` |
| Death Knight | Unholy | DPS | 6 | 12 | modernized | Rebuilt in `047` |
| Druid | Balance | DPS | 10 | 17 | legacy | Original starter DPS profile; source pack is staged in `tools/lw-editor/data/icy_veins_wotlk_druid_source.json` |
| Druid | Feral | TANK | 15 | 18 | legacy | Original bear profile from `005`; role-specific glyph/gear source data is staged, but later runtime work should distinguish tank vs DPS feral with `loadout_key` or role-aware templates |
| Druid | Feral | DPS | 30 | none dedicated | legacy | Added in `027`, no modern reset yet; source pack is staged in `tools/lw-editor/data/icy_veins_wotlk_druid_source.json` |
| Druid | Restoration | HEAL | 12 | 19 | legacy | Class-specific split exists in `007`; source pack is staged in `tools/lw-editor/data/icy_veins_wotlk_druid_source.json` |
| Hunter | Beast Mastery | DPS | 3 | 7 | modernized | Rebuilt in `057`; first-pass world-bot permanent pet maintenance/control now proven |
| Hunter | Marksmanship | DPS | 20 | 27 | modernized | Rebuilt in `057`; first-pass world-bot permanent pet maintenance/control now proven |
| Hunter | Survival | DPS | 21 | 28 | modernized | Rebuilt in `057`; first-pass world-bot permanent pet maintenance/control now proven |
| Mage | Arcane | DPS | 26 | 20 | modernized | Rebuilt in `051` |
| Mage | Fire | DPS | 27 | 26 | modernized | Rebuilt in `051` |
| Mage | Frost | DPS | 8 | 15 | modernized | Rebuilt in `051` |
| Paladin | Holy | HEAL | 11 | 4 | modernized | Rebuilt in `033`, tuned in `034/039` |
| Paladin | Protection | TANK | 35 | 5 | modernized | Rebuilt in `033`, tuned in `041/042/043` |
| Paladin | Retribution | DPS | 2 | 6 | modernized | Rebuilt in `033`, tuned in `036` |
| Priest | Discipline | HEAL | 24 | 25 | modernized | Rebuilt in `050` |
| Priest | Holy | HEAL | 31 | 9 | modernized | Rebuilt in `050` |
| Priest | Shadow | DPS | 5 | 10 | modernized | Rebuilt in `050` |
| Rogue | Assassination | DPS | 22 | 21 | modernized | Rebuilt in `044`, loadout tiers in `045` |
| Rogue | Combat | DPS | 4 | 8 | modernized | Rebuilt in `044`, loadout tiers in `045` |
| Rogue | Subtlety | DPS | 23 | 22 | modernized | Rebuilt in `044`, loadout tiers in `045` |
| Shaman | Elemental | DPS | 7 | 13 | modernized | Rebuilt in `048` |
| Shaman | Enhancement | DPS | 25 | 24 | modernized | Rebuilt in `048` |
| Shaman | Restoration | HEAL | 33 | 14 | modernized | Rebuilt in `048` |
| Warlock | Affliction | DPS | 9 | 16 | modernized | Rebuilt in `056`; first-pass demon maintenance/control now proven |
| Warlock | Demonology | DPS | 28 | 29 | modernized | Rebuilt in `056`; first-pass demon maintenance/control now proven |
| Warlock | Destruction | DPS | 29 | 30 | modernized | Rebuilt in `056`; first-pass demon maintenance/control now proven |
| Warrior | Arms | DPS | 1 | 1 | modernized | Rebuilt in `046` |
| Warrior | Fury | DPS | 19 | 2 | modernized | Rebuilt in `046` |
| Warrior | Protection | TANK | 13 | 3 | modernized | Rebuilt in `046` |

## Source map

Use the Icy Veins WotLK Classic class hub to locate the current per-spec guide
pages before rewriting doctrine:

- Death Knight: Blood / Frost / Unholy
- Druid: Balance / Feral (DPS) / Feral (Tank) / Restoration
- Hunter: Beast Mastery / Marksmanship / Survival
- Mage: Arcane / Fire / Frost
- Paladin: Holy / Protection / Retribution
- Priest: Discipline / Holy / Shadow
- Rogue: Assassination / Combat / Subtlety
- Shaman: Elemental / Enhancement / Restoration
- Warlock: Affliction / Demonology / Destruction
- Warrior: Arms / Fury / Protection

## Existing modernization anchors

Use these files as the pattern for future rewrites:

- Paladin family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_033_paladin_pve_doctrine_reset.sql`
- Rogue family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_044_rogue_pve_doctrine_reset.sql`
- Rogue virtual loadout tiers:
  - `data/sql/updates/pending_db_world/rev_living_world_045_rogue_virtual_loadout_tiers.sql`
- Warrior family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_046_warrior_pve_doctrine_reset.sql`
- Death Knight family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_047_death_knight_pve_doctrine_reset.sql`
- Shaman family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_048_shaman_pve_doctrine_reset.sql`
- Priest family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_050_priest_pve_doctrine_reset.sql`
- Mage family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_051_mage_pve_doctrine_reset.sql`
- Warlock family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_056_warlock_pve_doctrine_reset.sql`
- Hunter family reset:
  - `data/sql/updates/pending_db_world/rev_living_world_057_hunter_pve_doctrine_reset.sql`

## Recommended rewrite order

This is the best next-pass order if we keep replacing the remaining legacy
starter profiles and then polishing the already-modern families:

1. Druid
   - Balance / Feral DPS / Feral Tank / Restoration
   - last full legacy family in the default-class set
   - source bundle now staged in `tools/lw-editor/data/icy_veins_wotlk_druid_source.json`
2. Hunter refinement
   - trap positioning
   - pet spell/autocast finesse
   - higher-order ranged uptime polish
3. Warlock refinement
   - summon choice
   - demon spell/autocast finesse
   - proc-driven polish

## Practical rewrite checklist

For each class family rewrite:

1. Gather source pages from the WotLK Classic Icy Veins guide hub.
2. Compare guide rotation/cooldown priorities against current profile rows.
3. Replace the old default profile rows with class-specific `PvE` profiles.
4. Refresh or add missing talent templates for each spec.
5. Verify the virtual loadout tiers for the class family, especially at level 80.
6. Spawn at least one world bot of each rewritten spec and inspect:
   - `build_prepared`
   - `resource_snapshot`
   - `weapon_loadout_snapshot` (if weapon class matters)
7. Only then tune itemized extras like poisons, on-use trinkets, or proc hooks.
