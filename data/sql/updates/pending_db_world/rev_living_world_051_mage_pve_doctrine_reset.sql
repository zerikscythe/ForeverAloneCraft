-- rev_living_world_051_mage_pve_doctrine_reset (world DB)
--
-- Clean Mage PvE doctrine reset:
--   * fold the older Arcane-only refinement into one class-family reset
--   * replace the legacy Fire / Frost starter rows with modern PvE profiles
--   * refresh Arcane / Frost talent templates and add a dedicated Fire template
--     sourced from current WotLK Classic guide material
--
-- Human/source notes:
--   Arcane PvE:
--     https://www.icy-veins.com/wotlk-classic/arcane-mage-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/arcane-mage-dps-pve-rotation-cooldowns-abilities
--   Fire PvE:
--     https://www.icy-veins.com/wotlk-classic/fire-mage-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/fire-mage-dps-pve-rotation-cooldowns-abilities
--   Frost PvE:
--     https://www.icy-veins.com/wotlk-classic/frost-mage-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/frost-mage-dps-pve-rotation-cooldowns-abilities
--
-- Local DBC-backed spell references used here:
--   Arcane Blast      = 30451 / 42897
--   Arcane Missiles   = 5143 / 42846
--   Arcane Barrage    = 44781
--   Arcane Power      = 12042
--   Presence of Mind  = 12043
--   Mirror Image      = 55342
--   Icy Veins         = 12472
--   Evocation         = 12051
--   Mana Sapphire     = item 33312 -> Replenish Mana 42987
--   Living Bomb       = 44457 / aura 55360
--   Pyroblast         = 11366
--   Fireball          = 133
--   Scorch            = 2948 / debuff 22959
--   Flamestrike       = 42926
--   Combustion        = 11129
--   Frostbolt         = 116
--   Deep Freeze       = 44572
--   Frostfire Bolt    = 44614
--   Ice Lance         = 30455
--   Blizzard          = 42940
--   Fire Blast        = 42873 / 2136
--
-- Design note:
--   This pass keeps mage armor / conjured-food ceremony light. The goal is a
--   strong PvE spell-priority spine first, with richer buff / consumable /
--   utility scripting layered in later.

