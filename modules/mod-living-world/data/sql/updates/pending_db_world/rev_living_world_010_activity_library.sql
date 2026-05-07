-- living_world_activity_library (acore_world)
-- One row per activity template ambient and guild bots can be assigned.
-- The session composer picks from this table based on bot faction/level/skills.

CREATE TABLE IF NOT EXISTS living_world_activity_library (
    activity_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    activity_key         VARCHAR(64)       NOT NULL,
    display_name         VARCHAR(64)       NOT NULL,
    activity_type        VARCHAR(32)       NOT NULL, -- patrol|gather_herb|gather_ore|fish|idle_city|idle_inn
    target_zone_id       INT UNSIGNED      NOT NULL,
    required_faction     TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    min_level            TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level            TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    requires_herbalism   TINYINT(1)        NOT NULL DEFAULT 0,
    requires_mining      TINYINT(1)        NOT NULL DEFAULT 0,
    requires_fishing     TINYINT(1)        NOT NULL DEFAULT 0,
    weight               TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    duration_min_sec     INT UNSIGNED      NOT NULL DEFAULT 600,
    duration_max_sec     INT UNSIGNED      NOT NULL DEFAULT 1800,
    UNIQUE KEY uq_key (activity_key),
    KEY idx_faction_level (required_faction, min_level, max_level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Activity templates for ambient and guild bot sessions';

-- ── Seed data ────────────────────────────────────────────────────────────────
INSERT INTO living_world_activity_library
    (activity_key, display_name, activity_type, target_zone_id,
     required_faction, min_level, max_level,
     requires_herbalism, requires_mining, requires_fishing,
     weight, duration_min_sec, duration_max_sec)
VALUES
-- City idle activities (any bot, any level)
    ('idle_orgrimmar',        'Idle in Orgrimmar',        'idle_city',    1519, 2,  1, 80, 0, 0, 0, 3,  600, 1800),
    ('idle_undercity',        'Idle in Undercity',        'idle_city',    1497, 2,  1, 80, 0, 0, 0, 2,  600, 1800),
-- Low-level horde wilderness patrol
    ('patrol_durotar',        'Patrol Durotar',           'patrol',         14, 2,  1, 12, 0, 0, 0, 2,  900, 1800),
    ('patrol_barrens',        'Patrol The Barrens',       'patrol',         17, 2, 10, 25, 0, 0, 0, 2,  900, 1800),
    ('patrol_mulgore',        'Patrol Mulgore',           'patrol',        215, 2,  1, 12, 0, 0, 0, 2,  900, 1800),
-- Herb gathering
    ('herb_run_tirisfal',     'Herb Run – Tirisfal',      'gather_herb',    85, 2,  1, 15, 1, 0, 0, 2,  900, 1800),
    ('herb_run_silverpine',   'Herb Run – Silverpine',    'gather_herb',   130, 2, 10, 20, 1, 0, 0, 2,  900, 1800),
    ('herb_run_hillsbrad',    'Herb Run – Hillsbrad',     'gather_herb',   267, 2, 20, 30, 1, 0, 0, 2,  900, 1800),
    ('herb_run_ashenvale',    'Herb Run – Ashenvale',     'gather_herb',   331, 0, 18, 30, 1, 0, 0, 2,  900, 1800),
    ('herb_run_feralas',      'Herb Run – Feralas',       'gather_herb',   357, 0, 40, 50, 1, 0, 0, 2, 1200, 2400),
    ('herb_run_epl',          'Herb Run – E.Plaguelands', 'gather_herb',   139, 0, 53, 60, 1, 0, 0, 2, 1200, 2400),
    ('herb_run_howling_fjord','Herb Run – Howling Fjord', 'gather_herb',   495, 0, 68, 75, 1, 0, 0, 2, 1200, 2400),
    ('herb_run_grizzly_hills','Herb Run – Grizzly Hills', 'gather_herb',   394, 0, 73, 78, 1, 0, 0, 2, 1200, 2400),
    ('herb_run_icecrown',     'Herb Run – Icecrown',      'gather_herb',   210, 0, 77, 80, 1, 0, 0, 2, 1200, 2400),
-- Ore mining
    ('mine_hillsbrad',        'Mine Hillsbrad',           'gather_ore',    267, 2, 20, 30, 0, 1, 0, 2,  900, 1800),
    ('mine_arathi',           'Mine Arathi Highlands',    'gather_ore',     45, 0, 30, 40, 0, 1, 0, 2,  900, 1800),
    ('mine_burning_steppes',  'Mine Burning Steppes',     'gather_ore',     46, 0, 50, 58, 0, 1, 0, 2, 1200, 2400),
    ('mine_grizzly_hills',    'Mine Grizzly Hills',       'gather_ore',    394, 0, 73, 78, 0, 1, 0, 2, 1200, 2400),
    ('mine_storm_peaks',      'Mine Storm Peaks',         'gather_ore',     67, 0, 77, 80, 0, 1, 0, 2, 1200, 2400),
-- Fishing
    ('fish_barrens',          'Fish The Barrens',         'fish',           17, 2, 10, 25, 0, 0, 1, 1,  900, 1800),
    ('fish_feralas',          'Fish Feralas',             'fish',          357, 0, 40, 50, 0, 0, 1, 1, 1200, 2400),
    ('fish_grizzly_hills',    'Fish Grizzly Hills',       'fish',          394, 0, 73, 78, 0, 0, 1, 1, 1200, 2400)
ON DUPLICATE KEY UPDATE
    display_name         = VALUES(display_name),
    activity_type        = VALUES(activity_type),
    target_zone_id       = VALUES(target_zone_id),
    required_faction     = VALUES(required_faction),
    min_level            = VALUES(min_level),
    max_level            = VALUES(max_level),
    requires_herbalism   = VALUES(requires_herbalism),
    requires_mining      = VALUES(requires_mining),
    requires_fishing     = VALUES(requires_fishing),
    weight               = VALUES(weight),
    duration_min_sec     = VALUES(duration_min_sec),
    duration_max_sec     = VALUES(duration_max_sec);
