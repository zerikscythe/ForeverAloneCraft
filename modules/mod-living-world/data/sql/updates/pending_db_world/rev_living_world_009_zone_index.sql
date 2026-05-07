-- living_world_zone_index (acore_world)
-- One row per zone the ambient / guild bot system can use.
-- anchor_* is a safe spawn point inside the zone used as the travel destination.

CREATE TABLE IF NOT EXISTS living_world_zone_index (
    zone_id      INT UNSIGNED      NOT NULL PRIMARY KEY,
    map_id       SMALLINT UNSIGNED NOT NULL,
    zone_name    VARCHAR(64)       NOT NULL,
    faction      TINYINT UNSIGNED  NOT NULL DEFAULT 0, -- 0=both 1=alliance 2=horde
    zone_type    VARCHAR(32)       NOT NULL,            -- city|wilderness|contested
    has_herbs    TINYINT(1)        NOT NULL DEFAULT 0,
    has_ore      TINYINT(1)        NOT NULL DEFAULT 0,
    has_fish     TINYINT(1)        NOT NULL DEFAULT 0,
    min_level    TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level    TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    anchor_x     FLOAT             NOT NULL DEFAULT 0,
    anchor_y     FLOAT             NOT NULL DEFAULT 0,
    anchor_z     FLOAT             NOT NULL DEFAULT 0,
    notes        VARCHAR(255)      NULL,
    KEY idx_faction_type (faction, zone_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Zones eligible for ambient/guild bot activities';

-- ── Seed data ────────────────────────────────────────────────────────────────
-- Map 0 – Eastern Kingdoms
INSERT INTO living_world_zone_index
    (zone_id, map_id, zone_name, faction, zone_type, has_herbs, has_ore, has_fish, min_level, max_level, anchor_x, anchor_y, anchor_z)
VALUES
    (85,   0, 'Tirisfal Glades',      2, 'wilderness', 1, 0, 1,  1, 15,  2278.4, 277.0,  34.2),
    (130,  0, 'Silverpine Forest',    2, 'wilderness', 1, 0, 1, 10, 20, -278.0, -709.0,  34.0),
    (267,  0, 'Hillsbrad Foothills',  2, 'wilderness', 1, 1, 1, 20, 30, -966.0, -486.0,  54.2),
    (45,   0, 'Arathi Highlands',     0, 'contested',  1, 1, 1, 30, 40,-1278.0, -211.0,  28.3),
    (1497, 0, 'Undercity',            2, 'city',       0, 0, 0,  1, 80, 1565.7,  239.6, -62.0),
    (46,   0, 'Burning Steppes',      0, 'contested',  0, 1, 0, 50, 58, -7444.9,-1049.4, 225.0),
    (139,  0, 'Eastern Plaguelands',  0, 'wilderness', 1, 0, 1, 53, 60, 2809.7,-2697.0, 151.0),
-- Map 1 – Kalimdor
    (14,   1, 'Durotar',              2, 'wilderness', 1, 0, 1,  1, 10, -525.1,-4264.0,  40.0),
    (1519, 1, 'Orgrimmar',            2, 'city',       0, 0, 0,  1, 80, 1676.8, -4416.6,  62.1),
    (17,   1, 'The Barrens',          2, 'wilderness', 1, 1, 1, 10, 25,-1191.0,-2854.4,  92.7),
    (215,  1, 'Mulgore',              2, 'wilderness', 1, 1, 1,  1, 10,-2857.1,-179.3,  52.4),
    (406,  1, 'Stonetalon Mountains', 2, 'wilderness', 1, 1, 0, 15, 27,-450.0,-2576.0  ,84.0),
    (331,  1, 'Ashenvale',            0, 'contested',  1, 0, 1, 18, 30, 2283.5,-2175.7, 196.1),
    (357,  1, 'Feralas',              0, 'contested',  1, 0, 1, 40, 50,-4648.0,-1009.0,  44.9),
    (1377, 1, 'Silithus',             0, 'wilderness', 1, 0, 0, 55, 60,-6804.4,  816.8,  9.6),
-- Map 530 – Outland
    (3483,530, 'Hellfire Peninsula',  0, 'contested',  1, 1, 0, 58, 63,-367.0, 970.0, 45.2),
    (3518,530, 'Nagrand',             0, 'wilderness', 1, 0, 1, 64, 67,-1228.6, 7312.4, -3.7),
    (3523,530, 'Shadowmoon Valley',   0, 'wilderness', 0, 1, 0, 67, 70,-3357.0, 3331.0, 171.3),
-- Map 571 – Northrend
    (495, 571, 'Howling Fjord',       0, 'wilderness', 1, 0, 1, 68, 72, 590.5,-5069.0, 241.9),
    (394, 571, 'Grizzly Hills',       0, 'wilderness', 1, 1, 1, 73, 76, 3219.4,-2883.7, 194.3),
    (67,  571, 'Storm Peaks',         0, 'wilderness', 0, 1, 0, 77, 80, 6924.9, -1056.4, 960.8),
    (210, 571, 'Icecrown',            0, 'wilderness', 1, 1, 0, 77, 80, 5655.3, -5131.1, 824.8),
    (4395,571, 'Dalaran',             0, 'city',       0, 0, 0,  1, 80, 5804.1, 624.8, 647.8)
ON DUPLICATE KEY UPDATE
    zone_name = VALUES(zone_name),
    faction   = VALUES(faction),
    zone_type = VALUES(zone_type),
    has_herbs = VALUES(has_herbs),
    has_ore   = VALUES(has_ore),
    has_fish  = VALUES(has_fish),
    min_level = VALUES(min_level),
    max_level = VALUES(max_level),
    anchor_x  = VALUES(anchor_x),
    anchor_y  = VALUES(anchor_y),
    anchor_z  = VALUES(anchor_z);
