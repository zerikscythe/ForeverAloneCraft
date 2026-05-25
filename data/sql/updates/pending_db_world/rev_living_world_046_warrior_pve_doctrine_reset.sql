-- rev_living_world_046_warrior_pve_doctrine_reset (world DB)
--
-- Clean Warrior PvE doctrine reset:
--   * replace the old starter Arms profile with a class-specific PvE profile
--   * rebuild the shallow Fury placeholder profile into a real PvE doctrine
--   * replace the old shared Warrior/Paladin tank starter rows with an explicit
--     Warrior Protection PvE tank profile
--   * refresh Warrior talent templates 1/2/3 against the current WotLK
--     Classic guide surface so the family is no longer living on v004/v005
--     starter scaffolding
--
-- Human/source notes:
--   Arms PvE:
--     https://www.icy-veins.com/wotlk-classic/arms-warrior-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/arms-warrior-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/arms-warrior-dps-pve-spec-builds-talents-glyphs
--   Fury PvE:
--     https://www.icy-veins.com/wotlk-classic/fury-warrior-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/fury-warrior-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/fury-warrior-dps-pve-spec-builds-talents-glyphs
--   Protection PvE:
--     https://www.icy-veins.com/wotlk-classic/protection-warrior-tank-pve-guide
--     https://www.icy-veins.com/wotlk-classic/protection-warrior-tank-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/protection-warrior-tank-pve-spec-builds-talents-glyphs
--
-- Local DBC-backed spell references used here:
--   Pummel             = 6552
--   Bladestorm         = 46924
--   Sweeping Strikes   = 12328
--   Execute            = 5308
--   Mortal Strike      = 12294
--   Rend               = 772 (rank upkeep checks use 47465)
--   Thunder Clap       = 6343
--   Slam               = 1464
--   Heroic Strike      = 78
--   Death Wish         = 12292
--   Whirlwind          = 1680
--   Bloodthirst        = 23881
--   Cleave             = 845
--   Shield Bash        = 72
--   Shield Block       = 2565
--   Shockwave          = 46968
--   Shield Slam        = 23922
--   Revenge            = 6572
--   Concussion Blow    = 12809
--   Devastate          = 20243
--
-- Design note:
--   These Warrior PvE profiles are intentionally level-80 leaning. Rend
--   upkeep uses the current high-rank aura id because the runtime is not spell-
--   chain aware for aura checks yet, and the endgame band is the fidelity band
--   we care about most for modern doctrine work.

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
    ( 1, 'Arms',       'DPS',  'Warrior', 'PvE', '', 'Warrior Arms DPS',
      'Priority-based PvE Arms Warrior profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 2, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (13, 'Protection', 'TANK', 'Warrior', 'PvE', '', 'Warrior Protection Tank',
      'Priority-based PvE Protection Warrior tank profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 2, 10, 1, 115, 210, 95, 235, 260, 145, 190),
    (19, 'Fury',       'DPS',  'Warrior', 'PvE', '', 'Warrior Fury DPS',
      'Priority-based PvE Fury Warrior profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 2, 10, 1, 100, 165, 80, 185, 240, 145, 175)
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
-- WARRIOR TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    (1, 'Arms',       1, '', 'Arms Warrior DPS',
        'PvE Arms Warrior talent template refreshed against current WotLK Classic guide material.'),
    (2, 'Fury',       1, '', 'Fury Warrior DPS',
        'PvE Fury Warrior talent template refreshed against current WotLK Classic guide material.'),
    (3, 'Protection', 1, '', 'Protection Warrior Tank',
        'PvE Protection Warrior talent template refreshed against current WotLK Classic guide material.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (1, 2, 3);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Arms Warrior
    (1,   0,  124, 'Improved Heroic Strike', 3),
    (1,   1,  130, 'Deflection', 5),
    (1,   2,  127, 'Improved Rend', 2),
    (1,  11,  641, 'Iron Will', 3),
    (1,  12,  128, 'Tactical Mastery', 3),
    (1,  21,  137, 'Anger Management', 1),
    (1,  22,  662, 'Impale', 2),
    (1,  23,  121, 'Deep Wounds', 3),
    (1,  31,  136, 'Two-Handed Weapon Specialization', 3),
    (1,  32, 2232, 'Taste for Blood', 3),
    (1,  41,  133, 'Sweeping Strikes', 1),
    (1,  50,  134, 'Weapon Mastery', 2),
    (1,  53, 1859, 'Trauma', 2),
    (1,  61,  135, 'Mortal Strike', 1),
    (1,  62, 1862, 'Strength of Arms', 2),
    (1,  70, 2283, 'Juggernaut', 1),
    (1,  71, 1824, 'Improved Mortal Strike', 3),
    (1,  80, 1662, 'Sudden Death', 3),
    (1,  82, 1664, 'Blood Frenzy', 2),
    (1,  91, 2231, 'Wrecking Crew', 5),
    (1, 101, 1863, 'Bladestorm', 1),
    (1, 202,  157, 'Cruelty', 5),
    (1, 212,  159, 'Unbridled Wrath', 5),
    (1, 223,  154, 'Commanding Presence', 5),
    (1, 230, 1581, 'Dual Wield Specialization', 5),

    -- Fury Warrior
    (2,   2,  157, 'Cruelty', 5),
    (2,  12,  159, 'Unbridled Wrath', 5),
    (2,  22,  661, 'Blood Craze', 3),
    (2,  23,  154, 'Commanding Presence', 5),
    (2,  30, 1581, 'Dual Wield Specialization', 5),
    (2,  32,  155, 'Enrage', 5),
    (2,  40, 1657, 'Precision', 3),
    (2,  41,  165, 'Death Wish', 1),
    (2,  52,  156, 'Flurry', 5),
    (2,  60, 1864, 'Intensify Rage', 3),
    (2,  61,  167, 'Bloodthirst', 1),
    (2,  70, 1865, 'Furious Attacks', 2),
    (2,  73, 1658, 'Improved Berserker Stance', 5),
    (2,  81, 1659, 'Rampage', 1),
    (2,  82, 1866, 'Bloodsurge', 3),
    (2,  91, 2234, 'Unending Fury', 5),
    (2, 101, 1867, 'Titan''s Grip', 1),
    (2, 201,  130, 'Deflection', 5),
    (2, 222,  662, 'Impale', 2),
    (2, 223,  121, 'Deep Wounds', 3),
    (2, 231,  136, 'Two-Handed Weapon Specialization', 3),

    -- Protection Warrior
    (3,   1, 1601, 'Shield Specialization', 5),
    (3,   2,  141, 'Improved Thunder Clap', 3),
    (3,  11,  144, 'Incite', 3),
    (3,  12,  138, 'Anticipation', 5),
    (3,  21,  147, 'Improved Revenge', 2),
    (3,  22, 1654, 'Shield Mastery', 2),
    (3,  23,  140, 'Toughness', 5),
    (3,  31,  151, 'Improved Disarm', 2),
    (3,  32,  146, 'Puncture', 3),
    (3,  41,  152, 'Concussion Blow', 1),
    (3,  42,  149, 'Gag Order', 2),
    (3,  52,  702, 'One-Handed Weapon Specialization', 5),
    (3,  60, 1652, 'Improved Defensive Stance', 2),
    (3,  61,  148, 'Vigilance', 1),
    (3,  62, 1660, 'Focused Rage', 3),
    (3,  71, 1653, 'Vitality', 3),
    (3,  72, 1870, 'Safeguard', 2),
    (3,  80, 2236, 'Warbringer', 1),
    (3,  81, 1666, 'Devastate', 1),
    (3,  82, 1893, 'Critical Block', 3),
    (3,  91, 1871, 'Sword and Board', 3),
    (3,  92, 2246, 'Damage Shield', 2),
    (3, 101, 1872, 'Shockwave', 1),
    (3, 200,  124, 'Improved Heroic Strike', 3),
    (3, 201,  130, 'Deflection', 5),
    (3, 223,  121, 'Deep Wounds', 3);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (
    1, 2, 3, 4, 5,
    60, 61, 62, 63, 64,
    190, 191, 192, 193, 194,
    460, 461, 462, 463, 464, 465, 466, 467, 468,
    470, 471, 472, 473, 474, 475, 476, 477,
    480, 481, 482, 483, 484, 485, 486, 487, 488, 489
);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (
    1, 2, 3, 4, 5,
    60, 61, 62, 63, 64,
    190, 191, 192, 193, 194,
    460, 461, 462, 463, 464, 465, 466, 467, 468,
    470, 471, 472, 473, 474, 475, 476, 477,
    480, 481, 482, 483, 484, 485, 486, 487, 488, 489
);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (
    1, 2, 3, 4, 5,
    60, 61, 62, 63, 64,
    190, 191, 192, 193, 194,
    460, 461, 462, 463, 464, 465, 466, 467, 468,
    470, 471, 472, 473, 474, 475, 476, 477,
    480, 481, 482, 483, 484, 485, 486, 487, 488, 489
);

-- -----------------------------------------------------------------------
-- ARMS WARRIOR PVE PROFILE (1)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (460, 1,  0, 'Pummel',                       1, 1, 1, 0),
    (461, 1, 10, 'Bladestorm (3+ targets)',     0, 0, 1, 0),
    (462, 1, 20, 'Sweeping Strikes (2+ targets)',0, 0, 1, 0),
    (463, 1, 30, 'Execute',                     0, 0, 1, 0),
    (464, 1, 40, 'Mortal Strike',               0, 0, 1, 0),
    (465, 1, 50, 'Rend upkeep',                 0, 0, 1, 0),
    (466, 1, 60, 'Thunder Clap (3+ targets)',   0, 0, 1, 0),
    (467, 1, 70, 'Slam',                        0, 0, 1, 0),
    (468, 1, 80, 'Heroic Strike',               0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4600, 460, 0, 0,  6552, 0, 0, 0, 'enemy_primary'),
    (4610, 461, 0, 0, 46924, 0, 0, 0, 'self'),
    (4620, 462, 0, 0, 12328, 0, 0, 0, 'self'),
    (4630, 463, 0, 0,  5308, 0, 0, 0, 'enemy_primary'),
    (4640, 464, 0, 0, 12294, 0, 0, 0, 'enemy_primary'),
    (4650, 465, 0, 0,   772, 0, 0, 0, 'enemy_primary'),
    (4660, 466, 0, 0,  6343, 0, 0, 0, 'enemy_primary'),
    (4670, 467, 0, 0,  1464, 0, 0, 0, 'enemy_primary'),
    (4680, 468, 0, 0,    78, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4610, 461, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (4620, 462, 0, 'enemy', 'nearby_enemies',     5, 2.0,  '10'),
    (4630, 463, 0, 'enemy', 'hp_pct',             3, 20.0, ''),
    (4650, 465, 0, 'enemy', 'aura_remaining_secs',3, 3.0,  '47465'),
    (4660, 466, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (4670, 467, 0, 'self',  'power_pct',          5, 15.0, ''),
    (4680, 468, 0, 'self',  'power_pct',          5, 55.0, '');

-- -----------------------------------------------------------------------
-- FURY WARRIOR PVE PROFILE (19)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (470, 19,  0, 'Pummel',                  1, 1, 1, 0),
    (471, 19, 10, 'Death Wish',              0, 0, 1, 0),
    (472, 19, 20, 'Execute',                 0, 0, 1, 0),
    (473, 19, 30, 'Whirlwind',               0, 0, 1, 0),
    (474, 19, 40, 'Bloodthirst',             0, 0, 1, 0),
    (475, 19, 50, 'Cleave (2+ targets)',     0, 0, 1, 0),
    (476, 19, 60, 'Slam',                    0, 0, 1, 0),
    (477, 19, 70, 'Heroic Strike',           0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4700, 470, 0, 0,  6552, 0, 0, 0, 'enemy_primary'),
    (4710, 471, 0, 0, 12292, 0, 0, 0, 'self'),
    (4720, 472, 0, 0,  5308, 0, 0, 0, 'enemy_primary'),
    (4730, 473, 0, 0,  1680, 0, 0, 0, 'enemy_primary'),
    (4740, 474, 0, 0, 23881, 0, 1, 0, 'enemy_primary'),
    (4750, 475, 0, 0,   845, 0, 0, 0, 'enemy_primary'),
    (4760, 476, 0, 0,  1464, 0, 0, 0, 'enemy_primary'),
    (4770, 477, 0, 0,    78, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4720, 472, 0, 'enemy', 'hp_pct',         3, 20.0, ''),
    (4750, 475, 0, 'enemy', 'nearby_enemies', 5, 2.0,  '10'),
    (4751, 475, 1, 'self',  'power_pct',      5, 45.0, ''),
    (4760, 476, 0, 'self',  'power_pct',      5, 15.0, ''),
    (4770, 477, 0, 'enemy', 'nearby_enemies', 2, 2.0,  '10'),
    (4771, 477, 1, 'self',  'power_pct',      5, 55.0, '');

-- -----------------------------------------------------------------------
-- PROTECTION WARRIOR PVE PROFILE (13)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (480, 13,  0, 'Shield Bash',                    1, 1, 1, 0),
    (481, 13, 10, 'Shield Block',                   0, 0, 1, 0),
    (482, 13, 20, 'Shockwave (3+ targets)',         0, 0, 1, 0),
    (483, 13, 30, 'Thunder Clap (2+ targets)',      0, 0, 1, 0),
    (484, 13, 40, 'Shield Slam (Shield Block up)',  0, 0, 1, 0),
    (485, 13, 50, 'Shield Slam',                    0, 0, 1, 0),
    (486, 13, 60, 'Revenge',                        0, 0, 1, 0),
    (487, 13, 70, 'Concussion Blow',                0, 0, 1, 0),
    (488, 13, 80, 'Devastate',                      0, 0, 1, 0),
    (489, 13, 90, 'Heroic Strike',                  0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4800, 480, 0, 0,    72, 0, 0, 0, 'enemy_primary'),
    (4810, 481, 0, 0,  2565, 0, 0, 0, 'self'),
    (4820, 482, 0, 0, 46968, 0, 0, 0, 'enemy_primary'),
    (4830, 483, 0, 0,  6343, 0, 0, 0, 'enemy_primary'),
    (4840, 484, 0, 0, 23922, 0, 0, 0, 'enemy_primary'),
    (4850, 485, 0, 0, 23922, 0, 0, 0, 'enemy_primary'),
    (4860, 486, 0, 0,  6572, 0, 0, 0, 'enemy_primary'),
    (4870, 487, 0, 0, 12809, 0, 0, 0, 'enemy_primary'),
    (4880, 488, 0, 0, 20243, 0, 0, 0, 'enemy_primary'),
    (4890, 489, 0, 0,    78, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4810, 481, 0, 'self',  'is_aggro_holder', 0, 1.0,  ''),
    (4820, 482, 0, 'enemy', 'nearby_enemies',  5, 3.0,  '10'),
    (4830, 483, 0, 'enemy', 'nearby_enemies',  5, 2.0,  '10'),
    (4840, 484, 0, 'self',  'aura',            6, 0.0,  '2565'),
    (4890, 489, 0, 'self',  'power_pct',       5, 55.0, '');
