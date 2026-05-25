-- rev_living_world_017_taxi_routes (world DB)
--
-- Authorable taxi nodes/routes for long-distance task travel.

CREATE TABLE IF NOT EXISTS living_world_taxi_route (
    route_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    route_key         VARCHAR(64)       NOT NULL,
    source_point_key  VARCHAR(64)       NOT NULL,
    dest_point_key    VARCHAR(64)       NOT NULL,
    required_faction  TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    duration_sec      INT UNSIGNED      NOT NULL DEFAULT 60,
    display_name      VARCHAR(100)      NOT NULL,
    UNIQUE KEY uq_route_key (route_key),
    UNIQUE KEY uq_route_pair (source_point_key, dest_point_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('stormwind_taxi',   1519, 0,   'taxi', 'Stormwind Flight Master',   -8834.1, 493.5, 109.6),
    ('ironforge_taxi',   1537, 0,   'taxi', 'Ironforge Flight Master',   -4821.5, -1152.3, 502.2),
    ('exodar_taxi',      3557, 530, 'taxi', 'Exodar Flight Master',      -4057.2, -11788.6,   8.9),
    ('orgrimmar_taxi',   1637, 1,   'taxi', 'Orgrimmar Flight Master',   1676.3, -4315.7, 61.8),
    ('thunderbluff_taxi',1638, 1,   'taxi', 'Thunder Bluff Flight Master', -1196.1, 26.1, 176.9)
ON DUPLICATE KEY UPDATE
    zone_id = VALUES(zone_id),
    map_id = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x = VALUES(x),
    y = VALUES(y),
    z = VALUES(z);

INSERT INTO living_world_taxi_route
    (route_key, source_point_key, dest_point_key, required_faction, duration_sec, display_name)
VALUES
    ('stormwind_to_ironforge', 'stormwind_taxi', 'ironforge_taxi', 1, 95, 'Fly to Ironforge'),
    ('ironforge_to_stormwind', 'ironforge_taxi', 'stormwind_taxi', 1, 95, 'Fly to Stormwind'),
    ('orgrimmar_to_thunderbluff', 'orgrimmar_taxi', 'thunderbluff_taxi', 2, 105, 'Fly to Thunder Bluff'),
    ('thunderbluff_to_orgrimmar', 'thunderbluff_taxi', 'orgrimmar_taxi', 2, 105, 'Fly to Orgrimmar')
ON DUPLICATE KEY UPDATE
    required_faction = VALUES(required_faction),
    duration_sec = VALUES(duration_sec),
    display_name = VALUES(display_name);