-- -----------------------------------------------------------------------
-- PROFILE ROWS
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_profile` (
    `default_profile_id`, `spec_key`, `role_key`, `class_key`, `context_key`,
    `variant_key`, `display_name`, `description`,
    `conservation_mode`, `resource_low_water`, `resource_high_water`,
    `enable_down_rank`, `down_rank_floor`,
    `default_aoe_mode`, `default_aoe_min_targets`, `default_aoe_scan_radius`,
    `targeting_mode`, `current_target_bias`, `assist_target_bias`,
    `focus_fire_bias`, `protect_ally_bias`, `prefer_healer_bias`,
    `prefer_dps_bias`, `avoid_tank_bias`
) VALUES
    ( 8, 'Frost',  'DPS', 'Mage', 'PvE', '', 'Frost Mage DPS',
      'Priority-based PvE Frost Mage profile rebuilt from current WotLK Classic guide material.',
      1, 40, 70, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (26, 'Arcane', 'DPS', 'Mage', 'PvE', '', 'Arcane Mage DPS',
      'Priority-based PvE Arcane Mage profile rebuilt from current WotLK Classic guide material.',
      1, 45, 75, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (27, 'Fire',   'DPS', 'Mage', 'PvE', '', 'Fire Mage DPS',
      'Priority-based PvE Fire Mage profile rebuilt from current WotLK Classic guide material.',
      1, 40, 70, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175)
ON DUPLICATE KEY UPDATE
    `spec_key`                = VALUES(`spec_key`),
    `role_key`                = VALUES(`role_key`),
    `class_key`               = VALUES(`class_key`),
    `context_key`             = VALUES(`context_key`),
    `variant_key`             = VALUES(`variant_key`),
    `display_name`            = VALUES(`display_name`),
    `description`             = VALUES(`description`),
    `conservation_mode`       = VALUES(`conservation_mode`),
    `resource_low_water`      = VALUES(`resource_low_water`),
    `resource_high_water`     = VALUES(`resource_high_water`),
    `enable_down_rank`        = VALUES(`enable_down_rank`),
    `down_rank_floor`         = VALUES(`down_rank_floor`),
    `default_aoe_mode`        = VALUES(`default_aoe_mode`),
    `default_aoe_min_targets` = VALUES(`default_aoe_min_targets`),
    `default_aoe_scan_radius` = VALUES(`default_aoe_scan_radius`),
    `targeting_mode`          = VALUES(`targeting_mode`),
    `current_target_bias`     = VALUES(`current_target_bias`),
    `assist_target_bias`      = VALUES(`assist_target_bias`),
    `focus_fire_bias`         = VALUES(`focus_fire_bias`),
    `protect_ally_bias`       = VALUES(`protect_ally_bias`),
    `prefer_healer_bias`      = VALUES(`prefer_healer_bias`),
    `prefer_dps_bias`         = VALUES(`prefer_dps_bias`),
    `avoid_tank_bias`         = VALUES(`avoid_tank_bias`);

-- -----------------------------------------------------------------------
-- MAGE TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    (15, 'Frost',  8, '', 'Frost Mage DPS',
        'PvE Frost Mage talent template refreshed against current WotLK Classic guide material.'),
    (20, 'Arcane', 8, '', 'Arcane Mage DPS',
        'PvE Arcane Mage talent template refreshed against current WotLK Classic guide material.'),
    (26, 'Fire',   8, '', 'Fire Mage DPS',
        'PvE Fire Mage talent template based on current WotLK Classic guide material.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (15, 20, 26);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Frost Mage
    (15,   0,   74, 'Arcane Subtlety', 2),
    (15,   1,   76, 'Arcane Focus', 3),
    (15,  12,   75, 'Arcane Concentration', 5),
    (15,  21,   81, 'Spell Impact', 3),
    (15,  22, 1845, 'Student of the Mind', 1),
    (15,  23, 2211, 'Focus Magic', 1),
    (15,  33, 2222, 'Torment the Weak', 3),
    (15, 200,   37, 'Improved Frostbolt', 5),
    (15, 201,   62, 'Ice Floes', 3),
    (15, 210,   73, 'Ice Shards', 3),
    (15, 212, 1649, 'Precision', 3),
    (15, 220,   61, 'Piercing Ice', 3),
    (15, 221,   69, 'Icy Veins', 1),
    (15, 222,   63, 'Improved Blizzard', 1),
    (15, 230,  741, 'Arctic Reach', 1),
    (15, 231,   66, 'Frost Channeling', 3),
    (15, 232,   67, 'Shatter', 3),
    (15, 241,   72, 'Cold Snap', 1),
    (15, 252,   68, 'Winter''s Chill', 3),
    (15, 261,   71, 'Ice Barrier', 1),
    (15, 262, 1738, 'Arctic Winds', 5),
    (15, 271, 1740, 'Empowered Frostbolt', 2),
    (15, 272, 1853, 'Fingers of Frost', 2),
    (15, 280, 1854, 'Brain Freeze', 3),
    (15, 281, 1741, 'Summon Water Elemental', 1),
    (15, 282, 1855, 'Enduring Winter', 3),
    (15, 291, 1856, 'Chilled to the Bone', 5),
    (15, 301, 1857, 'Deep Freeze', 1),

    -- Arcane Mage
    (20,   0,   74, 'Arcane Subtlety', 2),
    (20,   1,   76, 'Arcane Focus', 3),
    (20,  12,   75, 'Arcane Concentration', 5),
    (20,  20,   82, 'Magic Attunement', 2),
    (20,  21,   81, 'Spell Impact', 3),
    (20,  22, 1845, 'Student of the Mind', 1),
    (20,  23, 2211, 'Focus Magic', 1),
    (20,  32, 1142, 'Arcane Meditation', 3),
    (20,  33, 2222, 'Torment the Weak', 3),
    (20,  41,   86, 'Presence of Mind', 1),
    (20,  43,   77, 'Arcane Mind', 5),
    (20,  51,  421, 'Arcane Instability', 3),
    (20,  52, 1725, 'Arcane Potency', 2),
    (20,  60, 1727, 'Arcane Empowerment', 3),
    (20,  61,   87, 'Arcane Power', 1),
    (20,  71, 1843, 'Arcane Flows', 2),
    (20,  72, 1728, 'Mind Mastery', 5),
    (20,  82, 2209, 'Missile Barrage', 5),
    (20,  91, 1846, 'Netherwind Presence', 3),
    (20,  92, 1826, 'Spell Power', 2),
    (20, 101, 1847, 'Arcane Barrage', 1),
    (20, 201, 1141, 'Incineration', 3),
    (20, 300,   37, 'Improved Frostbolt', 2),
    (20, 301,   62, 'Ice Floes', 3),
    (20, 310,   73, 'Ice Shards', 3),
    (20, 312, 1649, 'Precision', 3),
    (20, 321,   69, 'Icy Veins', 1),

    -- Fire Mage
    (26,   0,   74, 'Arcane Subtlety', 2),
    (26,   1,   76, 'Arcane Focus', 3),
    (26,  12,   75, 'Arcane Concentration', 5),
    (26,  21,   81, 'Spell Impact', 3),
    (26,  22, 1845, 'Student of the Mind', 1),
    (26,  23, 2211, 'Focus Magic', 1),
    (26,  33, 2222, 'Torment the Weak', 3),
    (26, 102,   26, 'Improved Fireball', 5),
    (26, 110,   34, 'Ignite', 5),
    (26, 112,   31, 'World in Flames', 3),
    (26, 122,   29, 'Pyroblast', 1),
    (26, 123,   23, 'Burning Soul', 2),
    (26, 130,   25, 'Improved Scorch', 3),
    (26, 133, 1639, 'Master of Elements', 2),
    (26, 140, 1730, 'Playing with Fire', 3),
    (26, 141,   33, 'Critical Mass', 3),
    (26, 142,   32, 'Blast Wave', 1),
    (26, 152,   35, 'Fire Power', 5),
    (26, 160, 1733, 'Pyromaniac', 3),
    (26, 161,   36, 'Combustion', 1),
    (26, 162, 1732, 'Molten Fury', 2),
    (26, 172, 1734, 'Empowered Fire', 3),
    (26, 180, 1849, 'Firestarter', 1),
    (26, 181, 1735, 'Dragon''s Breath', 1),
    (26, 182, 1850, 'Hot Streak', 3),
    (26, 191, 1851, 'Burnout', 5),
    (26, 201, 1852, 'Living Bomb', 1);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE `c`
FROM `living_world_bot_combat_default_condition` `c`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `c`.`entry_id`
WHERE `e`.`default_profile_id` IN (8, 26, 27);

DELETE `a`
FROM `living_world_bot_combat_default_action` `a`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `a`.`entry_id`
WHERE `e`.`default_profile_id` IN (8, 26, 27);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `default_profile_id` IN (8, 26, 27);

-- -----------------------------------------------------------------------
-- ARCANE MAGE PVE PROFILE (26)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (260, 26,  0, 'Mana Sapphire (mana recovery)',               0, 0, 1, 0),
    (261, 26,  2, 'Evocation (low mana)',                        0, 0, 1, 0),
    (262, 26,  4, 'Mirror Image',                                0, 0, 1, 0),
    (263, 26,  6, 'Icy Veins',                                   0, 0, 1, 0),
    (264, 26,  8, 'Arcane Power',                                0, 0, 1, 0),
    (265, 26, 10, 'Presence of Mind',                            0, 0, 1, 0),
    (266, 26, 20, 'Blizzard (3+ AoE)',                           0, 0, 1, 0),
    (2666, 26, 25, 'Arcane Explosion (3+ close AoE)',            0, 0, 1, 0),
    (267, 26, 30, 'Arcane Missiles (Missile Barrage 3+ stacks)', 0, 0, 1, 0),
    (268, 26, 40, 'Arcane Blast (stationary filler)',            0, 0, 1, 0),
    (269, 26, 50, 'Arcane Barrage (moving)',                     0, 0, 1, 0),
    (270, 26, 60, 'Fire Blast (moving fallback)',                0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (2600, 260, 0, 1,     0, 33312, 0, 0, 'self',          NULL, NULL, NULL),
    (2610, 261, 0, 0, 12051,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2620, 262, 0, 0, 55342,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2630, 263, 0, 0, 12472,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2640, 264, 0, 0, 12042,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2650, 265, 0, 0, 12043,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2660, 266, 0, 0, 42940,     0, 0, 0, 'enemy_primary', 0,    3,   10),
    (26660, 2666, 0, 0, 42920,   0, 0, 0, 'self',          NULL, NULL, NULL),
    (2670, 267, 0, 0,  5143,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2680, 268, 0, 0, 30451,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2690, 269, 0, 0, 44781,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2700, 270, 0, 0, 42873,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (2600, 260, 0, 'self',  'power_pct',           3, 60.0, ''),
    (2610, 261, 0, 'self',  'power_pct',           3, 15.0, ''),
    (2611, 261, 1, 'self',  'is_moving',           3,  0.0, ''),
    (2660, 266, 0, 'enemy', 'nearby_enemies',      5,  3.0, '10'),
    (26660, 2666, 0, 'self', 'nearby_enemies',     5,  3.0, '10'),
    (2670, 267, 0, 'self',  'aura',                6, 44401.0, ''),
    (2671, 267, 1, 'self',  'aura_stacks',         5,  3.0, '36032'),
    (2680, 268, 0, 'self',  'is_moving',           3,  0.0, ''),
    (2690, 269, 0, 'self',  'is_moving',           5,  1.0, ''),
    (2700, 270, 0, 'self',  'is_moving',           5,  1.0, '');

-- -----------------------------------------------------------------------
-- FIRE MAGE PVE PROFILE (27)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (271, 27,  0, 'Mana Sapphire (mana recovery)',             0, 0, 1, 0),
    (272, 27,  2, 'Evocation (low mana)',                      0, 0, 1, 0),
    (273, 27,  4, 'Mirror Image',                              0, 0, 1, 0),
    (274, 27,  6, 'Combustion',                                0, 0, 1, 0),
    (275, 27, 20, 'Flamestrike / Blizzard (4+ AoE)',           0, 0, 1, 0),
    (276, 27, 25, 'Blizzard (3+ AoE)',                         0, 0, 1, 0),
    (2766, 27, 27, 'Arcane Explosion (3+ close AoE)',         0, 0, 1, 0),
    (277, 27, 30, 'Living Bomb upkeep',                        0, 0, 1, 0),
    (278, 27, 40, 'Pyroblast (Hot Streak)',                    0, 0, 1, 0),
    (279, 27, 50, 'Scorch upkeep',                             0, 0, 1, 0),
    (280, 27, 60, 'Fireball',                                  0, 0, 1, 0),
    (281, 27, 70, 'Fire Blast (moving fallback)',              0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (2710, 271, 0, 1,     0, 33312, 0, 0, 'self',          NULL, NULL, NULL),
    (2720, 272, 0, 0, 12051,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2730, 273, 0, 0, 55342,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2740, 274, 0, 0, 11129,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (2750, 275, 0, 0, 42926,     0, 0, 0, 'enemy_primary', 0,    4,   10),
    (2751, 275, 1, 0, 42940,     0, 0, 0, 'enemy_primary', 0,    4,   10),
    (2760, 276, 0, 0, 42940,     0, 0, 0, 'enemy_primary', 0,    3,   10),
    (27660, 2766, 0, 0, 42920,   0, 0, 0, 'self',          NULL, NULL, NULL),
    (2770, 277, 0, 0, 44457,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2780, 278, 0, 0, 11366,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2790, 279, 0, 0,  2948,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2800, 280, 0, 0,   133,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (2810, 281, 0, 0, 42873,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (2710, 271, 0, 'self',  'power_pct',           3, 60.0, ''),
    (2720, 272, 0, 'self',  'power_pct',           3, 15.0, ''),
    (2721, 272, 1, 'self',  'is_moving',           3,  0.0, ''),
    (2750, 275, 0, 'enemy', 'nearby_enemies',      5,  4.0, '10'),
    (2760, 276, 0, 'enemy', 'nearby_enemies',      5,  3.0, '10'),
    (27660, 2766, 0, 'self', 'nearby_enemies',     5,  3.0, '10'),
    (2770, 277, 0, 'enemy', 'aura_remaining_secs', 3,  3.0, '55360'),
    (2780, 278, 0, 'self',  'aura',                6, 44448.0, ''),
    (2790, 279, 0, 'enemy', 'aura_remaining_secs', 3,  5.0, '22959'),
    (2810, 281, 0, 'self',  'is_moving',           5,  1.0, '');

-- -----------------------------------------------------------------------
-- FROST MAGE PVE PROFILE (8)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (350,  8,  0, 'Mana Sapphire (mana recovery)',        0, 0, 1, 0),
    (351,  8,  2, 'Evocation (low mana)',                 0, 0, 1, 0),
    (352,  8,  4, 'Mirror Image',                         0, 0, 1, 0),
    (353,  8,  6, 'Icy Veins',                            0, 0, 1, 0),
    (354,  8, 20, 'Blizzard (3+ AoE)',                    0, 0, 1, 0),
    (3546, 8, 25, 'Arcane Explosion (3+ close AoE)',      0, 0, 1, 0),
    (355,  8, 30, 'Deep Freeze (Fingers of Frost)',       0, 0, 1, 0),
    (356,  8, 40, 'Frostfire Bolt (Brain Freeze)',        0, 0, 1, 0),
    (357,  8, 50, 'Frostbolt',                            0, 0, 1, 0),
    (358,  8, 60, 'Ice Lance',                            0, 0, 1, 0),
    (359,  8, 70, 'Fire Blast (moving fallback)',         0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (3500, 350, 0, 1,     0, 33312, 0, 0, 'self',          NULL, NULL, NULL),
    (3510, 351, 0, 0, 12051,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (3520, 352, 0, 0, 55342,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (3530, 353, 0, 0, 12472,     0, 1, 0, 'self',          NULL, NULL, NULL),
    (3540, 354, 0, 0, 42940,     0, 0, 0, 'enemy_primary', 0,    3,   10),
    (35460, 3546, 0, 0, 42920,   0, 0, 0, 'self',          NULL, NULL, NULL),
    (3550, 355, 0, 0, 44572,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (3560, 356, 0, 0, 44614,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (3570, 357, 0, 0,   116,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (3580, 358, 0, 0, 30455,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (3590, 359, 0, 0,  2136,     0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3500, 350, 0, 'self',  'power_pct',           3, 60.0, ''),
    (3510, 351, 0, 'self',  'power_pct',           3, 15.0, ''),
    (3511, 351, 1, 'self',  'is_moving',           3,  0.0, ''),
    (3540, 354, 0, 'enemy', 'nearby_enemies',      5,  3.0, '10'),
    (35460, 3546, 0, 'self', 'nearby_enemies',     5,  3.0, '10'),
    (3550, 355, 0, 'self',  'aura',                6, 44545.0, ''),
    (3560, 356, 0, 'self',  'aura',                6, 44549.0, ''),
    (3590, 359, 0, 'self',  'is_moving',           5,  1.0, '');
