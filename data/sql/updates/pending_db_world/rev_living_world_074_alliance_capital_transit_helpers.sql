-- rev_living_world_074_alliance_capital_transit_helpers (world DB)
--
-- Adds a small transit-helper wave for Alliance capitals:
-- - Stormwind Harbor approach points near the harbor walk/ship area
-- - Rut'theran Village travel helpers for flight, ferry dock, and portal
-- - Darnassus-side portal endpoint for the Rut'theran tree portal

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('stormwind_harbor_walk_s',            1519, 0, 'harbor_access', 'Stormwind Harbor South Walk',            -8567.89,   980.578,   96.5265),
    ('stormwind_harbor_walk_n',            1519, 0, 'harbor_access', 'Stormwind Harbor North Walk',            -8581.44,   997.638,   96.3734),
    ('stormwind_harbor_ship_northrend',    1519, 0, 'boat_access',   'Stormwind Harbor Northrend Ship Access', -8645.00,  1038.200,   95.2000),

    ('ruttheran_taxi',                      141, 1, 'taxi',          'Rut''theran Hippogryph Master',           8640.58,   841.118,   23.3464),
    ('ruttheran_ferry_dock_nw',             141, 1, 'dock',          'Rut''theran Northwest Ferry Dock',        8657.60,   969.568,    2.0663),
    ('ruttheran_portal_to_darnassus',       141, 1, 'portal',        'Rut''theran Portal to Darnassus',         8786.36,   967.445,   30.1970),
    ('darnassus_portal_to_ruttheran',      1657, 1, 'portal',        'Darnassus Portal to Rut''theran',         9945.13,  2616.890, 1316.4600)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);
