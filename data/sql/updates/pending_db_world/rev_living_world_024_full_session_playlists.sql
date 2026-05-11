-- rev_living_world_024_full_session_playlists (world DB)
--
-- Expands authored ambient/world-bot routines into 25 full session playlists
-- built from reusable city-service and gathering task templates.

INSERT INTO living_world_task_template
    (template_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('shattrath_city_services', 'Shattrath City Services', 'city_errand', 0, 58, 80, 0, 0, 0, 2, 1)
ON DUPLICATE KEY UPDATE
    display_name     = VALUES(display_name),
    task_family      = VALUES(task_family),
    required_faction = VALUES(required_faction),
    min_level        = VALUES(min_level),
    max_level        = VALUES(max_level),
    requires_herbalism = VALUES(requires_herbalism),
    requires_mining    = VALUES(requires_mining),
    requires_fishing   = VALUES(requires_fishing),
    weight           = VALUES(weight),
    is_enabled       = VALUES(is_enabled);

DELETE s
FROM living_world_task_template_step s
JOIN living_world_task_template t ON t.template_id = s.template_id
WHERE t.template_key = 'shattrath_city_services';

INSERT INTO living_world_task_template_step
    (template_id, step_order, step_type, target_zone_id, target_point_key, duration_min_sec, duration_max_sec, label)
SELECT t.template_id, 1, 'idle_city', 3703, 'shattrath_mailbox', 20, 60, 'Check Mail in Shattrath'
FROM living_world_task_template t WHERE t.template_key = 'shattrath_city_services'
UNION ALL
SELECT t.template_id, 2, 'idle_city', 3703, 'shattrath_bank', 60, 180, 'Idle by Shattrath Bank'
FROM living_world_task_template t WHERE t.template_key = 'shattrath_city_services'
UNION ALL
SELECT t.template_id, 3, 'idle_inn', 3703, 'shattrath_inn', 120, 300, 'Rest at Shattrath Inn'
FROM living_world_task_template t WHERE t.template_key = 'shattrath_city_services';

INSERT INTO living_world_playlist
    (playlist_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('stormwind_city_errands_routine',   'Stormwind City Errands Routine',      'city_errand', 1,  1, 80, 0, 0, 0, 3, 1),
    ('orgrimmar_city_errands_routine',   'Orgrimmar City Errands Routine',      'city_errand', 2,  1, 80, 0, 0, 0, 3, 1),
    ('dalaran_city_errands_routine',     'Dalaran City Errands Routine',        'city_errand', 0, 68, 80, 0, 0, 0, 3, 1),
    ('stormwind_herbalism_day',          'Stormwind Herbalism Day',             'mixed',       1, 18, 30, 1, 0, 0, 2, 1),
    ('ashenvale_herb_circuit_long',      'Ashenvale Herb Circuit Long',         'gathering',   1, 18, 30, 1, 0, 0, 2, 1),
    ('alliance_apothecary_weekday',      'Alliance Apothecary Weekday',         'mixed',       1, 18, 30, 1, 0, 0, 2, 1),
    ('stormwind_herb_winddown',          'Stormwind Herb Winddown',             'mixed',       1, 18, 30, 1, 0, 0, 2, 1),
    ('silverpine_herbalism_day',         'Silverpine Herbalism Day',            'mixed',       2, 10, 20, 1, 0, 0, 2, 1),
    ('silverpine_herb_circuit_long',     'Silverpine Herb Circuit Long',        'gathering',   2, 10, 20, 1, 0, 0, 2, 1),
    ('hillsbrad_mining_day',             'Hillsbrad Mining Day',                'mixed',       2, 20, 30, 0, 1, 0, 2, 1),
    ('hillsbrad_ore_circuit_long',       'Hillsbrad Ore Circuit Long',          'gathering',   2, 20, 30, 0, 1, 0, 2, 1),
    ('orgrimmar_mining_winddown',        'Orgrimmar Mining Winddown',           'mixed',       2, 20, 30, 0, 1, 0, 2, 1),
    ('feralas_fishing_relaxed',          'Feralas Fishing Relaxed',             'fishing',     0, 40, 50, 0, 0, 1, 2, 1),
    ('feralas_fishing_marathon',         'Feralas Fishing Marathon',            'fishing',     0, 40, 50, 0, 0, 1, 2, 1),
    ('shattrath_city_services_routine',  'Shattrath City Services Routine',     'city_errand', 0, 58, 80, 0, 0, 0, 2, 1),
    ('outland_herbalism_day',            'Outland Herbalism Day',               'mixed',       0, 64, 67, 1, 0, 0, 2, 1),
    ('outland_fieldwork_marathon',       'Outland Fieldwork Marathon',          'gathering',   0, 64, 67, 1, 0, 0, 2, 1),
    ('alliance_outland_trade_run',       'Alliance Outland Trade Run',          'mixed',       1, 64, 67, 1, 0, 0, 2, 1),
    ('horde_outland_trade_run',          'Horde Outland Trade Run',             'mixed',       2, 64, 67, 1, 0, 0, 2, 1),
    ('northrend_mining_day',             'Northrend Mining Day',                'mixed',       0, 73, 78, 0, 1, 0, 2, 1),
    ('northrend_fishing_day',            'Northrend Fishing Day',               'mixed',       0, 73, 78, 0, 0, 1, 2, 1),
    ('northrend_dual_profession_day',    'Northrend Dual Profession Day',       'mixed',       0, 73, 78, 0, 1, 1, 2, 1),
    ('grizzly_hills_mining_loop',        'Grizzly Hills Mining Loop',           'gathering',   0, 73, 78, 0, 1, 0, 2, 1),
    ('grizzly_hills_fishing_loop',       'Grizzly Hills Fishing Loop',          'fishing',     0, 73, 78, 0, 0, 1, 2, 1),
    ('dalaran_gatherer_evening',         'Dalaran Gatherer Evening',            'mixed',       0, 73, 78, 0, 1, 1, 2, 1)
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
WHERE p.playlist_key IN (
    'stormwind_city_errands_routine',
    'orgrimmar_city_errands_routine',
    'dalaran_city_errands_routine',
    'stormwind_herbalism_day',
    'ashenvale_herb_circuit_long',
    'alliance_apothecary_weekday',
    'stormwind_herb_winddown',
    'silverpine_herbalism_day',
    'silverpine_herb_circuit_long',
    'hillsbrad_mining_day',
    'hillsbrad_ore_circuit_long',
    'orgrimmar_mining_winddown',
    'feralas_fishing_relaxed',
    'feralas_fishing_marathon',
    'shattrath_city_services_routine',
    'outland_herbalism_day',
    'outland_fieldwork_marathon',
    'alliance_outland_trade_run',
    'horde_outland_trade_run',
    'northrend_mining_day',
    'northrend_fishing_day',
    'northrend_dual_profession_day',
    'grizzly_hills_mining_loop',
    'grizzly_hills_fishing_loop',
    'dalaran_gatherer_evening');

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Repeat the full Stormwind services loop three times'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'stormwind_city_errands_routine';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Repeat the full Orgrimmar services loop three times'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'orgrimmar_city_errands_routine';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Repeat the full Dalaran services loop three times'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'dalaran_city_errands_routine';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Begin with Stormwind errands'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'stormwind_herbalism_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Gather herbs in Ashenvale'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_alliance'
WHERE p.playlist_key = 'stormwind_herbalism_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Return to Stormwind to finish the day'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'stormwind_herbalism_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Extended three-pass Ashenvale herb circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_alliance'
WHERE p.playlist_key = 'ashenvale_herb_circuit_long';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Morning herb gathering run'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_alliance'
WHERE p.playlist_key = 'alliance_apothecary_weekday'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Sort and idle in Stormwind'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'alliance_apothecary_weekday'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Second herb sweep before logging out'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_alliance'
WHERE p.playlist_key = 'alliance_apothecary_weekday';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Gather first'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_alliance'
WHERE p.playlist_key = 'stormwind_herb_winddown'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 2, 'Long Stormwind wind-down loop'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'stormwind_herb_winddown';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Start from Orgrimmar'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'silverpine_herbalism_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Gather herbs in Silverpine'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_horde'
WHERE p.playlist_key = 'silverpine_herbalism_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Return to Orgrimmar'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'silverpine_herbalism_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Extended three-pass Silverpine herb circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_low_horde'
WHERE p.playlist_key = 'silverpine_herb_circuit_long';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Morning Orgrimmar errands'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'hillsbrad_mining_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Mine in Hillsbrad'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_mid_horde'
WHERE p.playlist_key = 'hillsbrad_mining_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Return to Orgrimmar with ore'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'hillsbrad_mining_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Extended three-pass Hillsbrad ore circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_mid_horde'
WHERE p.playlist_key = 'hillsbrad_ore_circuit_long';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Mine before returning home'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_mid_horde'
WHERE p.playlist_key = 'orgrimmar_mining_winddown'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 2, 'Extended Orgrimmar wind-down loop'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'orgrimmar_mining_winddown';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Three relaxed fishing passes in Feralas'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'fish_mid_neutral'
WHERE p.playlist_key = 'feralas_fishing_relaxed';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 4, 'Long four-pass Feralas fishing marathon'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'fish_mid_neutral'
WHERE p.playlist_key = 'feralas_fishing_marathon';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Repeat Shattrath services loop three times'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'shattrath_city_services_routine';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Start in Shattrath'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'outland_herbalism_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Gather herbs in Nagrand'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_outland'
WHERE p.playlist_key = 'outland_herbalism_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Return to Shattrath'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'outland_herbalism_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Extended three-pass Nagrand herb circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_outland'
WHERE p.playlist_key = 'outland_fieldwork_marathon';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Stormwind prep before Outland trade run'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'alliance_outland_trade_run'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Shattrath arrival and errands'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'alliance_outland_trade_run'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Gather Nagrand herbs while abroad'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_outland'
WHERE p.playlist_key = 'alliance_outland_trade_run'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'Shattrath wrap-up before returning'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'alliance_outland_trade_run';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Orgrimmar prep before Outland trade run'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'horde_outland_trade_run'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Shattrath arrival and errands'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'horde_outland_trade_run'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Gather Nagrand herbs while abroad'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_herb_outland'
WHERE p.playlist_key = 'horde_outland_trade_run'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'Shattrath wrap-up before returning'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'shattrath_city_services'
WHERE p.playlist_key = 'horde_outland_trade_run';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Start in Dalaran'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'northrend_mining_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Mine in Grizzly Hills'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_northrend'
WHERE p.playlist_key = 'northrend_mining_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Return to Dalaran'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'northrend_mining_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Start in Dalaran'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'northrend_fishing_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Fish in Grizzly Hills'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'fish_northrend'
WHERE p.playlist_key = 'northrend_fishing_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Return to Dalaran'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'northrend_fishing_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Open with Dalaran services'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'northrend_dual_profession_day'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Ore gathering pass'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_northrend'
WHERE p.playlist_key = 'northrend_dual_profession_day'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Fishing pass'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'fish_northrend'
WHERE p.playlist_key = 'northrend_dual_profession_day'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'Close with Dalaran services'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'northrend_dual_profession_day';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Extended three-pass Grizzly Hills ore circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_northrend'
WHERE p.playlist_key = 'grizzly_hills_mining_loop';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 3, 'Extended three-pass Grizzly Hills fishing circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'fish_northrend'
WHERE p.playlist_key = 'grizzly_hills_fishing_loop';

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Evening ore pass'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'gather_ore_northrend'
WHERE p.playlist_key = 'dalaran_gatherer_evening'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Evening fishing pass'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'fish_northrend'
WHERE p.playlist_key = 'dalaran_gatherer_evening'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 2, 'Settle back into Dalaran for the night'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'dalaran_gatherer_evening';