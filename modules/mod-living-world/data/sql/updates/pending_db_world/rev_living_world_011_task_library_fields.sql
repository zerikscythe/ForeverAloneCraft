-- living_world_activity_library task-library expansion (acore_world)
-- Adds DB-authored metadata so Tier 2 session composition can evolve toward
-- explicit task-family and chaining rules without requiring C++ recompiles.

ALTER TABLE living_world_activity_library
    ADD COLUMN task_family VARCHAR(32) NOT NULL DEFAULT 'misc' AFTER activity_type,
    ADD COLUMN required_zone_type VARCHAR(32) NULL DEFAULT NULL AFTER task_family,
    ADD COLUMN max_per_session TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER required_zone_type;

-- Backfill task families from the existing runtime primitive.
UPDATE living_world_activity_library
SET task_family =
    CASE activity_type
        WHEN 'idle_city'   THEN 'city_errand'
        WHEN 'idle_inn'    THEN 'city_errand'
        WHEN 'patrol'      THEN 'patrol'
        WHEN 'gather_herb' THEN 'gathering'
        WHEN 'gather_ore'  THEN 'gathering'
        WHEN 'fish'        THEN 'fishing'
        ELSE 'misc'
    END
WHERE task_family IS NULL
   OR task_family = ''
   OR task_family = 'misc';

-- Backfill preferred zone-type filters from the linked zone row when possible.
UPDATE living_world_activity_library a
JOIN living_world_zone_index z
  ON z.zone_id = a.target_zone_id
SET a.required_zone_type =
    CASE
        WHEN a.required_zone_type IS NULL OR a.required_zone_type = '' THEN z.zone_type
        ELSE a.required_zone_type
    END;

-- Session repeat caps: allow broader city chaining while keeping most tasks unique.
UPDATE living_world_activity_library
SET max_per_session =
    CASE
        WHEN activity_type IN ('idle_city', 'idle_inn') THEN 2
        ELSE 1
    END
WHERE max_per_session = 0
   OR max_per_session IS NULL;

-- Refresh current seed rows with explicit authoring metadata.
UPDATE living_world_activity_library SET
    task_family = 'city_errand',
    required_zone_type = 'city',
    max_per_session = 2
WHERE activity_key IN ('idle_orgrimmar', 'idle_undercity');

UPDATE living_world_activity_library SET
    task_family = 'patrol',
    required_zone_type = 'wilderness',
    max_per_session = 1
WHERE activity_key IN ('patrol_durotar', 'patrol_barrens', 'patrol_mulgore');

UPDATE living_world_activity_library SET
    task_family = 'gathering',
    max_per_session = 1
WHERE activity_key IN (
    'herb_run_tirisfal',
    'herb_run_silverpine',
    'herb_run_hillsbrad',
    'herb_run_ashenvale',
    'herb_run_feralas',
    'herb_run_epl',
    'herb_run_howling_fjord',
    'herb_run_grizzly_hills',
    'herb_run_icecrown',
    'mine_hillsbrad',
    'mine_arathi',
    'mine_burning_steppes',
    'mine_grizzly_hills',
    'mine_storm_peaks'
);

UPDATE living_world_activity_library SET
    task_family = 'fishing',
    max_per_session = 1
WHERE activity_key IN ('fish_barrens', 'fish_feralas', 'fish_grizzly_hills');