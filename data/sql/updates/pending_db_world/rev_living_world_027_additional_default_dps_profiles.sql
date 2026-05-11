-- rev_living_world_027_additional_default_dps_profiles (world DB)
--
-- Fills out additional class/spec default combat profiles that already exist as
-- placeholders in some live environments and are now required by the canonical
-- world-bot identity flavor set.

-- -----------------------------------------------------------------------
-- DEFAULT PROFILES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_profile` (
    `default_profile_id`, `spec_key`, `role_key`, `class_key`, `display_name`,
    `conservation_mode`, `resource_low_water`, `resource_high_water`,
    `enable_down_rank`, `down_rank_floor`,
    `default_aoe_mode`, `default_aoe_min_targets`, `default_aoe_scan_radius`
) VALUES
    (19, 'Fury',         'DPS', 'Warrior',      'Warrior Fury DPS',              0,  0, 100, 0, 0, 0, 2, 10),
    (20, 'Marksmanship', 'DPS', 'Hunter',       'Hunter Marksmanship DPS',       0,  0, 100, 0, 0, 0, 2, 10),
    (21, 'Survival',     'DPS', 'Hunter',       'Hunter Survival DPS',           0,  0, 100, 0, 0, 0, 2, 10),
    (22, 'Assassination','DPS', 'Rogue',        'Rogue Assassination DPS',       0,  0, 100, 0, 0, 0, 2, 10),
    (23, 'Subtlety',     'DPS', 'Rogue',        'Rogue Subtlety DPS',            0,  0, 100, 0, 0, 0, 2, 10),
    (25, 'Enhancement',  'DPS', 'Shaman',       'Shaman Enhancement DPS',        1, 55, 75, 1, 2, 0, 2, 10),
    (26, 'Arcane',       'DPS', 'Mage',         'Mage Arcane DPS',               1, 50, 75, 1, 2, 0, 2, 10),
    (27, 'Fire',         'DPS', 'Mage',         'Mage Fire DPS',                 1, 50, 75, 1, 2, 0, 2, 10),
    (28, 'Demonology',   'DPS', 'Warlock',      'Warlock Demonology DPS',        1, 45, 70, 0, 0, 0, 2, 10),
    (29, 'Destruction',  'DPS', 'Warlock',      'Warlock Destruction DPS',       1, 45, 70, 0, 0, 0, 2, 10),
    (30, 'Feral',        'DPS', 'Druid',        'Druid Feral DPS',               0,  0, 100, 0, 0, 0, 2, 10),
    (34, 'Frost',        'DPS', 'Death Knight', 'Death Knight Frost DPS',        0,  0, 100, 0, 0, 0, 2, 10)
ON DUPLICATE KEY UPDATE
    `spec_key`               = VALUES(`spec_key`),
    `role_key`               = VALUES(`role_key`),
    `class_key`              = VALUES(`class_key`),
    `display_name`           = VALUES(`display_name`),
    `conservation_mode`      = VALUES(`conservation_mode`),
    `resource_low_water`     = VALUES(`resource_low_water`),
    `resource_high_water`    = VALUES(`resource_high_water`),
    `enable_down_rank`       = VALUES(`enable_down_rank`),
    `down_rank_floor`        = VALUES(`down_rank_floor`),
    `default_aoe_mode`       = VALUES(`default_aoe_mode`),
    `default_aoe_min_targets`= VALUES(`default_aoe_min_targets`),
    `default_aoe_scan_radius`= VALUES(`default_aoe_scan_radius`);

-- -----------------------------------------------------------------------
-- ENTRIES
-- Entry IDs 190-355 reserved for these additional profiles.
-- Legacy Frost Mage starter rows (33-37) are disabled below and replaced with
-- a smarter Frost Mage profile pass using entries 350-355.
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    -- Profile 8: Frost Mage DPS (legacy starter rows disabled)
    ( 33,  8, 90, 'Legacy Frostfire Bolt (disabled)',          0, 0, 0, 0),
    ( 34,  8, 91, 'Legacy Frostbolt (disabled)',               0, 0, 0, 0),
    ( 35,  8, 92, 'Legacy Ice Lance (disabled)',               0, 0, 0, 0),
    ( 36,  8, 93, 'Legacy Arcane Missiles (disabled)',         0, 0, 0, 0),
    ( 37,  8, 94, 'Legacy Fire Blast (disabled)',              0, 0, 0, 0),

    -- Profile 19: Fury Warrior DPS
    (190, 19,  0, 'Execute',                0, 0, 1, 0),
    (191, 19, 10, 'Bloodthirst',            0, 0, 1, 0),
    (192, 19, 20, 'Whirlwind',              0, 0, 1, 0),
    (193, 19, 30, 'Slam',                   0, 0, 1, 0),
    (194, 19, 40, 'Heroic Strike',          0, 0, 1, 0),

    -- Profile 20: Marksmanship Hunter DPS
    (200, 20,  0, 'Serpent Sting',          0, 0, 1, 0),
    (201, 20, 10, 'Chimera Shot',           0, 0, 1, 0),
    (202, 20, 20, 'Aimed Shot',             0, 0, 1, 0),
    (203, 20, 30, 'Steady Shot',            0, 0, 1, 0),
    (204, 20, 40, 'Arcane Shot',            0, 0, 1, 0),

    -- Profile 21: Survival Hunter DPS
    (210, 21,  0, 'Serpent Sting',          0, 0, 1, 0),
    (211, 21, 10, 'Explosive Shot',         0, 0, 1, 0),
    (212, 21, 20, 'Black Arrow',            0, 0, 1, 0),
    (213, 21, 30, 'Aimed Shot / Multi-Shot',0, 0, 1, 0),
    (214, 21, 40, 'Steady Shot / Arcane',   0, 0, 1, 0),

    -- Profile 22: Assassination Rogue DPS
    (220, 22,  0, 'Envenom (4+ CP)',        0, 0, 1, 0),
    (221, 22, 10, 'Slice and Dice (2+ CP)', 0, 0, 1, 0),
    (222, 22, 20, 'Rupture (3+ CP)',        0, 0, 1, 0),
    (223, 22, 30, 'Mutilate',               0, 0, 1, 0),
    (224, 22, 40, 'Eviscerate (4+ CP)',     0, 0, 1, 0),

    -- Profile 23: Subtlety Rogue DPS
    (230, 23,  0, 'Eviscerate (4+ CP)',     0, 0, 1, 0),
    (231, 23, 10, 'Slice and Dice (2+ CP)', 0, 0, 1, 0),
    (232, 23, 20, 'Rupture (3+ CP)',        0, 0, 1, 0),
    (233, 23, 30, 'Hemorrhage',             0, 0, 1, 0),
    (234, 23, 40, 'Backstab',               0, 0, 1, 0),

    -- Profile 25: Enhancement Shaman DPS
    (250, 25,  0, 'Stormstrike',            0, 0, 1, 0),
    (251, 25, 10, 'Lava Lash',              0, 0, 1, 0),
    (252, 25, 20, 'Flame Shock',            0, 0, 1, 0),
    (253, 25, 30, 'Earth Shock',            0, 0, 1, 0),
    (254, 25, 40, 'Lightning Bolt',         0, 0, 1, 0),

    -- Profile 26: Arcane Mage DPS
    -- One smart profile with AoE branching, Arcane Blast stack building,
    -- Missile Barrage consumption, and instant fallback actions for motion.
    (260, 26,  0, 'Blizzard / Arcane Explosion (3+ AoE)',      0, 0, 1, 0),
    (261, 26, 10, 'Arcane Missiles (Missile Barrage 3+ stacks)',0, 0, 1, 0),
    (262, 26, 20, 'Arcane Blast',                              0, 0, 1, 0),
    (263, 26, 40, 'Arcane Barrage',                            0, 0, 1, 0),
    (264, 26, 50, 'Fire Blast',                                0, 0, 1, 0),

    -- Profile 27: Fire Mage DPS
    -- One smart profile with AoE branches, Living Bomb upkeep, Hot Streak,
    -- Scorch debuff maintenance, Fireball filler, and instant fallback.
    (270, 27,  0, 'Flamestrike / Arcane Explosion (4+ AoE)',   0, 0, 1, 0),
    (271, 27,  5, 'Blizzard / Arcane Explosion (3+ AoE)',      0, 0, 1, 0),
    (272, 27, 10, 'Living Bomb upkeep',                        0, 0, 1, 0),
    (273, 27, 20, 'Pyroblast (Hot Streak)',                    0, 0, 1, 0),
    (274, 27, 30, 'Scorch (Improved Scorch upkeep)',           0, 0, 1, 0),
    (275, 27, 40, 'Fireball',                                  0, 0, 1, 0),
    (276, 27, 50, 'Fire Blast',                                0, 0, 1, 0),

    -- Profile 28: Demonology Warlock DPS
    (280, 28,  0, 'Corruption',             0, 0, 1, 0),
    (281, 28, 10, 'Immolate',               0, 0, 1, 0),
    (282, 28, 20, 'Curse of Agony',         0, 0, 1, 0),
    (283, 28, 30, 'Incinerate',             0, 0, 1, 0),
    (284, 28, 40, 'Shadow Bolt',            0, 0, 1, 0),

    -- Profile 29: Destruction Warlock DPS
    (290, 29,  0, 'Immolate',               0, 0, 1, 0),
    (291, 29, 10, 'Conflagrate',            0, 0, 1, 0),
    (292, 29, 20, 'Chaos Bolt',             0, 0, 1, 0),
    (293, 29, 30, 'Incinerate',             0, 0, 1, 0),
    (294, 29, 40, 'Shadow Bolt',            0, 0, 1, 0),

    -- Profile 30: Feral Druid DPS
    (300, 30,  0, 'Rip (4+ CP)',            0, 0, 1, 0),
    (301, 30, 10, 'Ferocious Bite (4+ CP)', 0, 0, 1, 0),
    (302, 30, 20, 'Rake',                   0, 0, 1, 0),
    (303, 30, 30, 'Mangle Cat',             0, 0, 1, 0),
    (304, 30, 40, 'Claw',                   0, 0, 1, 0),

    -- Profile 8: Frost Mage DPS
    -- One smart profile with AoE branching, Fingers of Frost / Brain Freeze
    -- proc handling, Frostbolt filler, and instant movement-friendly fallbacks.
    (350,  8,  0, 'Blizzard / Arcane Explosion (3+ AoE)',      0, 0, 1, 0),
    (351,  8, 10, 'Deep Freeze (Fingers of Frost)',            0, 0, 1, 0),
    (352,  8, 20, 'Frostfire Bolt (Brain Freeze)',             0, 0, 1, 0),
    (353,  8, 30, 'Frostbolt',                                 0, 0, 1, 0),
    (354,  8, 40, 'Ice Lance',                                 0, 0, 1, 0),
    (355,  8, 50, 'Fire Blast',                                0, 0, 1, 0),

    -- Profile 34: Frost Death Knight DPS
    -- One smart profile with disease upkeep, single-target rune spenders,
    -- runic-power dump rules, and AoE branches keyed off nearby_enemies.
    (340, 34,  0, 'Icy Touch (Frost Fever upkeep)',            0, 0, 1, 1),
    (341, 34,  5, 'Plague Strike (Blood Plague upkeep)',       0, 0, 1, 1),
    (345, 34, 10, 'Death and Decay / Howling Blast (3+ AoE)',  0, 0, 1, 0),
    (344, 34, 15, 'Howling Blast (2+ AoE)',                    0, 0, 1, 0),
    (346, 34, 20, 'Pestilence / Blood Strike (AoE spread)',    0, 0, 1, 0),
    (342, 34, 30, 'Obliterate',                                0, 0, 1, 0),
    (347, 34, 35, 'Blood Strike',                              0, 0, 1, 0),
    (343, 34, 40, 'Frost Strike (Killing Machine)',            0, 0, 1, 0),
    (348, 34, 45, 'Howling Blast (Rime proc)',                 0, 0, 1, 0),
    (349, 34, 50, 'Frost Strike (Runic Power dump)',           0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    `default_profile_id`  = VALUES(`default_profile_id`),
    `priority`            = VALUES(`priority`),
    `label`               = VALUES(`label`),
    `is_interrupt`        = VALUES(`is_interrupt`),
    `breaks_current_cast` = VALUES(`breaks_current_cast`),
    `enabled`             = VALUES(`enabled`),
    `condition_logic`     = VALUES(`condition_logic`);

-- -----------------------------------------------------------------------
-- ACTIONS
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    -- Fury Warrior
    (1900, 190, 0, 0,  5308, 0, 0, 0, 'enemy_primary'),
    (1910, 191, 0, 0, 23881, 0, 1, 0, 'enemy_primary'),
    (1920, 192, 0, 0,  1680, 0, 0, 0, 'enemy_primary'),
    (1930, 193, 0, 0,  1464, 0, 0, 0, 'enemy_primary'),
    (1940, 194, 0, 0,    78, 0, 0, 0, 'enemy_primary'),

    -- Marksmanship Hunter
    (2000, 200, 0, 0,  1978, 0, 0, 0, 'enemy_primary'),
    (2010, 201, 0, 0, 53209, 0, 1, 0, 'enemy_primary'),
    (2020, 202, 0, 0, 19434, 0, 0, 0, 'enemy_primary'),
    (2030, 203, 0, 0, 34120, 0, 1, 0, 'enemy_primary'),
    (2040, 204, 0, 0,  3044, 0, 0, 0, 'enemy_primary'),

    -- Survival Hunter
    (2100, 210, 0, 0,  1978, 0, 0, 0, 'enemy_primary'),
    (2110, 211, 0, 0, 53301, 0, 1, 0, 'enemy_primary'),
    (2120, 212, 0, 0,  3674, 0, 0, 0, 'enemy_primary'),
    (2130, 213, 0, 0, 19434, 0, 0, 0, 'enemy_primary'),
    (2131, 213, 1, 0,  2643, 0, 0, 0, 'enemy_primary'),
    (2140, 214, 0, 0, 34120, 0, 1, 0, 'enemy_primary'),
    (2141, 214, 1, 0,  3044, 0, 0, 0, 'enemy_primary'),

    -- Assassination Rogue
    (2200, 220, 0, 0, 32645, 0, 1, 0, 'enemy_primary'),
    (2210, 221, 0, 0,  5171, 0, 0, 0, 'enemy_primary'),
    (2220, 222, 0, 0,  1943, 0, 0, 0, 'enemy_primary'),
    (2230, 223, 0, 0,  1329, 0, 0, 0, 'enemy_primary'),
    (2240, 224, 0, 0,  2098, 0, 0, 0, 'enemy_primary'),

    -- Subtlety Rogue
    (2300, 230, 0, 0,  2098, 0, 0, 0, 'enemy_primary'),
    (2310, 231, 0, 0,  5171, 0, 0, 0, 'enemy_primary'),
    (2320, 232, 0, 0,  1943, 0, 0, 0, 'enemy_primary'),
    (2330, 233, 0, 0, 16511, 0, 1, 0, 'enemy_primary'),
    (2340, 234, 0, 0,    53, 0, 0, 0, 'enemy_primary'),

    -- Enhancement Shaman
    (2500, 250, 0, 0, 17364, 0, 0, 0, 'enemy_primary'),
    (2510, 251, 0, 0, 60103, 0, 1, 0, 'enemy_primary'),
    (2520, 252, 0, 0,  8050, 0, 0, 0, 'enemy_primary'),
    (2530, 253, 0, 0,  8042, 0, 0, 0, 'enemy_primary'),
    (2540, 254, 0, 0,   403, 0, 0, 0, 'enemy_primary'),

    -- Arcane Mage
    (2600, 260, 0, 0, 42940, 0, 0, 0, 'enemy_primary'), -- Blizzard
    (2601, 260, 1, 0, 42920, 0, 0, 0, 'enemy_primary'), -- Arcane Explosion fallback
    (2610, 261, 0, 0,  5143, 0, 0, 0, 'enemy_primary'), -- Arcane Missiles
    (2620, 262, 0, 0, 30451, 0, 0, 0, 'enemy_primary'), -- Arcane Blast
    (2621, 262, 1, 0,   116, 0, 0, 0, 'enemy_primary'), -- Frostbolt fallback
    (2630, 263, 0, 0, 44781, 0, 0, 0, 'enemy_primary'), -- Arcane Barrage
    (2640, 264, 0, 0,  2136, 0, 0, 0, 'enemy_primary'), -- Fire Blast

    -- Fire Mage
    (2700, 270, 0, 0, 42926, 0, 0, 0, 'enemy_primary'), -- Flamestrike
    (2701, 270, 1, 0, 42920, 0, 0, 0, 'enemy_primary'), -- Arcane Explosion fallback
    (2710, 271, 0, 0, 42940, 0, 0, 0, 'enemy_primary'), -- Blizzard
    (2711, 271, 1, 0, 42920, 0, 0, 0, 'enemy_primary'), -- Arcane Explosion fallback
    (2720, 272, 0, 0, 44457, 0, 0, 0, 'enemy_primary'), -- Living Bomb
    (2721, 272, 1, 0,   133, 0, 0, 0, 'enemy_primary'), -- Fireball fallback
    (2730, 273, 0, 0, 11366, 0, 0, 0, 'enemy_primary'), -- Pyroblast
    (2740, 274, 0, 0,  2948, 0, 0, 0, 'enemy_primary'), -- Scorch
    (2750, 275, 0, 0,   133, 0, 0, 0, 'enemy_primary'), -- Fireball
    (2760, 276, 0, 0,  2136, 0, 0, 0, 'enemy_primary'), -- Fire Blast

    -- Demonology Warlock
    (2800, 280, 0, 0,   172, 0, 0, 0, 'enemy_primary'),
    (2810, 281, 0, 0,   348, 0, 0, 0, 'enemy_primary'),
    (2820, 282, 0, 0,   980, 0, 0, 0, 'enemy_primary'),
    (2830, 283, 0, 0, 29722, 0, 1, 0, 'enemy_primary'),
    (2840, 284, 0, 0,   686, 0, 0, 0, 'enemy_primary'),

    -- Destruction Warlock
    (2900, 290, 0, 0,   348, 0, 0, 0, 'enemy_primary'),
    (2910, 291, 0, 0, 17962, 0, 0, 0, 'enemy_primary'),
    (2920, 292, 0, 0, 50796, 0, 1, 0, 'enemy_primary'),
    (2930, 293, 0, 0, 29722, 0, 1, 0, 'enemy_primary'),
    (2940, 294, 0, 0,   686, 0, 0, 0, 'enemy_primary'),

    -- Feral Druid DPS
    (3000, 300, 0, 0,  1079, 0, 0, 0, 'enemy_primary'),
    (3010, 301, 0, 0, 22568, 0, 0, 0, 'enemy_primary'),
    (3020, 302, 0, 0,  1822, 0, 0, 0, 'enemy_primary'),
    (3030, 303, 0, 0, 33876, 0, 1, 0, 'enemy_primary'),
    (3040, 304, 0, 0,  1082, 0, 0, 0, 'enemy_primary'),

    -- Frost Mage
    (3500, 350, 0, 0, 42940, 0, 0, 0, 'enemy_primary'), -- Blizzard
    (3501, 350, 1, 0, 42920, 0, 0, 0, 'enemy_primary'), -- Arcane Explosion fallback
    (3510, 351, 0, 0, 44572, 0, 0, 0, 'enemy_primary'), -- Deep Freeze
    (3511, 351, 1, 0, 30455, 0, 0, 0, 'enemy_primary'), -- Ice Lance fallback
    (3520, 352, 0, 0, 44614, 0, 0, 0, 'enemy_primary'), -- Frostfire Bolt
    (3530, 353, 0, 0,   116, 0, 0, 0, 'enemy_primary'), -- Frostbolt
    (3531, 353, 1, 0,   133, 0, 0, 0, 'enemy_primary'), -- Fireball fallback
    (3540, 354, 0, 0, 30455, 0, 0, 0, 'enemy_primary'), -- Ice Lance
    (3550, 355, 0, 0,  2136, 0, 0, 0, 'enemy_primary'), -- Fire Blast

    -- Frost Death Knight
    -- Use BestKnown for lower-level friendliness; if a level-80 spell is not
    -- available yet, the chain resolver can still fall back to the best learned rank.
    (3400, 340, 0, 0, 45477, 0, 0, 0, 'enemy_primary'), -- Icy Touch
    (3410, 341, 0, 0, 45462, 0, 0, 0, 'enemy_primary'), -- Plague Strike
    (3420, 342, 0, 0, 49020, 0, 0, 0, 'enemy_primary'), -- Obliterate
    (3430, 343, 0, 0, 49143, 0, 0, 0, 'enemy_primary'), -- Frost Strike
    (3440, 344, 0, 0, 49184, 0, 0, 0, 'enemy_primary'), -- Howling Blast
    (3450, 345, 0, 0, 49938, 0, 0, 0, 'enemy_primary'), -- Death and Decay
    (3451, 345, 1, 0, 49184, 0, 0, 0, 'enemy_primary'), -- Howling Blast fallback
    (3460, 346, 0, 0, 50842, 0, 0, 0, 'enemy_primary'), -- Pestilence
    (3461, 346, 1, 0, 49930, 0, 0, 0, 'enemy_primary'), -- Blood Strike fallback
    (3470, 347, 0, 0, 49930, 0, 0, 0, 'enemy_primary'), -- Blood Strike
    (3480, 348, 0, 0, 49184, 0, 0, 0, 'enemy_primary'), -- Howling Blast on Rime
    (3490, 349, 0, 0, 49143, 0, 0, 0, 'enemy_primary')  -- Frost Strike dump
ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `slot`          = VALUES(`slot`),
    `action_type`   = VALUES(`action_type`),
    `spell_base_id` = VALUES(`spell_base_id`),
    `item_id`       = VALUES(`item_id`),
    `rank_mode`     = VALUES(`rank_mode`),
    `rank_value`    = VALUES(`rank_value`),
    `target_key`    = VALUES(`target_key`);

-- -----------------------------------------------------------------------
-- CONDITIONS
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    -- Fury Warrior: Execute only when enemy hp <= 20%
    (1900, 190, 0, 'enemy', 'hp_pct', 3, 20.0, ''),

    -- Assassination Rogue combo-point gates
    (2200, 220, 0, 'self', 'combo_points', 5, 4.0, ''),
    (2210, 221, 0, 'self', 'combo_points', 5, 2.0, ''),
    (2220, 222, 0, 'self', 'combo_points', 5, 3.0, ''),
    (2240, 224, 0, 'self', 'combo_points', 5, 4.0, ''),

    -- Subtlety Rogue combo-point gates
    (2300, 230, 0, 'self', 'combo_points', 5, 4.0, ''),
    (2310, 231, 0, 'self', 'combo_points', 5, 2.0, ''),
    (2320, 232, 0, 'self', 'combo_points', 5, 3.0, ''),

    -- Feral Druid combo-point gates
    (3000, 300, 0, 'self', 'combo_points', 5, 4.0, ''),
    (3010, 301, 0, 'self', 'combo_points', 5, 4.0, ''),

    -- Arcane Mage: AoE branching and Missile Barrage consumption after building stacks.
    (2600, 260, 0, 'enemy', 'nearby_enemies', 5, 3.0, '10'),
    (2610, 261, 0, 'self',  'aura',          6, 44401.0, ''),
    (2611, 261, 1, 'self',  'aura_stacks',   5,     3.0, '36032'),

    -- Fire Mage: AoE branching, Living Bomb upkeep, Hot Streak, and Scorch upkeep.
    (2700, 270, 0, 'enemy', 'nearby_enemies',      5, 4.0, '10'),
    (2710, 271, 0, 'enemy', 'nearby_enemies',      5, 3.0, '10'),
    (2720, 272, 0, 'enemy', 'aura_remaining_secs', 3, 3.0, '55360'),
    (2730, 273, 0, 'self',  'aura',                6, 44448.0, ''),
    (2740, 274, 0, 'enemy', 'aura_remaining_secs', 3, 5.0, '22959'),

    -- Frost Mage: AoE branching and proc-driven Deep Freeze / Brain Freeze rules.
    (3500, 350, 0, 'enemy', 'nearby_enemies', 5, 3.0, '10'),
    (3510, 351, 0, 'self',  'aura',          6, 44545.0, ''),
    (3520, 352, 0, 'self',  'aura',          6, 44549.0, ''),

    -- Frost DK disease upkeep: apply if missing OR if the disease is about to fall off.
    (3400, 340, 0, 'enemy', 'aura',                7, 55095.0, ''),
    (3401, 340, 1, 'enemy', 'aura_remaining_secs', 3,     3.0, '55095'),
    (3410, 341, 0, 'enemy', 'aura',                7, 55078.0, ''),
    (3411, 341, 1, 'enemy', 'aura_remaining_secs', 3,     3.0, '55078'),

    -- Frost DK smart AoE branching.
    (3440, 344, 0, 'enemy', 'nearby_enemies', 5, 2.0, '10'),
    (3450, 345, 0, 'enemy', 'nearby_enemies', 5, 3.0, '10'),
    (3460, 346, 0, 'enemy', 'nearby_enemies', 5, 3.0, '10'),
    (3461, 346, 1, 'enemy', 'aura',           6, 55095.0, ''),
    (3462, 346, 2, 'enemy', 'aura',           6, 55078.0, ''),

    -- Frost DK single-target proc / RP handling.
    (3430, 343, 0, 'self',  'aura',          6, 51130.0, ''),
    (3431, 343, 1, 'self',  'runic_power',   5,    40.0, ''),
    (3480, 348, 0, 'self',  'aura',          6, 59057.0, ''),
    (3481, 348, 1, 'enemy', 'nearby_enemies',3,     1.0, '10'),
    (3490, 349, 0, 'self',  'runic_power',   5,    40.0, '')
ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `sequence`      = VALUES(`sequence`),
    `subject_key`   = VALUES(`subject_key`),
    `stat_key`      = VALUES(`stat_key`),
    `comparison`    = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value`  = VALUES(`string_value`);