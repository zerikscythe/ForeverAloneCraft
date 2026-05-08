-- living_world_activity_library Tier 2 seed pass (acore_world)
-- Small expansion of DB-authored ambient/session tasks so chained Tier 2 bots
-- have better city, patrol, gathering, and contested-zone variety.
-- MySQL 8.0 compatible.

INSERT INTO living_world_activity_library
    (activity_key, display_name, activity_type, task_family, required_zone_type,
     max_per_session, target_zone_id, required_faction, min_level, max_level,
     requires_herbalism, requires_mining, requires_fishing,
     weight, duration_min_sec, duration_max_sec)
VALUES
-- Additional city errands / idles
    ('idle_inn_orgrimmar',         'Rest at Orgrimmar Inn',        'idle_inn',     'city_errand', 'city',       2, 1519, 2,  1, 80, 0, 0, 0, 3,  480, 1200),
    ('idle_inn_undercity',         'Rest at Undercity Inn',        'idle_inn',     'city_errand', 'city',       2, 1497, 2,  1, 80, 0, 0, 0, 2,  480, 1200),
    ('idle_dalaran',               'Idle in Dalaran',              'idle_city',    'city_errand', 'city',       2, 4395, 0, 68, 80, 0, 0, 0, 3,  600, 1500),

-- Wilderness / contested patrols for broader level coverage
    ('patrol_stonetalon',          'Patrol Stonetalon',            'patrol',       'patrol',      'wilderness', 1,  406, 2, 15, 27, 0, 0, 0, 2,  900, 1800),
    ('patrol_arathi',              'Patrol Arathi Highlands',      'patrol',       'patrol',      'contested',  1,   45, 0, 30, 40, 0, 0, 0, 2,  900, 1800),
    ('patrol_hellfire',            'Patrol Hellfire Peninsula',    'patrol',       'patrol',      'contested',  1, 3483, 0, 58, 63, 0, 0, 0, 2, 1200, 2100),
    ('patrol_nagrand',             'Patrol Nagrand',               'patrol',       'patrol',      'wilderness', 1, 3518, 0, 64, 67, 0, 0, 0, 2, 1200, 2100),
    ('patrol_shadowmoon',          'Patrol Shadowmoon Valley',     'patrol',       'patrol',      'wilderness', 1, 3523, 0, 67, 70, 0, 0, 0, 2, 1200, 2100),
    ('patrol_icecrown',            'Patrol Icecrown',              'patrol',       'patrol',      'wilderness', 1,  210, 0, 77, 80, 0, 0, 0, 2, 1200, 2400),

-- Additional gathering coverage
    ('herb_run_durotar',           'Herb Run – Durotar',           'gather_herb',  'gathering',   'wilderness', 1,   14, 2,  1, 10, 1, 0, 0, 2,  900, 1500),
    ('herb_run_mulgore',           'Herb Run – Mulgore',           'gather_herb',  'gathering',   'wilderness', 1,  215, 2,  1, 10, 1, 0, 0, 2,  900, 1500),
    ('herb_run_stonetalon',        'Herb Run – Stonetalon',        'gather_herb',  'gathering',   'wilderness', 1,  406, 2, 15, 27, 1, 0, 0, 2,  900, 1800),
    ('herb_run_nagrand',           'Herb Run – Nagrand',           'gather_herb',  'gathering',   'wilderness', 1, 3518, 0, 64, 67, 1, 0, 0, 2, 1200, 2100),
    ('mine_mulgore',               'Mine Mulgore',                 'gather_ore',   'gathering',   'wilderness', 1,  215, 2,  1, 10, 0, 1, 0, 2,  900, 1500),
    ('mine_stonetalon',            'Mine Stonetalon',              'gather_ore',   'gathering',   'wilderness', 1,  406, 2, 15, 27, 0, 1, 0, 2,  900, 1800),
    ('mine_hellfire',              'Mine Hellfire Peninsula',      'gather_ore',   'gathering',   'contested',  1, 3483, 0, 58, 63, 0, 1, 0, 2, 1200, 2100),
    ('mine_shadowmoon',            'Mine Shadowmoon Valley',       'gather_ore',   'gathering',   'wilderness', 1, 3523, 0, 67, 70, 0, 1, 0, 2, 1200, 2100),
    ('mine_icecrown',              'Mine Icecrown',                'gather_ore',   'gathering',   'wilderness', 1,  210, 0, 77, 80, 0, 1, 0, 2, 1200, 2400),

-- Additional fishing coverage
    ('fish_tirisfal',              'Fish Tirisfal Glades',         'fish',         'fishing',     'wilderness', 1,   85, 2,  1, 15, 0, 0, 1, 1,  900, 1500),
    ('fish_mulgore',               'Fish Mulgore',                 'fish',         'fishing',     'wilderness', 1,  215, 2,  1, 10, 0, 0, 1, 1,  900, 1500),
    ('fish_hillsbrad',             'Fish Hillsbrad',               'fish',         'fishing',     'wilderness', 1,  267, 2, 20, 30, 0, 0, 1, 1,  900, 1800),
    ('fish_nagrand',               'Fish Nagrand',                 'fish',         'fishing',     'wilderness', 1, 3518, 0, 64, 67, 0, 0, 1, 1, 1200, 2100),
    ('fish_howling_fjord',         'Fish Howling Fjord',           'fish',         'fishing',     'wilderness', 1,  495, 0, 68, 72, 0, 0, 1, 1, 1200, 2100)

ON DUPLICATE KEY UPDATE
    display_name         = VALUES(display_name),
    activity_type        = VALUES(activity_type),
    task_family          = VALUES(task_family),
    required_zone_type   = VALUES(required_zone_type),
    max_per_session      = VALUES(max_per_session),
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