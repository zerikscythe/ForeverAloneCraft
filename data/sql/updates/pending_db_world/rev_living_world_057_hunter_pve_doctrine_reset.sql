-- rev_living_world_057_hunter_pve_doctrine_reset (world DB)
--
-- Clean Hunter PvE doctrine reset:
--   * replace the old starter Beast Mastery profile with a class-family PvE pass
--   * rebuild the shallow Marksmanship / Survival placeholder rows into real
--     PvE doctrine profiles
--   * refresh the Beast Mastery template and add dedicated Marksmanship /
--     Survival talent templates from local WotLK guide data
--
-- Human/source notes:
--   Local structured sources:
--     tools/lw-editor/data/icy_veins_wotlk_builds.json
--     tools/lw-editor/data/class_spells.json
--     tools/lw-editor/data/spell_names.json
--   Guide stems represented in that local source bundle:
--     https://www.icy-veins.com/wotlk-classic/beast-mastery-hunter-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/beast-mastery-hunter-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/marksmanship-hunter-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/marksmanship-hunter-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/survival-hunter-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/survival-hunter-dps-pve-rotation-cooldowns-abilities
--
-- Local DBC-backed spell references used here:
--   Rapid Fire          = 3045
--   Bestial Wrath       = 19574
--   Kill Shot           = 61006
--   Serpent Sting       = 1978 / rank aura 49001
--   Volley              = 1510 / rank 58434
--   Multi-Shot          = 2643 / rank 49048
--   Arcane Shot         = 3044 / rank 49045
--   Steady Shot         = 34120 / rank 49052
--   Chimera Shot        = 53209
--   Aimed Shot          = 19434 / rank 49050
--   Black Arrow         = 3674 / rank aura 63672
--   Explosive Shot      = 53301 / rank 60053
--   Lock and Load       = aura 56342
--
-- Design note:
--   This is intentionally a pet-light first pass. The owner-side ranged PvE
--   cadence is modernized now; pet-specific attack weaving like Kill Command
--   and deeper trap positioning can be tightened once shared pet-control is
--   further along.

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
    ( 3, 'BeastMastery', 'DPS', 'Hunter', 'PvE', '', 'Beast Mastery Hunter DPS',
      'Priority-based PvE Beast Mastery Hunter profile rebuilt from local WotLK guide data; pet-light first pass.',
      1, 25, 55, 0, 0, 0, 4, 12, 1, 100, 170, 85, 185, 240, 145, 175),
    (20, 'Marksmanship', 'DPS', 'Hunter', 'PvE', '', 'Marksmanship Hunter DPS',
      'Priority-based PvE Marksmanship Hunter profile rebuilt from local WotLK guide data; pet-light first pass.',
      1, 25, 55, 0, 0, 0, 4, 12, 1, 100, 170, 85, 185, 240, 145, 175),
    (21, 'Survival',     'DPS', 'Hunter', 'PvE', '', 'Survival Hunter DPS',
      'Priority-based PvE Survival Hunter profile rebuilt from local WotLK guide data; pet-light first pass.',
      1, 25, 55, 0, 0, 0, 4, 12, 1, 100, 170, 85, 185, 240, 145, 175)
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
-- HUNTER TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    ( 7, 'BeastMastery', 3, '', 'Beast Mastery Hunter DPS',
        '52/14/5 PvE Beast Mastery Hunter talent template based on local WotLK guide data.'),
    (27, 'Marksmanship', 3, '', 'Marksmanship Hunter DPS',
        '7/54/10 PvE Marksmanship Hunter talent template based on local WotLK guide data.'),
    (28, 'Survival',     3, '', 'Survival Hunter DPS',
        '0/15/56 PvE Survival Hunter talent template based on local WotLK guide data.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (7, 27, 28);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Beast Mastery Hunter (52 / 14 / 5)
    ( 7,   1, 1382, 'Improved Aspect of the Hawk', 5),
    ( 7,   2, 1389, 'Endurance Training', 1),
    ( 7,  10, 1624, 'Focused Fire', 2),
    ( 7,  13, 1625, 'Improved Revive Pet', 2),
    ( 7,  21, 2138, 'Aspect Mastery', 1),
    ( 7,  22, 1396, 'Unleashed Fury', 5),
    ( 7,  32, 1393, 'Ferocity', 5),
    ( 7,  40, 1388, 'Spirit Bond', 1),
    ( 7,  41, 1387, 'Intimidation', 1),
    ( 7,  43, 1390, 'Bestial Discipline', 2),
    ( 7,  50, 1799, 'Animal Handler', 2),
    ( 7,  52, 1397, 'Frenzy', 3),
    ( 7,  60, 1800, 'Ferocious Inspiration', 3),
    ( 7,  61, 1386, 'Bestial Wrath', 1),
    ( 7,  62, 1801, 'Catlike Reflexes', 1),
    ( 7,  72, 1802, 'Serpent''s Swiftness', 5),
    ( 7,  80, 2140, 'Longevity', 3),
    ( 7,  81, 1803, 'The Beast Within', 1),
    ( 7,  82, 2137, 'Cobra Strikes', 2),
    ( 7,  91, 2227, 'Kindred Spirits', 5),
    ( 7, 101, 2139, 'Beast Mastery', 1),
    ( 7, 202, 1344, 'Lethal Shots', 5),
    ( 7, 210, 1806, 'Careful Aim', 3),
    ( 7, 212, 1349, 'Mortal Shots', 5),
    ( 7, 220, 1818, 'Go for the Throat', 1),
    ( 7, 300, 1623, 'Improved Tracking', 5),

    -- Marksmanship Hunter (7 / 54 / 10)
    (27,   1, 1382, 'Improved Aspect of the Hawk', 5),
    (27,  10, 1624, 'Focused Fire', 2),
    (27, 202, 1344, 'Lethal Shots', 5),
    (27, 210, 1806, 'Careful Aim', 3),
    (27, 211, 1343, 'Improved Hunter''s Mark', 3),
    (27, 212, 1349, 'Mortal Shots', 5),
    (27, 220, 1818, 'Go for the Throat', 1),
    (27, 222, 1345, 'Aimed Shot', 1),
    (27, 231, 1348, 'Improved Stings', 3),
    (27, 241, 1353, 'Readiness', 1),
    (27, 242, 1347, 'Barrage', 3),
    (27, 250, 1804, 'Combat Experience', 2),
    (27, 253, 1362, 'Ranged Weapon Specialization', 3),
    (27, 260, 2130, 'Piercing Shots', 3),
    (27, 261, 1361, 'Trueshot Aura', 1),
    (27, 262, 1821, 'Improved Barrage', 3),
    (27, 271, 1807, 'Master Marksman', 5),
    (27, 280, 2132, 'Wild Quiver', 3),
    (27, 281, 1808, 'Silencing Shot', 1),
    (27, 282, 2133, 'Improved Steady Shot', 2),
    (27, 291, 2134, 'Marked for Death', 5),
    (27, 301, 2135, 'Chimera Shot', 1),
    (27, 300, 1623, 'Improved Tracking', 5),
    (27, 312, 1305, 'Trap Mastery', 3),
    (27, 313, 1810, 'Survival Instincts', 2),

    -- Survival Hunter (0 / 15 / 56)
    (28,   2, 1344, 'Lethal Shots', 5),
    (28,  10, 1806, 'Careful Aim', 3),
    (28,  12, 1349, 'Mortal Shots', 5),
    (28,  20, 1818, 'Go for the Throat', 1),
    (28,  22, 1345, 'Aimed Shot', 1),
    (28, 200, 1623, 'Improved Tracking', 5),
    (28, 212, 1305, 'Trap Mastery', 3),
    (28, 213, 1810, 'Survival Instincts', 2),
    (28, 220, 1622, 'Survivalist', 5),
    (28, 231, 2229, 'T.N.T.', 3),
    (28, 233, 1306, 'Lock and Load', 3),
    (28, 240, 2228, 'Hunter vs. Wild', 3),
    (28, 241, 1321, 'Killer Instinct', 3),
    (28, 250, 1303, 'Lightning Reflexes', 5),
    (28, 252, 1809, 'Resourcefulness', 3),
    (28, 260, 1812, 'Expose Weakness', 1),
    (28, 261, 1325, 'Wyvern Sting', 1),
    (28, 262, 1811, 'Thrill of the Hunt', 3),
    (28, 270, 1813, 'Master Tactician', 5),
    (28, 271, 2141, 'Noxious Stings', 3),
    (28, 281, 1322, 'Black Arrow', 1),
    (28, 283, 2143, 'Sniper Training', 3),
    (28, 292, 2144, 'Hunting Party', 3),
    (28, 301, 2145, 'Explosive Shot', 1);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE `c`
FROM `living_world_bot_combat_default_condition` `c`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `c`.`entry_id`
WHERE `e`.`default_profile_id` IN (3, 20, 21);

DELETE `a`
FROM `living_world_bot_combat_default_action` `a`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `a`.`entry_id`
WHERE `e`.`default_profile_id` IN (3, 20, 21);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `default_profile_id` IN (3, 20, 21);

-- -----------------------------------------------------------------------
-- BEAST MASTERY HUNTER PVE PROFILE (3)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (610,  3,  0, 'Rapid Fire',                   0, 0, 1, 0),
    (611,  3, 10, 'Bestial Wrath',                0, 0, 1, 0),
    (612,  3, 20, 'Kill Shot execute',            0, 0, 1, 0),
    (613,  3, 30, 'Serpent Sting upkeep',         0, 0, 1, 0),
    (614,  3, 40, 'Volley (4+ AoE)',              0, 0, 1, 0),
    (615,  3, 50, 'Multi-Shot (3+ cleave)',       0, 0, 1, 0),
    (616,  3, 60, 'Arcane Shot',                  0, 0, 1, 0),
    (617,  3, 70, 'Steady Shot filler',           0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (6100, 610, 0, 0, 3045, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6110, 611, 0, 0, 19574, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6120, 612, 0, 0, 61006, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6130, 613, 0, 0,  1978, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6140, 614, 0, 0,  1510, 0, 0, 0, 'enemy_primary', 0,    4,   12),
    (6150, 615, 0, 0,  2643, 0, 0, 0, 'enemy_primary', 0,    3,   10),
    (6160, 616, 0, 0,  3044, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6170, 617, 0, 0, 34120, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (6120, 612, 0, 'enemy', 'hp_pct',              3, 20.0, ''),
    (6130, 613, 0, 'enemy', 'aura',                7,  0.0, '49001'),
    (6140, 614, 0, 'enemy', 'nearby_enemies',      5,  4.0, '12'),
    (6150, 615, 0, 'enemy', 'nearby_enemies',      5,  3.0, '10'),
    (6160, 616, 0, 'self',  'power_pct',           5, 35.0, '');

-- -----------------------------------------------------------------------
-- MARKSMANSHIP HUNTER PVE PROFILE (20)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (620, 20,  0, 'Rapid Fire',                    0, 0, 1, 0),
    (621, 20, 10, 'Kill Shot execute',             0, 0, 1, 0),
    (622, 20, 20, 'Serpent Sting upkeep',          0, 0, 1, 0),
    (623, 20, 30, 'Chimera Shot',                  0, 0, 1, 0),
    (624, 20, 40, 'Aimed Shot',                    0, 0, 1, 0),
    (625, 20, 50, 'Volley (4+ AoE)',               0, 0, 1, 0),
    (626, 20, 60, 'Multi-Shot (3+ cleave)',        0, 0, 1, 0),
    (627, 20, 70, 'Arcane Shot',                   0, 0, 1, 0),
    (628, 20, 80, 'Steady Shot filler',            0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (6200, 620, 0, 0, 3045, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6210, 621, 0, 0, 61006, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6220, 622, 0, 0,  1978, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6230, 623, 0, 0, 53209, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6240, 624, 0, 0, 19434, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6250, 625, 0, 0,  1510, 0, 0, 0, 'enemy_primary', 0,    4,   12),
    (6260, 626, 0, 0,  2643, 0, 0, 0, 'enemy_primary', 0,    3,   10),
    (6270, 627, 0, 0,  3044, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6280, 628, 0, 0, 34120, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (6210, 621, 0, 'enemy', 'hp_pct',              3, 20.0, ''),
    (6220, 622, 0, 'enemy', 'aura',                7,  0.0, '49001'),
    (6250, 625, 0, 'enemy', 'nearby_enemies',      5,  4.0, '12'),
    (6260, 626, 0, 'enemy', 'nearby_enemies',      5,  3.0, '10'),
    (6270, 627, 0, 'self',  'power_pct',           5, 40.0, '');

-- -----------------------------------------------------------------------
-- SURVIVAL HUNTER PVE PROFILE (21)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (630, 21,  0, 'Rapid Fire',                    0, 0, 1, 0),
    (631, 21, 10, 'Explosive Shot (Lock and Load)',0, 0, 1, 0),
    (632, 21, 20, 'Kill Shot execute',             0, 0, 1, 0),
    (633, 21, 30, 'Black Arrow upkeep',            0, 0, 1, 0),
    (634, 21, 40, 'Serpent Sting upkeep',          0, 0, 1, 0),
    (635, 21, 50, 'Explosive Shot',                0, 0, 1, 0),
    (636, 21, 60, 'Aimed Shot',                    0, 0, 1, 0),
    (637, 21, 70, 'Volley (4+ AoE)',               0, 0, 1, 0),
    (638, 21, 80, 'Multi-Shot (3+ cleave)',        0, 0, 1, 0),
    (639, 21, 90, 'Steady Shot filler',            0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (6300, 630, 0, 0, 3045, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6310, 631, 0, 0, 53301, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6320, 632, 0, 0, 61006, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6330, 633, 0, 0,  3674, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6340, 634, 0, 0,  1978, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6350, 635, 0, 0, 53301, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6360, 636, 0, 0, 19434, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6370, 637, 0, 0,  1510, 0, 0, 0, 'enemy_primary', 0,    4,   12),
    (6380, 638, 0, 0,  2643, 0, 0, 0, 'enemy_primary', 0,    3,   10),
    (6390, 639, 0, 0, 34120, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (6310, 631, 0, 'self',  'aura',                6,  0.0, '56342'),
    (6320, 632, 0, 'enemy', 'hp_pct',              3, 20.0, ''),
    (6330, 633, 0, 'enemy', 'aura_remaining_secs', 3,  3.0, '63672'),
    (6340, 634, 0, 'enemy', 'aura',                7,  0.0, '49001'),
    (6370, 637, 0, 'enemy', 'nearby_enemies',      5,  4.0, '12'),
    (6380, 638, 0, 'enemy', 'nearby_enemies',      5,  3.0, '10');
