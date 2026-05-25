-- rev_living_world_075_ruttheran_auberdine_transit (world DB)
--
-- Adds the Auberdine side of the Rut'theran travel cluster so Alliance
-- Kalimdor transit has a real ferry/taxi partner.

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('auberdine_taxi',              148, 1, 'taxi', 'Auberdine Hippogryph Master', 6343.20,  561.651, 16.1047),
    ('auberdine_boat_teldrassil',   148, 1, 'boat', 'Auberdine Boat to Teldrassil',6509.15,  799.073,  8.2293)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);

INSERT INTO living_world_taxi_route
    (route_key, source_point_key, dest_point_key, required_faction, duration_sec, display_name)
VALUES
    ('ruttheran_to_auberdine', 'ruttheran_taxi', 'auberdine_taxi', 1, 75, 'Fly to Auberdine'),
    ('auberdine_to_ruttheran', 'auberdine_taxi', 'ruttheran_taxi', 1, 75, 'Fly to Rut''theran')
ON DUPLICATE KEY UPDATE
    required_faction = VALUES(required_faction),
    duration_sec     = VALUES(duration_sec),
    display_name     = VALUES(display_name);

INSERT INTO living_world_transit_route
    (route_key, source_point_key, dest_point_key, transit_type, required_faction, min_level, max_level, duration_sec, display_name)
VALUES
    ('ruttheran_to_auberdine_taxi',  'ruttheran_taxi',            'auberdine_taxi',            'taxi', 1, 1, 80, 75,  'Fly to Auberdine'),
    ('auberdine_to_ruttheran_taxi',  'auberdine_taxi',            'ruttheran_taxi',            'taxi', 1, 1, 80, 75,  'Fly to Rut''theran'),
    ('ruttheran_to_auberdine_boat',  'ruttheran_ferry_dock_nw',   'auberdine_boat_teldrassil', 'boat', 1, 1, 80, 120, 'Boat to Auberdine'),
    ('auberdine_to_ruttheran_boat',  'auberdine_boat_teldrassil', 'ruttheran_ferry_dock_nw',   'boat', 1, 1, 80, 120, 'Boat to Rut''theran')
ON DUPLICATE KEY UPDATE
    transit_type      = VALUES(transit_type),
    required_faction  = VALUES(required_faction),
    min_level         = VALUES(min_level),
    max_level         = VALUES(max_level),
    duration_sec      = VALUES(duration_sec),
    display_name      = VALUES(display_name);
