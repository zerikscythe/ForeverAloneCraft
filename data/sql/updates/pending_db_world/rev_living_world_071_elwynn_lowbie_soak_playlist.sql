-- rev_living_world_071_elwynn_lowbie_soak_playlist (world DB)
--
-- Adds a deterministic low-level Alliance playlist for Stormwind -> Elwynn
-- soak testing. This bypasses the still-thin lowbie Alliance ambient activity
-- pool and gives the orchestrator a stable quest/gather/city loop to run.

INSERT INTO living_world_task_template
    (template_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('debug_elwynn_lowbie_quest',      'Elwynn Lowbie Questing',     'questing',  1,  1, 15, 0, 0, 0, 8, 1),
    ('debug_elwynn_lowbie_gather_herb','Elwynn Lowbie Herb Route',   'gathering', 1,  1, 15, 1, 0, 0, 4, 1),
    ('debug_elwynn_lowbie_gather_ore', 'Elwynn Lowbie Ore Route',    'gathering', 1,  1, 15, 0, 1, 0, 4, 1)
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
WHERE t.template_key IN (
    'debug_elwynn_lowbie_quest',
    'debug_elwynn_lowbie_gather_herb',
    'debug_elwynn_lowbie_gather_ore');

INSERT INTO living_world_task_template_step
    (template_id, step_order, step_type, target_zone_id, target_point_key,
     resolver_kind, subject_kind, subject_id, subject_key, return_anchor_role, cycle_count,
     duration_min_sec, duration_max_sec, label)
SELECT t.template_id, 1, 'grind', 12, NULL,
       'quest_zone', NULL, NULL, NULL, 'town', 1,
       180, 300, 'Quest in Elwynn Forest'
FROM living_world_task_template t
WHERE t.template_key = 'debug_elwynn_lowbie_quest'
UNION ALL
SELECT t.template_id, 1, 'gather_herb', 12, NULL,
       'resource_zone', 'herb', NULL, NULL, 'town', 1,
       90, 120, 'Gather herbs in Elwynn Forest'
FROM living_world_task_template t
WHERE t.template_key = 'debug_elwynn_lowbie_gather_herb'
UNION ALL
SELECT t.template_id, 1, 'gather_ore', 12, NULL,
       'resource_zone', 'ore', NULL, NULL, 'town', 1,
       90, 120, 'Gather ore in Elwynn Forest'
FROM living_world_task_template t
WHERE t.template_key = 'debug_elwynn_lowbie_gather_ore';

INSERT INTO living_world_playlist
    (playlist_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('debug_elwynn_lowbie_swarm_day', 'Stormwind to Elwynn Lowbie Day', 'routine',
     1, 1, 15, 1, 1, 0, 10, 1)
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

DELETE e
FROM living_world_playlist_entry e
JOIN living_world_playlist p ON p.playlist_id = e.playlist_id
WHERE p.playlist_key = 'debug_elwynn_lowbie_swarm_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Quest at an Elwynn hub'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'debug_elwynn_lowbie_quest'
WHERE p.playlist_key = 'debug_elwynn_lowbie_swarm_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Herb loop in Elwynn'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'debug_elwynn_lowbie_gather_herb'
WHERE p.playlist_key = 'debug_elwynn_lowbie_swarm_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Quest at the next Elwynn hub'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'debug_elwynn_lowbie_quest'
WHERE p.playlist_key = 'debug_elwynn_lowbie_swarm_day'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'Ore loop in Elwynn'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'debug_elwynn_lowbie_gather_ore'
WHERE p.playlist_key = 'debug_elwynn_lowbie_swarm_day'
UNION ALL
SELECT p.playlist_id, 5, t.template_id, 1, 'Stormwind mailbox/AH/bank loop'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'debug_elwynn_lowbie_swarm_day';
