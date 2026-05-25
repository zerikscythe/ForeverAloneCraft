-- rev_living_world_056_warlock_pve_doctrine_reset (world DB)
--
-- Clean Warlock PvE doctrine reset:
--   * replace the old starter Affliction profile with a class-family PvE pass
--   * rebuild the shallow Demonology / Destruction placeholder rows into real
--     PvE doctrine profiles
--   * refresh the Affliction template and add dedicated Demonology /
--     Destruction talent templates from local WotLK guide data
--
-- Human/source notes:
--   Local structured sources:
--     tools/lw-editor/data/icy_veins_wotlk_builds.json
--     tools/lw-editor/data/class_spells.json
--     tools/lw-editor/data/spell_names.json
--   Guide stems represented in that local source bundle:
--     https://www.icy-veins.com/wotlk-classic/affliction-warlock-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/affliction-warlock-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/demonology-warlock-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/demonology-warlock-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/destruction-warlock-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/destruction-warlock-dps-pve-rotation-cooldowns-abilities
--
-- Local DBC-backed spell references used here:
--   Life Tap             = 1454 / rank 57946
--   Haunt                = 48181
--   Unstable Affliction  = 30108 / rank aura 47843
--   Corruption           = 172 / rank aura 47813
--   Curse of Agony       = 980 / rank aura 47864
--   Curse of Doom        = 603 / rank aura 47867
--   Seed of Corruption   = 47836
--   Drain Soul           = 1120 / rank 47855
--   Shadow Bolt          = 686 / rank 47809
--   Metamorphosis        = 61610
--   Soul Fire            = 6353 / rank 47825
--   Molten Core          = aura 47245
--   Immolation Aura      = 50589
--   Immolate             = 348 / rank aura 47811
--   Incinerate           = 29722 / rank 47838
--   Shadowburn           = 17877 / rank 47827
--   Conflagrate          = 17962
--   Chaos Bolt           = 50796
--
-- Design note:
--   This is intentionally a pet-light first pass. Owner-side Warlock cadence,
--   DoT upkeep, execute behavior, and major cooldowns are modernized now; pet
--   stance/control finesse still comes later with the shared pet-control work.

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
    ( 9, 'Affliction',  'DPS', 'Warlock', 'PvE', '', 'Affliction Warlock DPS',
      'Priority-based PvE Affliction Warlock profile rebuilt from local WotLK guide data; pet-light first pass.',
      1, 35, 65, 0, 0, 0, 4, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (28, 'Demonology',  'DPS', 'Warlock', 'PvE', '', 'Demonology Warlock DPS',
      'Priority-based PvE Demonology Warlock profile rebuilt from local WotLK guide data; pet-light first pass.',
      1, 35, 65, 0, 0, 0, 4, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (29, 'Destruction', 'DPS', 'Warlock', 'PvE', '', 'Destruction Warlock DPS',
      'Priority-based PvE Destruction Warlock profile rebuilt from local WotLK guide data; pet-light first pass.',
      1, 35, 65, 0, 0, 0, 4, 10, 1, 100, 165, 80, 185, 240, 145, 175)
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
-- WARLOCK TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    (16, 'Affliction',  9, '', 'Affliction Warlock DPS',
        '55/0/16 PvE Affliction Warlock talent template based on local WotLK guide data.'),
    (29, 'Demonology',  9, '', 'Demonology Warlock DPS',
        '2/54/15 PvE Demonology Warlock talent template based on local WotLK guide data.'),
    (30, 'Destruction', 9, '', 'Destruction Warlock DPS',
        '0/19/52 PvE Destruction Warlock talent template based on local WotLK guide data.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (16, 29, 30);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Affliction Warlock (55 / 0 / 16)
    (16,   0, 1284, 'Improved Curse of Agony', 2),
    (16,   1, 1005, 'Suppression', 3),
    (16,   2, 1003, 'Improved Corruption', 5),
    (16,  13, 1004, 'Soul Siphon', 2),
    (16,  21, 1001, 'Fel Concentration', 3),
    (16,  31, 1002, 'Nightfall', 2),
    (16,  33, 1764, 'Empowered Corruption', 3),
    (16,  40, 1763, 'Shadow Embrace', 5),
    (16,  41, 1041, 'Siphon Life', 1),
    (16,  50, 1873, 'Improved Felhunter', 2),
    (16,  51, 1042, 'Shadow Mastery', 5),
    (16,  60, 1878, 'Eradication', 3),
    (16,  61, 1669, 'Contagion', 5),
    (16,  72, 1667, 'Malediction', 3),
    (16,  80, 1875, 'Death''s Embrace', 3),
    (16,  81, 1670, 'Unstable Affliction', 1),
    (16,  82, 2245, 'Pandemic', 1),
    (16,  91, 1876, 'Everlasting Affliction', 5),
    (16, 101, 2041, 'Haunt', 1),
    (16, 201,  944, 'Improved Shadow Bolt', 5),
    (16, 202,  943, 'Bane', 5),
    (16, 222,  967, 'Ruin', 5),
    (16, 230,  985, 'Intensity', 1),

    -- Demonology Warlock (2 / 54 / 15)
    (29,   1, 1005, 'Suppression', 2),
    (29, 202, 1223, 'Demonic Embrace', 3),
    (29, 203, 1883, 'Fel Synergy', 2),
    (29, 211, 1225, 'Demonic Brutality', 3),
    (29, 212, 1242, 'Fel Vitality', 3),
    (29, 221, 1282, 'Soul Link', 1),
    (29, 222, 1226, 'Fel Domination', 1),
    (29, 223, 1671, 'Demonic Aegis', 3),
    (29, 231, 1262, 'Unholy Power', 5),
    (29, 232, 1227, 'Master Summoner', 2),
    (29, 242, 1261, 'Master Conjuror', 2),
    (29, 251, 1244, 'Master Demonologist', 5),
    (29, 252, 1283, 'Molten Core', 3),
    (29, 261, 1880, 'Demonic Empowerment', 1),
    (29, 262, 1263, 'Demonic Knowledge', 3),
    (29, 271, 1673, 'Demonic Tactics', 5),
    (29, 272, 2261, 'Decimation', 2),
    (29, 281, 1672, 'Summon Felguard', 1),
    (29, 282, 1884, 'Nemesis', 3),
    (29, 291, 1885, 'Demonic Pact', 5),
    (29, 301, 1886, 'Metamorphosis', 1),
    (29, 301,  944, 'Improved Shadow Bolt', 5),
    (29, 302,  943, 'Bane', 5),
    (29, 322,  967, 'Ruin', 5),

    -- Destruction Warlock (0 / 19 / 52)
    (30,   1, 1222, 'Improved Imp', 3),
    (30,   2, 1223, 'Demonic Embrace', 3),
    (30,   3, 1883, 'Fel Synergy', 1),
    (30,  12, 1242, 'Fel Vitality', 3),
    (30,  21, 1282, 'Soul Link', 1),
    (30,  22, 1226, 'Fel Domination', 1),
    (30,  23, 1671, 'Demonic Aegis', 3),
    (30,  31, 1262, 'Unholy Power', 4),
    (30, 202,  943, 'Bane', 5),
    (30, 210,  982, 'Aftermath', 2),
    (30, 212,  941, 'Cataclysm', 3),
    (30, 220,  983, 'Demonic Power', 2),
    (30, 222,  967, 'Ruin', 5),
    (30, 230,  985, 'Intensity', 2),
    (30, 231,  964, 'Destructive Reach', 1),
    (30, 240, 1817, 'Backlash', 2),
    (30, 241,  961, 'Improved Immolate', 3),
    (30, 242,  981, 'Devastation', 1),
    (30, 252,  966, 'Emberstorm', 5),
    (30, 261,  968, 'Conflagrate', 1),
    (30, 263,  986, 'Pyroclasm', 3),
    (30, 271, 1677, 'Shadow and Flame', 5),
    (30, 280, 1888, 'Backdraft', 3),
    (30, 282, 2045, 'Empowered Imp', 3),
    (30, 291, 1890, 'Fire and Brimstone', 5),
    (30, 301, 1891, 'Chaos Bolt', 1);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE `c`
FROM `living_world_bot_combat_default_condition` `c`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `c`.`entry_id`
WHERE `e`.`default_profile_id` IN (9, 28, 29);

DELETE `a`
FROM `living_world_bot_combat_default_action` `a`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `a`.`entry_id`
WHERE `e`.`default_profile_id` IN (9, 28, 29);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `default_profile_id` IN (9, 28, 29);

-- -----------------------------------------------------------------------
-- AFFLICTION WARLOCK PVE PROFILE (9)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (680,  9,  0, 'Life Tap (mana sustain)',   0, 0, 1, 0),
    (681,  9, 10, 'Seed of Corruption (4+ AoE)', 0, 0, 1, 0),
    (682,  9, 20, 'Haunt upkeep',              0, 0, 1, 0),
    (683,  9, 30, 'Unstable Affliction upkeep',0, 0, 1, 0),
    (684,  9, 40, 'Corruption upkeep',         0, 0, 1, 0),
    (685,  9, 50, 'Curse of Agony upkeep',     0, 0, 1, 0),
    (686,  9, 60, 'Drain Soul execute',        0, 0, 1, 0),
    (687,  9, 70, 'Shadow Bolt filler',        0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (6800, 680, 0, 0,  1454, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6810, 681, 0, 0, 47836, 0, 0, 0, 'enemy_primary', 0,    4,   10),
    (6820, 682, 0, 0, 48181, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6830, 683, 0, 0, 30108, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6840, 684, 0, 0,   172, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6850, 685, 0, 0,   980, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6860, 686, 0, 0,  1120, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6870, 687, 0, 0,   686, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (6800, 680, 0, 'self',  'power_pct',           3, 35.0, ''),
    (6801, 680, 1, 'self',  'hp_pct',              5, 45.0, ''),
    (6810, 681, 0, 'enemy', 'nearby_enemies',      5,  4.0, '10'),
    (6820, 682, 0, 'enemy', 'aura',                7,  0.0, '48181'),
    (6830, 683, 0, 'enemy', 'aura_remaining_secs', 3,  3.0, '47843'),
    (6840, 684, 0, 'enemy', 'aura',                7,  0.0, '47813'),
    (6850, 685, 0, 'enemy', 'aura_remaining_secs', 3,  3.0, '47864'),
    (6860, 686, 0, 'enemy', 'hp_pct',              3, 25.0, '');

-- -----------------------------------------------------------------------
-- DEMONOLOGY WARLOCK PVE PROFILE (28)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (690, 28,  0, 'Life Tap (mana sustain)',       0, 0, 1, 0),
    (691, 28, 10, 'Metamorphosis',                 0, 0, 1, 0),
    (692, 28, 20, 'Seed of Corruption (4+ AoE)',   0, 0, 1, 0),
    (693, 28, 30, 'Soul Fire execute',             0, 0, 1, 0),
    (694, 28, 40, 'Incinerate (Molten Core)',      0, 0, 1, 0),
    (695, 28, 50, 'Immolate upkeep',               0, 0, 1, 0),
    (696, 28, 60, 'Corruption upkeep',             0, 0, 1, 0),
    (697, 28, 70, 'Curse of Doom upkeep',          0, 0, 1, 0),
    (698, 28, 72, 'Curse of Agony upkeep',         0, 0, 1, 0),
    (699, 28, 80, 'Shadow Bolt filler',            0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (6900, 690, 0, 0,  1454, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6910, 691, 0, 0, 61610, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (6920, 692, 0, 0, 47836, 0, 0, 0, 'enemy_primary', 0,    4,   10),
    (6930, 693, 0, 0,  6353, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6940, 694, 0, 0, 29722, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6950, 695, 0, 0,   348, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6960, 696, 0, 0,   172, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6970, 697, 0, 0,   603, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6980, 698, 0, 0,   980, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (6990, 699, 0, 0,   686, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (6900, 690, 0, 'self',  'power_pct',           3, 35.0, ''),
    (6901, 690, 1, 'self',  'hp_pct',              5, 45.0, ''),
    (6920, 692, 0, 'enemy', 'nearby_enemies',      5,  4.0, '10'),
    (6930, 693, 0, 'enemy', 'hp_pct',              3, 35.0, ''),
    (6940, 694, 0, 'self',  'aura',                6,  0.0, '47245'),
    (6950, 695, 0, 'enemy', 'aura_remaining_secs', 3,  3.0, '47811'),
    (6960, 696, 0, 'enemy', 'aura',                7,  0.0, '47813'),
    (6970, 697, 0, 'enemy', 'hp_pct',              5, 50.0, ''),
    (6971, 697, 1, 'enemy', 'aura',                7,  0.0, '47867'),
    (6980, 698, 0, 'enemy', 'hp_pct',              3, 49.0, ''),
    (6981, 698, 1, 'enemy', 'aura',                7,  0.0, '47864');

-- -----------------------------------------------------------------------
-- DESTRUCTION WARLOCK PVE PROFILE (29)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (700, 29,  0, 'Life Tap (mana sustain)',       0, 0, 1, 0),
    (701, 29, 10, 'Seed of Corruption (4+ AoE)',   0, 0, 1, 0),
    (702, 29, 20, 'Shadowburn execute',            0, 0, 1, 0),
    (703, 29, 30, 'Immolate upkeep',               0, 0, 1, 0),
    (704, 29, 40, 'Conflagrate',                   0, 0, 1, 0),
    (705, 29, 50, 'Chaos Bolt',                    0, 0, 1, 0),
    (706, 29, 60, 'Curse of Doom upkeep',          0, 0, 1, 0),
    (707, 29, 62, 'Curse of Agony upkeep',         0, 0, 1, 0),
    (708, 29, 70, 'Incinerate filler',             0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (7000, 700, 0, 0,  1454, 0, 0, 0, 'self',          NULL, NULL, NULL),
    (7010, 701, 0, 0, 47836, 0, 0, 0, 'enemy_primary', 0,    4,   10),
    (7020, 702, 0, 0, 17877, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (7030, 703, 0, 0,   348, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (7040, 704, 0, 0, 17962, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (7050, 705, 0, 0, 50796, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (7060, 706, 0, 0,   603, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (7070, 707, 0, 0,   980, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (7080, 708, 0, 0, 29722, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (7000, 700, 0, 'self',  'power_pct',           3, 35.0, ''),
    (7001, 700, 1, 'self',  'hp_pct',              5, 45.0, ''),
    (7010, 701, 0, 'enemy', 'nearby_enemies',      5,  4.0, '10'),
    (7020, 702, 0, 'enemy', 'hp_pct',              3, 20.0, ''),
    (7030, 703, 0, 'enemy', 'aura_remaining_secs', 3,  3.0, '47811'),
    (7040, 704, 0, 'enemy', 'aura',                6,  0.0, '47811'),
    (7060, 706, 0, 'enemy', 'hp_pct',              5, 50.0, ''),
    (7061, 706, 1, 'enemy', 'aura',                7,  0.0, '47867'),
    (7070, 707, 0, 'enemy', 'hp_pct',              3, 49.0, ''),
    (7071, 707, 1, 'enemy', 'aura',                7,  0.0, '47864');
