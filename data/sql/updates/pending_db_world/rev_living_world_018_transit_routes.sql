-- rev_living_world_018_transit_routes (world DB)
--
-- Generalized authored transit network for taxi, boat, zeppelin, and portal travel.

CREATE TABLE IF NOT EXISTS living_world_transit_route (
    route_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    route_key         VARCHAR(64)       NOT NULL,
    source_point_key  VARCHAR(64)       NOT NULL,
    dest_point_key    VARCHAR(64)       NOT NULL,
    transit_type      VARCHAR(16)       NOT NULL DEFAULT 'taxi',
    required_faction  TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    min_level         TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level         TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    duration_sec      INT UNSIGNED      NOT NULL DEFAULT 60,
    display_name      VARCHAR(100)      NOT NULL,
    UNIQUE KEY uq_route_key (route_key),
    UNIQUE KEY uq_route_pair (source_point_key, dest_point_key, transit_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO living_world_transit_route
    (route_key, source_point_key, dest_point_key, transit_type, required_faction, min_level, max_level, duration_sec, display_name)
SELECT route_key, source_point_key, dest_point_key, 'taxi', required_faction, 1, 80, duration_sec, display_name
FROM living_world_taxi_route
ON DUPLICATE KEY UPDATE
    required_faction = VALUES(required_faction),
    min_level = VALUES(min_level),
    max_level = VALUES(max_level),
    duration_sec = VALUES(duration_sec),
    display_name = VALUES(display_name);

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('booty_bay_boat',               35,   0,   'boat',     'Booty Bay Boat',                 -14284.2, 551.2, 8.9),
    ('ratchet_boat',                392,  1,   'boat',     'Ratchet Boat',                    -995.5, -3823.4, 5.5),
    ('orgrimmar_zeppelin_tirisfal', 1637, 1,   'zeppelin', 'Orgrimmar Zeppelin Tower',        1337.1, -4633.9, 54.1),
    ('undercity_zeppelin_durotar',  1497, 0,   'zeppelin', 'Undercity Zeppelin Tower',        2067.6, 274.9, 97.0),
    ('stormwind_portal_outland',    1519, 0,   'portal',   'Stormwind Portal to Outland',    -8960.4, 517.1, 96.3),
    ('orgrimmar_portal_outland',    1637, 1,   'portal',   'Orgrimmar Portal to Outland',     1818.4, -4416.2, -18.8),
    ('shattrath_arrival',           3703, 530, 'portal',   'Shattrath Arrival',               -1887.5, 5359.7, -12.4),
    ('stormwind_ship_northrend',    1519, 0,   'boat',     'Stormwind Northrend Ship',       -8645.0, 1038.2, 95.2),
    ('orgrimmar_zeppelin_northrend',1637, 1,   'zeppelin', 'Orgrimmar Northrend Zeppelin',    1354.9, -4641.0, 54.5),
    ('borean_arrival_alliance',     3537, 571, 'boat',     'Borean Tundra Harbor',            2274.8, 5178.3, 11.2),
    ('borean_arrival_horde',        3537, 571, 'zeppelin', 'Borean Tundra Warsong Arrival',   2824.4, 6174.2, 121.9)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);

INSERT INTO living_world_transit_route
    (route_key, source_point_key, dest_point_key, transit_type, required_faction, min_level, max_level, duration_sec, display_name)
VALUES
    ('booty_bay_to_ratchet', 'booty_bay_boat', 'ratchet_boat', 'boat', 0, 1, 80, 120, 'Boat to Kalimdor'),
    ('ratchet_to_booty_bay', 'ratchet_boat', 'booty_bay_boat', 'boat', 0, 1, 80, 120, 'Boat to Eastern Kingdoms'),
    ('orgrimmar_to_tirisfal_zeppelin', 'orgrimmar_zeppelin_tirisfal', 'undercity_zeppelin_durotar', 'zeppelin', 2, 1, 80, 90, 'Zeppelin to Eastern Kingdoms'),
    ('undercity_to_durotar_zeppelin', 'undercity_zeppelin_durotar', 'orgrimmar_zeppelin_tirisfal', 'zeppelin', 2, 1, 80, 90, 'Zeppelin to Kalimdor'),
    ('stormwind_to_outland_portal', 'stormwind_portal_outland', 'shattrath_arrival', 'portal', 1, 58, 80, 20, 'Portal to Outland'),
    ('orgrimmar_to_outland_portal', 'orgrimmar_portal_outland', 'shattrath_arrival', 'portal', 2, 58, 80, 20, 'Portal to Outland'),
    ('stormwind_to_northrend_ship', 'stormwind_ship_northrend', 'borean_arrival_alliance', 'boat', 1, 68, 80, 90, 'Ship to Northrend'),
    ('orgrimmar_to_northrend_zeppelin', 'orgrimmar_zeppelin_northrend', 'borean_arrival_horde', 'zeppelin', 2, 68, 80, 90, 'Zeppelin to Northrend'),
    ('shattrath_to_stormwind_portal', 'shattrath_arrival', 'stormwind_portal_outland', 'portal', 1, 58, 80, 20, 'Portal to Stormwind'),
    ('shattrath_to_orgrimmar_portal', 'shattrath_arrival', 'orgrimmar_portal_outland', 'portal', 2, 58, 80, 20, 'Portal to Orgrimmar'),
    ('borean_to_stormwind_ship', 'borean_arrival_alliance', 'stormwind_ship_northrend', 'boat', 1, 68, 80, 90, 'Ship to Stormwind'),
    ('borean_to_orgrimmar_zeppelin', 'borean_arrival_horde', 'orgrimmar_zeppelin_northrend', 'zeppelin', 2, 68, 80, 90, 'Zeppelin to Orgrimmar')
ON DUPLICATE KEY UPDATE
    required_faction = VALUES(required_faction),
    min_level = VALUES(min_level),
    max_level = VALUES(max_level),
    duration_sec = VALUES(duration_sec),
    display_name = VALUES(display_name);