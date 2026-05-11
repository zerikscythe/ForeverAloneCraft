-- rev_living_world_013_hub_spawn_support (world DB)
--
-- Adds the city-hub data needed for level-banded autonomous world-bot spawn
-- fallback and corrects the Orgrimmar zone-id mismatch in the first seed pass.

-- Ensure the zone index contains the intended hub cities.
INSERT INTO living_world_zone_index
    (zone_id, map_id, zone_name, faction, zone_type, has_herbs, has_ore, has_fish,
     min_level, max_level, anchor_x, anchor_y, anchor_z, notes)
VALUES
    (1519,    0, 'Stormwind City', 1, 'city', 0, 0, 0,  1, 80, -8924.0,   529.0,   96.0,
        'Alliance low-level hub for autonomous world-bot spawn fallback'),
    (1637,    1, 'Orgrimmar',      2, 'city', 0, 0, 0,  1, 80,  1676.8, -4416.6,   62.1,
        'Horde low-level hub for autonomous world-bot spawn fallback'),
    (3703,  530, 'Shattrath City', 0, 'city', 0, 0, 0, 58, 70, -1887.5,  5353.4,  -12.4,
        'Outland hub for autonomous world-bot spawn fallback')
ON DUPLICATE KEY UPDATE
    map_id    = VALUES(map_id),
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
    anchor_z  = VALUES(anchor_z),
    notes     = VALUES(notes);

-- Correct the original Horde city idle rows to use the real Orgrimmar zone id.
UPDATE living_world_activity_library
SET target_zone_id = 1637,
    required_faction = 2,
    required_zone_type = 'city',
    max_per_session = 2
WHERE activity_key IN ('idle_orgrimmar', 'idle_inn_orgrimmar');

-- Ensure the generic world-bot template has at least one fallback model row.
DELETE FROM creature_template_model WHERE CreatureID = 9900001 AND Idx = 0;
INSERT INTO creature_template_model
    (CreatureID, Idx, CreatureDisplayID, DisplayScale, Probability, VerifiedBuild)
VALUES
    (9900001, 0, 49, 1.0, 1.0, NULL);

-- Add a minimal Alliance low-level city task and an Outland neutral hub task so
-- both factions have reliable eligible sessions under the hub-spawn policy.
INSERT INTO living_world_activity_library
    (activity_key, display_name, activity_type, task_family, required_zone_type,
     max_per_session, target_zone_id, required_faction, min_level, max_level,
     requires_herbalism, requires_mining, requires_fishing,
     weight, duration_min_sec, duration_max_sec)
VALUES
    ('idle_stormwind',      'Idle in Stormwind',        'idle_city', 'city_errand', 'city', 2, 1519, 1,  1, 80, 0, 0, 0, 3,  600, 1800),
    ('idle_inn_stormwind',  'Rest at Stormwind Inn',    'idle_inn',  'city_errand', 'city', 2, 1519, 1,  1, 80, 0, 0, 0, 2,  480, 1200),
    ('idle_shattrath',      'Idle in Shattrath',        'idle_city', 'city_errand', 'city', 2, 3703, 0, 58, 69, 0, 0, 0, 3,  600, 1500),
    ('idle_inn_shattrath',  'Rest at Shattrath Inn',    'idle_inn',  'city_errand', 'city', 2, 3703, 0, 58, 69, 0, 0, 0, 2,  480, 1200)
ON DUPLICATE KEY UPDATE
    display_name        = VALUES(display_name),
    activity_type       = VALUES(activity_type),
    task_family         = VALUES(task_family),
    required_zone_type  = VALUES(required_zone_type),
    max_per_session     = VALUES(max_per_session),
    target_zone_id      = VALUES(target_zone_id),
    required_faction    = VALUES(required_faction),
    min_level           = VALUES(min_level),
    max_level           = VALUES(max_level),
    requires_herbalism  = VALUES(requires_herbalism),
    requires_mining     = VALUES(requires_mining),
    requires_fishing    = VALUES(requires_fishing),
    weight              = VALUES(weight),
    duration_min_sec    = VALUES(duration_min_sec),
    duration_max_sec    = VALUES(duration_max_sec);