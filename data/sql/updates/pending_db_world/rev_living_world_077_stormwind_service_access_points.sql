-- Stormwind access-style routing points for service clusters.
-- These are intentionally reachable approach spots, not exact NPC positions.

INSERT INTO living_world_task_point (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('stormwind_bank_access',          1519, 0, 'bank_access',          'Stormwind Bank Access',          -8916.20, 622.80, 99.50),
    ('stormwind_auction_house_access', 1519, 0, 'auction_house_access', 'Stormwind Auction House Access', -8811.90, 667.30, 97.90)
ON DUPLICATE KEY UPDATE
    zone_id = VALUES(zone_id),
    map_id = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x = VALUES(x),
    y = VALUES(y),
    z = VALUES(z);
