-- rev_living_world_044_rogue_pve_doctrine_reset (world DB)
--
-- Clean Rogue PvE doctrine reset:
--   * replace the old starter Combat profile with a class-specific PvE profile
--   * rebuild the shallow Assassination/Subtlety placeholder rows into real PvE
--     doctrine profiles
--   * refresh the Combat talent template and add curated Assassination/Subtlety
--     talent templates from current WotLK Classic guide material
--
-- Human/source notes:
--   Assassination PvE:
--     https://www.icy-veins.com/wotlk-classic/assassination-rogue-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/assassination-rogue-dps-pve-rotation-cooldowns-abilities
--   Combat PvE:
--     https://www.icy-veins.com/wotlk-classic/combat-rogue-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/combat-rogue-dps-pve-rotation-cooldowns-abilities
--   Subtlety PvE:
--     https://www.icy-veins.com/wotlk-classic/subtlety-rogue-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/subtlety-rogue-dps-pve-rotation-cooldowns-abilities
--   Pre-raid BiS references for the next loadout pass:
--     https://www.wowhead.com/wotlk/guide/classes/rogue/assassination/dps-bis-gear-pre-raid-pve
--     https://www.wowhead.com/wotlk/guide/classes/rogue/combat/dps-bis-gear-pre-raid-pve
--     https://www.wowhead.com/wotlk/guide/classes/rogue/subtlety/dps-bis-gear-pre-raid-pve-p4
--
-- Local DBC-backed spell references used here:
--   Kick               = 1766
--   Feint              = 1966
--   Sinister Strike    = 1752
--   Eviscerate         = 2098
--   Slice and Dice     = 5171 (rank upkeep checks use 6774)
--   Rupture            = 1943 (rank upkeep checks use 48672)
--   Fan of Knives      = 51723
--   Blade Flurry       = 13877
--   Adrenaline Rush    = 13750
--   Killing Spree      = 51690
--   Cold Blood         = 14177
--   Mutilate           = 1329
--   Envenom            = 32645
--   Hemorrhage         = 16511 (rank upkeep checks use 48660)
--   Expose Armor       = 48669
--   Ghostly Strike     = 14278
--   Shadow Dance       = 51713
--
-- Design note:
--   These Rogue PvE profiles are intentionally level-80 leaning. Aura upkeep
--   uses exact high-rank spell ids because the current runtime is not spell-
--   chain aware for aura checks. That makes the behavior much cleaner for the
--   pre-raid/raid band we actually care about for doctrine fidelity.

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
    ( 4, 'Combat',        'DPS', 'Rogue', 'PvE', '', 'Rogue Combat DPS',
      'Priority-based PvE Combat Rogue profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 3, 10, 1, 100, 160, 75, 185, 235, 145, 175),
    (22, 'Assassination', 'DPS', 'Rogue', 'PvE', '', 'Rogue Assassination DPS',
      'Priority-based PvE Assassination Rogue profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 6, 10, 1, 100, 150, 70, 180, 235, 145, 170),
    (23, 'Subtlety',      'DPS', 'Rogue', 'PvE', '', 'Rogue Subtlety DPS',
      'Utility-flavored PvE Subtlety Rogue profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 6, 10, 1,  95, 170, 80, 190, 240, 150, 170)
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
-- ROGUE TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    ( 8, 'Combat',        4, '', 'Combat Rogue DPS',
        '20/51/0 PvE Combat Rogue talent template based on current WotLK Classic guide material.'),
    (21, 'Assassination', 4, '', 'Assassination Rogue DPS',
        '51/13/7 PvE Assassination Rogue talent template based on current WotLK Classic guide material.'),
    (22, 'Subtlety',      4, '', 'Subtlety Rogue DPS',
        '19/0/52 PvE Subtlety Rogue talent template based on current WotLK Classic guide material.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (8, 21, 22);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Combat Rogue (20 / 51 / 0)
    ( 8,   0,  270, 'Malice', 5),
    ( 8,  10,  273, 'Ruthlessness', 3),
    ( 8,  20, 2068, 'Blood Spatter', 2),
    ( 8,  30,  269, 'Lethality', 5),
    ( 8,  40,  682, 'Vile Poisons', 1),
    ( 8,  50,  268, 'Improved Poisons', 4),
    ( 8,  60,  201, 'Improved Sinister Strike', 2),
    ( 8,  70,  221, 'Dual Wield Specialization', 5),
    ( 8,  80, 1827, 'Improved Slice and Dice', 2),
    ( 8,  90,  181, 'Precision', 5),
    ( 8, 100,  204, 'Endurance', 1),
    ( 8, 110,  182, 'Close Quarters Combat', 5),
    ( 8, 120,  186, 'Lightning Reflexes', 3),
    ( 8, 130, 1122, 'Aggression', 5),
    ( 8, 140,  223, 'Blade Flurry', 1),
    ( 8, 150, 1703, 'Weapon Expertise', 2),
    ( 8, 160, 1706, 'Blade Twisting', 2),
    ( 8, 170, 1705, 'Vitality', 3),
    ( 8, 180,  205, 'Adrenaline Rush', 1),
    ( 8, 190, 1825, 'Combat Potency', 5),
    ( 8, 200, 1709, 'Surprise Attacks', 1),
    ( 8, 210, 2074, 'Savage Combat', 2),
    ( 8, 220, 2075, 'Prey on the Weak', 5),
    ( 8, 230, 2076, 'Killing Spree', 1),

    -- Assassination Rogue (51 / 13 / 7)
    (21,   0,  270, 'Malice', 5),
    (21,  10,  273, 'Ruthlessness', 3),
    (21,  20,  277, 'Puncturing Wounds', 3),
    (21,  30,  269, 'Lethality', 5),
    (21,  40,  682, 'Vile Poisons', 3),
    (21,  50,  268, 'Improved Poisons', 5),
    (21,  60, 1721, 'Fleet Footed', 2),
    (21,  70,  280, 'Cold Blood', 1),
    (21,  80,  283, 'Seal Fate', 5),
    (21,  90,  274, 'Murder', 2),
    (21, 100,  281, 'Overkill', 1),
    (21, 110, 2069, 'Focused Attacks', 3),
    (21, 120, 1718, 'Find Weakness', 3),
    (21, 130, 1715, 'Master Poisoner', 3),
    (21, 140, 1719, 'Mutilate', 1),
    (21, 150, 2070, 'Cut to the Chase', 5),
    (21, 160, 2071, 'Hunger For Blood', 1),
    (21, 170,  221, 'Dual Wield Specialization', 5),
    (21, 180,  181, 'Precision', 5),
    (21, 190,  182, 'Close Quarters Combat', 3),
    (21, 200, 2244, 'Relentless Strikes', 5),
    (21, 210,  261, 'Opportunity', 2),

    -- Subtlety Rogue (19 / 0 / 52)
    (22,   0,  270, 'Malice', 5),
    (22,  10,  273, 'Ruthlessness', 3),
    (22,  20, 2068, 'Blood Spatter', 2),
    (22,  30,  269, 'Lethality', 5),
    (22,  40,  682, 'Vile Poisons', 1),
    (22,  50,  268, 'Improved Poisons', 3),
    (22,  60, 2244, 'Relentless Strikes', 5),
    (22,  70,  261, 'Opportunity', 2),
    (22,  80,  244, 'Camouflage', 3),
    (22,  90,  247, 'Elusiveness', 2),
    (22, 100,  303, 'Ghostly Strike', 1),
    (22, 110, 1123, 'Serrated Blades', 3),
    (22, 120,  245, 'Initiative', 3),
    (22, 130,  263, 'Improved Ambush', 2),
    (22, 140,  284, 'Preparation', 1),
    (22, 150,  265, 'Dirty Deeds', 2),
    (22, 160,  681, 'Hemorrhage', 1),
    (22, 170, 1713, 'Master of Subtlety', 3),
    (22, 180, 1702, 'Deadliness', 5),
    (22, 190,  381, 'Premeditation', 1),
    (22, 200, 1722, 'Cheat Death', 1),
    (22, 210, 1712, 'Sinister Calling', 5),
    (22, 220, 2078, 'Honor Among Thieves', 3),
    (22, 230, 1714, 'Shadowstep', 1),
    (22, 240, 2079, 'Filthy Tricks', 2),
    (22, 250, 2080, 'Slaughter from the Shadows', 5),
    (22, 260, 2081, 'Shadow Dance', 1);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (
    14, 15, 16, 17,
    220, 221, 222, 223, 224,
    230, 231, 232, 233, 234,
    410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420,
    430, 431, 432, 433, 434, 435, 436, 437,
    450, 451, 452, 453, 454, 455, 456, 457, 458, 459
);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (
    14, 15, 16, 17,
    220, 221, 222, 223, 224,
    230, 231, 232, 233, 234,
    410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420,
    430, 431, 432, 433, 434, 435, 436, 437,
    450, 451, 452, 453, 454, 455, 456, 457, 458, 459
);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (
    14, 15, 16, 17,
    220, 221, 222, 223, 224,
    230, 231, 232, 233, 234,
    410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420,
    430, 431, 432, 433, 434, 435, 436, 437,
    450, 451, 452, 453, 454, 455, 456, 457, 458, 459
);

-- -----------------------------------------------------------------------
-- COMBAT ROGUE PVE PROFILE (4)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (410, 4,  0, 'Kick',                               1, 1, 1, 0),
    (411, 4,  2, 'Feint',                              0, 0, 1, 1),
    (412, 4, 10, 'Blade Flurry (2+ targets)',          0, 0, 1, 0),
    (413, 4, 20, 'Killing Spree',                      0, 0, 1, 0),
    (414, 4, 30, 'Adrenaline Rush',                    0, 0, 1, 0),
    (415, 4, 40, 'Fan of Knives (3+ targets)',         0, 0, 1, 0),
    (416, 4, 50, 'Slice and Dice upkeep',              0, 0, 1, 0),
    (417, 4, 60, 'Rupture upkeep',                     0, 0, 1, 0),
    (418, 4, 65, 'Eviscerate (Blade Flurry cleave)',   0, 0, 1, 0),
    (419, 4, 70, 'Eviscerate',                         0, 0, 1, 0),
    (420, 4, 80, 'Sinister Strike',                    0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4100, 410, 0, 0,  1766, 0, 0, 0, 'enemy_primary'),
    (4110, 411, 0, 0,  1966, 0, 0, 0, 'self'),
    (4120, 412, 0, 0, 13877, 0, 0, 0, 'self'),
    (4130, 413, 0, 0, 51690, 0, 0, 0, 'enemy_primary'),
    (4140, 414, 0, 0, 13750, 0, 0, 0, 'self'),
    (4150, 415, 0, 0, 51723, 0, 0, 0, 'enemy_primary'),
    (4160, 416, 0, 0,  5171, 0, 0, 0, 'self'),
    (4170, 417, 0, 0,  1943, 0, 0, 0, 'enemy_primary'),
    (4180, 418, 0, 0,  2098, 0, 0, 0, 'enemy_primary'),
    (4190, 419, 0, 0,  2098, 0, 0, 0, 'enemy_primary'),
    (4200, 420, 0, 0,  1752, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4110, 411, 0, 'self',  'threat_pct',         5, 90.0, ''),
    (4111, 411, 1, 'self',  'is_aggro_holder',    0, 1.0,  ''),
    (4120, 412, 0, 'enemy', 'nearby_enemies',     5, 2.0,  '10'),
    (4130, 413, 0, 'enemy', 'distance',           3, 10.0, ''),
    (4140, 414, 0, 'self',  'power_pct',          3, 40.0, ''),
    (4150, 415, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (4160, 416, 0, 'self',  'combo_points',       5, 2.0,  ''),
    (4161, 416, 1, 'self',  'aura_remaining_secs',3, 2.0,  '6774'),
    (4170, 417, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4171, 417, 1, 'enemy', 'aura_remaining_secs',3, 3.0,  '48672'),
    (4172, 417, 2, 'self',  'aura',               7, 0.0,  '13877'),
    (4180, 418, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4181, 418, 1, 'self',  'aura',               6, 0.0,  '13877'),
    (4182, 418, 2, 'self',  'aura_remaining_secs',4, 2.0,  '6774'),
    (4190, 419, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4191, 419, 1, 'self',  'aura_remaining_secs',4, 2.0,  '6774'),
    (4192, 419, 2, 'enemy', 'aura_remaining_secs',4, 3.0,  '48672');

-- -----------------------------------------------------------------------
-- ASSASSINATION ROGUE PVE PROFILE (22)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (430, 22,  0, 'Kick',                               1, 1, 1, 0),
    (431, 22,  2, 'Feint',                              0, 0, 1, 1),
    (432, 22, 10, 'Cold Blood',                         0, 0, 1, 0),
    (433, 22, 20, 'Fan of Knives (6+ targets)',         0, 0, 1, 0),
    (434, 22, 30, 'Slice and Dice upkeep',              0, 0, 1, 0),
    (435, 22, 40, 'Rupture upkeep',                     0, 0, 1, 0),
    (436, 22, 50, 'Envenom',                            0, 0, 1, 0),
    (437, 22, 60, 'Mutilate',                           0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4300, 430, 0, 0,  1766, 0, 0, 0, 'enemy_primary'),
    (4310, 431, 0, 0,  1966, 0, 0, 0, 'self'),
    (4320, 432, 0, 0, 14177, 0, 0, 0, 'self'),
    (4330, 433, 0, 0, 51723, 0, 0, 0, 'enemy_primary'),
    (4340, 434, 0, 0,  5171, 0, 0, 0, 'self'),
    (4350, 435, 0, 0,  1943, 0, 0, 0, 'enemy_primary'),
    (4360, 436, 0, 0, 32645, 0, 0, 0, 'enemy_primary'),
    (4370, 437, 0, 0,  1329, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4310, 431, 0, 'self',  'threat_pct',         5, 90.0, ''),
    (4311, 431, 1, 'self',  'is_aggro_holder',    0, 1.0,  ''),
    (4320, 432, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4330, 433, 0, 'enemy', 'nearby_enemies',     5, 6.0,  '10'),
    (4340, 434, 0, 'self',  'combo_points',       5, 2.0,  ''),
    (4341, 434, 1, 'self',  'aura_remaining_secs',3, 2.0,  '6774'),
    (4350, 435, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4351, 435, 1, 'enemy', 'aura_remaining_secs',3, 3.0,  '48672'),
    (4360, 436, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4361, 436, 1, 'self',  'aura_remaining_secs',4, 2.0,  '6774');

-- -----------------------------------------------------------------------
-- SUBTLETY ROGUE PVE PROFILE (23)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (450, 23,  0, 'Kick',                               1, 1, 1, 0),
    (451, 23,  2, 'Feint',                              0, 0, 1, 1),
    (452, 23, 10, 'Shadow Dance',                       0, 0, 1, 0),
    (453, 23, 20, 'Fan of Knives (6+ targets)',         0, 0, 1, 0),
    (454, 23, 30, 'Expose Armor upkeep',                0, 0, 1, 0),
    (455, 23, 40, 'Slice and Dice upkeep',              0, 0, 1, 0),
    (456, 23, 50, 'Rupture upkeep',                     0, 0, 1, 0),
    (457, 23, 60, 'Eviscerate',                         0, 0, 1, 0),
    (458, 23, 70, 'Ghostly Strike',                     0, 0, 1, 0),
    (459, 23, 80, 'Hemorrhage',                         0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4500, 450, 0, 0,  1766, 0, 0, 0, 'enemy_primary'),
    (4510, 451, 0, 0,  1966, 0, 0, 0, 'self'),
    (4520, 452, 0, 0, 51713, 0, 0, 0, 'self'),
    (4530, 453, 0, 0, 51723, 0, 0, 0, 'enemy_primary'),
    (4540, 454, 0, 0, 48669, 0, 1, 0, 'enemy_primary'),
    (4550, 455, 0, 0,  5171, 0, 0, 0, 'self'),
    (4560, 456, 0, 0,  1943, 0, 0, 0, 'enemy_primary'),
    (4570, 457, 0, 0,  2098, 0, 0, 0, 'enemy_primary'),
    (4580, 458, 0, 0, 14278, 0, 0, 0, 'enemy_primary'),
    (4590, 459, 0, 0, 16511, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4510, 451, 0, 'self',  'threat_pct',         5, 90.0, ''),
    (4511, 451, 1, 'self',  'is_aggro_holder',    0, 1.0,  ''),
    (4520, 452, 0, 'self',  'power_pct',          3, 60.0, ''),
    (4530, 453, 0, 'enemy', 'nearby_enemies',     5, 6.0,  '10'),
    (4540, 454, 0, 'self',  'combo_points',       5, 5.0,  ''),
    (4541, 454, 1, 'enemy', 'aura_remaining_secs',3, 3.0,  '48669'),
    (4550, 455, 0, 'self',  'combo_points',       5, 2.0,  ''),
    (4551, 455, 1, 'self',  'aura_remaining_secs',3, 2.0,  '6774'),
    (4560, 456, 0, 'self',  'combo_points',       5, 4.0,  ''),
    (4561, 456, 1, 'enemy', 'aura_remaining_secs',3, 3.0,  '48672'),
    (4570, 457, 0, 'self',  'combo_points',       5, 5.0,  ''),
    (4571, 457, 1, 'self',  'aura_remaining_secs',4, 2.0,  '6774'),
    (4572, 457, 2, 'enemy', 'aura_remaining_secs',4, 3.0,  '48672');
