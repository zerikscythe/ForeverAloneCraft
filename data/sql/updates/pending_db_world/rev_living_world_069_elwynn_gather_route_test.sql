-- rev_living_world_069_elwynn_gather_route_test (world DB)
--
-- Adds concrete Elwynn Forest herb/ore gather-route entry points and route-backed
-- zone content rows, plus a deterministic bundled test template:
-- Stormwind spawn -> gather herbs in Elwynn -> gather ore in Elwynn -> return to Stormwind AH.

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('elwynn_herb_route_1_entry', 12, 0, 'route_entry', 'Elwynn Herb Route 01 Entry', -9820.7014,   862.7955, 25.7979),
    ('elwynn_herb_route_2_entry', 12, 0, 'route_entry', 'Elwynn Herb Route 02 Entry', -9520.2590,   -23.2458, 56.5388),
    ('elwynn_herb_route_3_entry', 12, 0, 'route_entry', 'Elwynn Herb Route 03 Entry', -9614.9376, -1125.5065, 42.5689),
    ('elwynn_ore_route_1_entry',  12, 0, 'route_entry', 'Elwynn Ore Route 01 Entry',  -9820.7014,   862.7955, 25.7979),
    ('elwynn_ore_route_2_entry',  12, 0, 'route_entry', 'Elwynn Ore Route 02 Entry',  -9520.2590,   -23.2458, 56.5388),
    ('elwynn_ore_route_3_entry',  12, 0, 'route_entry', 'Elwynn Ore Route 03 Entry',  -9590.9339,  -935.7448, 43.6258)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);

INSERT INTO living_world_zone_content
    (zone_id, content_kind, subject_id, subject_key, display_name, required_faction,
     min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes)
VALUES
    (12, 'herb', NULL, 'ElwynnForest_Herb_01', 'Elwynn Herb Route 01', 1, 1, 80, 0, 0, 3, 'elwynn_herb_route_1_entry', 'town',
        'Route-backed herb loop: Peacebloom, Silverleaf, Earthroot, Mageroyal, Briarthorn, Bruiseweed'),
    (12, 'herb', NULL, 'ElwynnForest_Herb_02', 'Elwynn Herb Route 02', 1, 1, 80, 0, 0, 3, 'elwynn_herb_route_2_entry', 'town',
        'Route-backed herb loop: Peacebloom, Silverleaf, Earthroot, Mageroyal, Briarthorn, Bruiseweed'),
    (12, 'herb', NULL, 'ElwynnForest_Herb_03', 'Elwynn Herb Route 03', 1, 1, 80, 0, 0, 3, 'elwynn_herb_route_3_entry', 'town',
        'Route-backed herb loop: Peacebloom, Silverleaf, Earthroot, Mageroyal, Briarthorn, Bruiseweed'),
    (12, 'ore',  NULL, 'ElwynnForest_Ore_01',  'Elwynn Ore Route 01',  1, 1, 80, 0, 0, 3, 'elwynn_ore_route_1_entry',  'town',
        'Route-backed ore loop: Copper Ore, Tin Ore, Silver Ore'),
    (12, 'ore',  NULL, 'ElwynnForest_Ore_02',  'Elwynn Ore Route 02',  1, 1, 80, 0, 0, 3, 'elwynn_ore_route_2_entry',  'town',
        'Route-backed ore loop: Copper Ore, Tin Ore, Silver Ore'),
    (12, 'ore',  NULL, 'ElwynnForest_Ore_03',  'Elwynn Ore Route 03',  1, 1, 80, 0, 0, 3, 'elwynn_ore_route_3_entry',  'town',
        'Route-backed ore loop: Copper Ore, Tin Ore, Silver Ore')
ON DUPLICATE KEY UPDATE
    subject_id        = VALUES(subject_id),
    subject_key       = VALUES(subject_key),
    required_faction  = VALUES(required_faction),
    min_level         = VALUES(min_level),
    max_level         = VALUES(max_level),
    min_skill         = VALUES(min_skill),
    max_skill         = VALUES(max_skill),
    weight            = VALUES(weight),
    anchor_point_key  = VALUES(anchor_point_key),
    return_anchor_role = VALUES(return_anchor_role),
    notes             = VALUES(notes);

INSERT INTO living_world_task_template
    (template_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('debug_elwynn_dual_gather_sw_ah', 'Elwynn Herb and Ore Test Run', 'gathering', 1, 80, 80, 1, 1, 0, 10, 1)
ON DUPLICATE KEY UPDATE
    display_name       = VALUES(display_name),
    task_family        = VALUES(task_family),
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    requires_herbalism = VALUES(requires_herbalism),
    requires_mining    = VALUES(requires_mining),
    requires_fishing   = VALUES(requires_fishing),
    weight             = VALUES(weight),
    is_enabled         = VALUES(is_enabled);

DELETE s
FROM living_world_task_template_step s
JOIN living_world_task_template t ON t.template_id = s.template_id
WHERE t.template_key = 'debug_elwynn_dual_gather_sw_ah';

INSERT INTO living_world_task_template_step
    (template_id, step_order, step_type, target_zone_id, target_point_key,
     resolver_kind, subject_kind, subject_id, subject_key, return_anchor_role, cycle_count,
     duration_min_sec, duration_max_sec, label)
SELECT t.template_id, 1, 'gather_herb', 12, NULL,
       'resource_zone', 'herb', NULL, NULL, 'town', 1,
       90, 90, 'Gather herbs in Elwynn Forest'
FROM living_world_task_template t WHERE t.template_key = 'debug_elwynn_dual_gather_sw_ah'
UNION ALL
SELECT t.template_id, 2, 'gather_ore', 12, NULL,
       'resource_zone', 'ore', NULL, NULL, 'town', 1,
       90, 90, 'Gather ore in Elwynn Forest'
FROM living_world_task_template t WHERE t.template_key = 'debug_elwynn_dual_gather_sw_ah'
UNION ALL
SELECT t.template_id, 3, 'idle_city', 1519, 'stormwind_ah',
       'point', 'city_service', NULL, NULL, 'town', 1,
       90, 90, 'Use the Stormwind Auction House'
FROM living_world_task_template t WHERE t.template_key = 'debug_elwynn_dual_gather_sw_ah';
