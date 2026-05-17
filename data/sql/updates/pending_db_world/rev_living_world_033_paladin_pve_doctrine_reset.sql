-- rev_living_world_033_paladin_pve_doctrine_reset (world DB)
--
-- Clean Paladin PvE doctrine reset:
--   * replace the old starter Retribution profile with a class-specific PvE profile
--   * replace the old Holy profile with explicit owner/self healing logic
--   * split the shared Warrior/Paladin Protection tank profile so Paladin gets
--     its own class-specific PvE profile and Warrior keeps a clean warrior row
--   * rebuild Paladin talent templates 4/5/6 from the currently curated Icy Veins
--     Wrath Classic PvE guidance and the locally extracted talent data
--
-- Human/source notes:
--   Retribution PvE:
--     https://www.icy-veins.com/wotlk-classic/retribution-paladin-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/retribution-paladin-dps-pve-rotation-cooldowns-abilities
--     user-provided calculator URL decoded locally to 11/5/55:
--       Holy 11 / Protection 5 / Retribution 55
--   Protection PvE:
--     https://www.icy-veins.com/wotlk-classic/protection-paladin-tank-pve-spec-builds-talents-glyphs
--     https://www.icy-veins.com/wotlk-classic/protection-paladin-tank-pve-rotation-cooldowns-abilities
--   Holy PvE:
--     https://www.icy-veins.com/wotlk-classic/holy-paladin-healer-pve-spec-builds-talents-glyphs
--     https://www.icy-veins.com/wotlk-classic/holy-paladin-healer-pve-rotation-cooldowns-abilities
--
-- Local DBC-backed spell references used here:
--   Avenging Wrath        = 31884
--   Beacon of Light       = 53563
--   Consecration          = 20116
--   Crusader Strike       = 35395
--   Divine Plea           = 54428
--   Divine Storm          = 53385
--   Flash of Light        = 19750
--   Hammer of the Righteous = 53595
--   Hammer of Wrath       = 24275
--   Hand of Reckoning     = 62124
--   Holy Light            = 635
--   Holy Shield           = 20925
--   Holy Shock            = 20473
--   Holy Wrath            = 2812
--   Judgement of Wisdom   = 20186
--   Righteous Fury        = 25780
--   Sacred Shield         = 53601
--   Seal of Command       = 20375
--   Seal of Vengeance     = 31801
--   Seal of Wisdom        = 20166
--   Shield of Righteousness = 53600 / 61411

-- -----------------------------------------------------------------------
-- PROFILE ROWS
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_profile` (
    `default_profile_id`, `spec_key`, `role_key`, `class_key`, `context_key`,
    `variant_key`, `display_name`, `description`,
    `conservation_mode`, `resource_low_water`, `resource_high_water`,
    `enable_down_rank`, `down_rank_floor`,
    `default_aoe_mode`, `default_aoe_min_targets`, `default_aoe_scan_radius`
) VALUES
    ( 2, 'Retribution', 'DPS',  'Paladin', 'PvE', '', 'Paladin Retribution DPS',
      'Priority-based PvE Retribution Paladin profile rebuilt from current Icy Veins guidance.',
      1, 30, 55, 0, 0, 0, 2, 10),
    (11, 'Holy',        'HEAL', 'Paladin', 'PvE', '', 'Paladin Holy',
      'Owner/self aware PvE Holy Paladin healer profile rebuilt from current Icy Veins guidance.',
      2, 45, 70, 1, 2, 0, 2, 10),
    (13, 'Protection',  'TANK', 'Warrior', 'PvE', '', 'Warrior Protection Tank',
      'Warrior-only PvE Protection tank profile after splitting out Paladin logic.',
      0,  0,100, 0, 0, 0, 2, 10),
    (35, 'Protection',  'TANK', 'Paladin', 'PvE', '', 'Paladin Protection Tank',
      'Priority-based PvE Protection Paladin tank profile rebuilt from current Icy Veins guidance.',
      1, 25, 45, 0, 0, 0, 2, 10)
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
    `default_aoe_scan_radius` = VALUES(`default_aoe_scan_radius`);

-- -----------------------------------------------------------------------
-- PALADIN TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    (4, 'Holy',        2, '', 'Holy Paladin Healer',
        '52/19/0 PvE Holy Paladin talent template based on current Icy Veins guidance.'),
    (5, 'Protection',  2, '', 'Protection Paladin Tank',
        '0/51/20 PvE Protection Paladin talent template based on current Icy Veins guidance.'),
    (6, 'Retribution', 2, '', 'Retribution Paladin DPS',
        '11/5/55 PvE Retribution Paladin talent template based on current Icy Veins guidance.')
ON DUPLICATE KEY UPDATE
    `spec_key`    = VALUES(`spec_key`),
    `class_id`    = VALUES(`class_id`),
    `variant_key` = VALUES(`variant_key`),
    `display_name`= VALUES(`display_name`),
    `description` = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (4, 5, 6);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Holy Paladin (52 / 19 / 0)
    (4,   0, 1432, 'Spiritual Focus', 5),
    (4,  10, 1444, 'Healing Light', 3),
    (4,  20, 1449, 'Divine Intellect', 5),
    (4,  30, 1435, 'Aura Mastery', 1),
    (4,  40, 1461, 'Illumination', 5),
    (4,  50, 1443, 'Improved Lay on Hands', 2),
    (4,  60, 1446, 'Improved Blessing of Wisdom', 2),
    (4,  70, 1433, 'Divine Favor', 1),
    (4,  80, 1465, 'Sanctified Light', 3),
    (4,  90, 1627, 'Holy Power', 5),
    (4, 100, 1745, 'Light''s Grace', 3),
    (4, 110, 1502, 'Holy Shock', 1),
    (4, 120, 1746, 'Holy Guidance', 5),
    (4, 130, 1747, 'Divine Illumination', 1),
    (4, 140, 2199, 'Judgements of the Pure', 5),
    (4, 150, 2193, 'Infusion of Light', 2),
    (4, 160, 2191, 'Enlightened Judgements', 2),
    (4, 170, 2192, 'Beacon of Light', 1),
    (4, 200, 1442, 'Divinity', 5),
    (4, 210, 1748, 'Stoicism', 3),
    (4, 220, 1425, 'Guardian''s Favor', 2),
    (4, 230, 2280, 'Divine Sacrifice', 1),
    (4, 240, 1423, 'Toughness', 4),
    (4, 250, 2281, 'Divine Guardian', 2),
    (4, 260, 1422, 'Improved Devotion Aura', 2),

    -- Protection Paladin (0 / 51 / 20)
    (5,   0, 2185, 'Divine Strength', 5),
    (5,  10, 1629, 'Anticipation', 5),
    (5,  20, 2280, 'Divine Sacrifice', 1),
    (5,  30, 1501, 'Improved Righteous Fury', 3),
    (5,  40, 1423, 'Toughness', 5),
    (5,  50, 2281, 'Divine Guardian', 2),
    (5,  60, 1431, 'Blessing of Sanctuary', 1),
    (5,  70, 1426, 'Reckoning', 3),
    (5,  80, 1750, 'Sacred Duty', 2),
    (5,  90, 1429, 'One-Handed Weapon Specialization', 3),
    (5, 100, 2282, 'Spiritual Attunement', 1),
    (5, 110, 1430, 'Holy Shield', 1),
    (5, 120, 1751, 'Ardent Defender', 3),
    (5, 130, 1421, 'Redoubt', 3),
    (5, 140, 1753, 'Combat Expertise', 3),
    (5, 150, 2195, 'Touched by the Light', 3),
    (5, 160, 1754, 'Avenger''s Shield', 1),
    (5, 170, 2194, 'Guarded by the Light', 2),
    (5, 180, 2204, 'Shield of the Templar', 3),
    (5, 190, 2196, 'Hammer of the Righteous', 1),
    (5, 220, 1403, 'Deflection', 5),
    (5, 230, 1407, 'Benediction', 2),
    (5, 240, 1631, 'Improved Judgements', 1),
    (5, 250, 1401, 'Improved Blessing of Might', 2),
    (5, 260, 1633, 'Vindication', 2),
    (5, 270, 1481, 'Seal of Command', 1),
    (5, 280, 1634, 'Pursuit of Justice', 2),
    (5, 290, 1761, 'Sanctity of Battle', 2),
    (5, 300, 1755, 'Crusade', 3),

    -- Retribution Paladin (11 / 5 / 55)
    (6,   0, 1407, 'Benediction', 5),
    (6,  10, 1631, 'Improved Judgements', 2),
    (6,  20, 1464, 'Heart of the Crusader', 3),
    (6,  30, 1401, 'Improved Blessing of Might', 2),
    (6,  40, 1411, 'Conviction', 5),
    (6,  50, 1481, 'Seal of Command', 1),
    (6,  60, 1634, 'Pursuit of Justice', 2),
    (6,  70, 1761, 'Sanctity of Battle', 3),
    (6,  80, 1755, 'Crusade', 3),
    (6,  90, 1410, 'Two-Handed Weapon Specialization', 3),
    (6, 100, 1756, 'Sanctified Retribution', 1),
    (6, 110, 1402, 'Vengeance', 3),
    (6, 120, 2176, 'The Art of War', 2),
    (6, 130, 1441, 'Repentance', 1),
    (6, 140, 1758, 'Judgements of the Wise', 3),
    (6, 150, 1759, 'Fanaticism', 3),
    (6, 160, 2147, 'Sanctified Wrath', 2),
    (6, 170, 2148, 'Swift Retribution', 3),
    (6, 180, 1823, 'Crusader Strike', 1),
    (6, 190, 2179, 'Sheath of Light', 3),
    (6, 200, 2149, 'Righteous Vengeance', 3),
    (6, 210, 2150, 'Divine Storm', 1),
    (6, 220, 1463, 'Seals of the Pure', 5),
    (6, 230, 1444, 'Healing Light', 3),
    (6, 240, 1628, 'Unyielding Faith', 2),
    (6, 250, 1435, 'Aura Mastery', 1),
    (6, 260, 2185, 'Divine Strength', 5);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (
    6, 7, 8, 9, 10,
    60, 61, 62, 63, 64,
    83, 84, 85, 86, 87, 88, 89, 90,
    360, 361, 362, 363, 364, 365, 366, 367, 368, 369,
    370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383,
    390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401
);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (
    6, 7, 8, 9, 10,
    60, 61, 62, 63, 64,
    83, 84, 85, 86, 87, 88, 89, 90,
    360, 361, 362, 363, 364, 365, 366, 367, 368, 369,
    370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383,
    390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401
);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (
    6, 7, 8, 9, 10,
    60, 61, 62, 63, 64,
    83, 84, 85, 86, 87, 88, 89, 90,
    360, 361, 362, 363, 364, 365, 366, 367, 368, 369,
    370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383,
    390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401
);

-- -----------------------------------------------------------------------
-- RETRIBUTION PALADIN PVE PROFILE (2)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (360, 2,  0, 'Seal of Command (2+ targets)',  0, 0, 1, 0),
    (361, 2,  1, 'Seal of Vengeance (single target)', 0, 0, 1, 0),
    (362, 2,  2, 'Divine Plea',                  0, 0, 1, 0),
    (363, 2,  4, 'Avenging Wrath',               0, 0, 1, 0),
    (364, 2, 10, 'Hammer of Wrath',              0, 0, 1, 0),
    (365, 2, 20, 'Crusader Strike',              0, 0, 1, 0),
    (366, 2, 30, 'Judgement of Wisdom',          0, 0, 1, 0),
    (367, 2, 40, 'Divine Storm',                 0, 0, 1, 0),
    (368, 2, 50, 'Consecration',                 0, 0, 1, 0),
    (369, 2, 60, 'Holy Wrath (Undead / Demon)',  0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (3600, 360, 0, 0, 20375, 0, 0, 0, 'self'),
    (3610, 361, 0, 0, 31801, 0, 0, 0, 'self'),
    (3620, 362, 0, 0, 54428, 0, 0, 0, 'self'),
    (3630, 363, 0, 0, 31884, 0, 0, 0, 'self'),
    (3640, 364, 0, 0, 24275, 0, 0, 0, 'enemy_primary'),
    (3650, 365, 0, 0, 35395, 0, 0, 0, 'enemy_primary'),
    (3660, 366, 0, 0, 20186, 0, 0, 0, 'enemy_primary'),
    (3670, 367, 0, 0, 53385, 0, 0, 0, 'enemy_primary'),
    (3680, 368, 0, 0, 20116, 0, 0, 0, 'self'),
    (3690, 369, 0, 0, 2812,  0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3600, 360, 0, 'enemy', 'nearby_enemies', 5, 2.0, '10'),
    (3601, 360, 1, 'self',  'aura',           7, 0.0, '20375'),

    (3610, 361, 0, 'enemy', 'nearby_enemies', 2, 2.0, '10'),
    (3611, 361, 1, 'self',  'aura',           7, 0.0, '31801'),

    (3620, 362, 0, 'self',  'mana_pct',       3, 70.0, ''),
    (3621, 362, 1, 'self',  'aura',           7, 0.0, '54428'),

    (3640, 364, 0, 'enemy', 'hp_pct',         3, 20.0, ''),

    (3690, 369, 0, 'enemy', 'creature_type',  6, 0.0, 'undead|demon');

-- -----------------------------------------------------------------------
-- HOLY PALADIN PVE PROFILE (11)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (370, 11,  0, 'Beacon of Light (owner)',                 0, 0, 1, 0),
    (371, 11,  1, 'Beacon of Light (self fallback)',         0, 0, 1, 0),
    (372, 11,  2, 'Sacred Shield (owner)',                   0, 0, 1, 0),
    (373, 11,  3, 'Sacred Shield (self fallback)',           0, 0, 1, 0),
    (374, 11,  4, 'Seal of Wisdom',                          0, 0, 1, 0),
    (375, 11, 10, 'Holy Shock (self emergency)',             0, 0, 1, 0),
    (376, 11, 11, 'Holy Shock (owner emergency)',            0, 0, 1, 0),
    (377, 11, 12, 'Holy Shock (self while moving)',          0, 0, 1, 0),
    (378, 11, 13, 'Holy Shock (owner while moving)',         0, 0, 1, 0),
    (379, 11, 20, 'Flash of Light (self moderate)',          0, 0, 1, 0),
    (380, 11, 21, 'Flash of Light (owner moderate)',         0, 0, 1, 0),
    (381, 11, 30, 'Holy Light (self stable cast)',           0, 0, 1, 0),
    (382, 11, 31, 'Holy Light (owner stable cast)',          0, 0, 1, 0),
    (383, 11, 50, 'Judgement of Wisdom',                     0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (3700, 370, 0, 0, 53563, 0, 0, 0, 'owner'),
    (3710, 371, 0, 0, 53563, 0, 0, 0, 'self'),
    (3720, 372, 0, 0, 53601, 0, 0, 0, 'owner'),
    (3730, 373, 0, 0, 53601, 0, 0, 0, 'self'),
    (3740, 374, 0, 0, 20166, 0, 0, 0, 'self'),
    (3750, 375, 0, 0, 20473, 0, 0, 0, 'self'),
    (3760, 376, 0, 0, 20473, 0, 0, 0, 'owner'),
    (3770, 377, 0, 0, 20473, 0, 0, 0, 'self'),
    (3780, 378, 0, 0, 20473, 0, 0, 0, 'owner'),
    (3790, 379, 0, 0, 19750, 0, 0, 0, 'self'),
    (3800, 380, 0, 0, 19750, 0, 0, 0, 'owner'),
    (3810, 381, 0, 0,   635, 0, 0, 0, 'self'),
    (3820, 382, 0, 0,   635, 0, 0, 0, 'owner'),
    (3830, 383, 0, 0, 20186, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3700, 370, 0, 'owner', 'exists', 8, 0.0, ''),
    (3701, 370, 1, 'owner', 'aura',   7, 0.0, '53563'),

    (3710, 371, 0, 'owner', 'exists', 0, 0.0, ''),
    (3711, 371, 1, 'self',  'aura',   7, 0.0, '53563'),

    (3720, 372, 0, 'owner', 'exists', 8, 0.0, ''),
    (3721, 372, 1, 'owner', 'aura',   7, 0.0, '53601'),

    (3730, 373, 0, 'owner', 'exists', 0, 0.0, ''),
    (3731, 373, 1, 'self',  'aura',   7, 0.0, '53601'),

    (3740, 374, 0, 'self',  'aura',   7, 0.0, '20166'),

    (3750, 375, 0, 'self',  'hp_pct', 3, 35.0, ''),

    (3760, 376, 0, 'owner', 'exists', 8, 0.0, ''),
    (3761, 376, 1, 'owner', 'hp_pct', 3, 35.0, ''),

    (3770, 377, 0, 'self',  'is_moving', 5, 1.0, ''),
    (3771, 377, 1, 'self',  'hp_pct',    3, 80.0, ''),

    (3780, 378, 0, 'owner', 'exists',    8, 0.0, ''),
    (3781, 378, 1, 'self',  'is_moving', 5, 1.0, ''),
    (3782, 378, 2, 'owner', 'hp_pct',    3, 80.0, ''),

    (3790, 379, 0, 'self',  'hp_pct', 3, 55.0, ''),

    (3800, 380, 0, 'owner', 'exists', 8, 0.0, ''),
    (3801, 380, 1, 'owner', 'hp_pct', 3, 55.0, ''),

    (3810, 381, 0, 'self',  'hp_pct', 3, 80.0, ''),

    (3820, 382, 0, 'owner', 'exists', 8, 0.0, ''),
    (3821, 382, 1, 'owner', 'hp_pct', 3, 80.0, '');

-- -----------------------------------------------------------------------
-- WARRIOR PROTECTION PVE PROFILE (13 cleanup)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (60, 13,  0, 'Shield Slam',   0, 0, 1, 0),
    (61, 13, 10, 'Revenge',       0, 0, 1, 0),
    (62, 13, 20, 'Devastate',     0, 0, 1, 0),
    (63, 13, 30, 'Thunder Clap',  0, 0, 1, 0),
    (64, 13, 40, 'Heroic Strike', 0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (600, 60, 0, 0, 23922, 0, 0, 0, 'enemy_primary'),
    (610, 61, 0, 0,  6572, 0, 0, 0, 'enemy_primary'),
    (620, 62, 0, 0, 20243, 0, 0, 0, 'enemy_primary'),
    (630, 63, 0, 0,  6343, 0, 0, 0, 'enemy_primary'),
    (640, 64, 0, 0,    78, 0, 0, 0, 'enemy_primary');

-- -----------------------------------------------------------------------
-- PROTECTION PALADIN PVE PROFILE (35)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (390, 35,  0, 'Righteous Fury',                        0, 0, 1, 0),
    (391, 35,  1, 'Seal of Command (3+ targets)',          0, 0, 1, 0),
    (392, 35,  2, 'Seal of Vengeance (single target)',     0, 0, 1, 0),
    (393, 35,  4, 'Avenging Wrath',                        0, 0, 1, 0),
    (394, 35,  6, 'Holy Shield',                           0, 0, 1, 0),
    (395, 35, 10, 'Avenger''s Shield opener',              0, 0, 1, 0),
    (396, 35, 15, 'Hand of Reckoning (regain threat)',     0, 0, 1, 0),
    (397, 35, 20, 'Hammer of the Righteous',               0, 0, 1, 0),
    (398, 35, 30, 'Judgement of Wisdom',                   0, 0, 1, 0),
    (399, 35, 40, 'Shield of Righteousness',               0, 0, 1, 0),
    (400, 35, 50, 'Consecration (2+ targets)',             0, 0, 1, 0),
    (401, 35, 60, 'Holy Wrath (Undead / Demon)',           0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (3900, 390, 0, 0, 25780, 0, 0, 0, 'self'),
    (3910, 391, 0, 0, 20375, 0, 0, 0, 'self'),
    (3920, 392, 0, 0, 31801, 0, 0, 0, 'self'),
    (3930, 393, 0, 0, 31884, 0, 0, 0, 'self'),
    (3940, 394, 0, 0, 20925, 0, 0, 0, 'self'),
    (3950, 395, 0, 0, 31935, 0, 0, 0, 'enemy_primary'),
    (3960, 396, 0, 0, 62124, 0, 0, 0, 'enemy_primary'),
    (3970, 397, 0, 0, 53595, 0, 0, 0, 'enemy_primary'),
    (3980, 398, 0, 0, 20186, 0, 0, 0, 'enemy_primary'),
    (3990, 399, 0, 0, 53600, 0, 0, 0, 'enemy_primary'),
    (4000, 400, 0, 0, 20116, 0, 0, 0, 'self'),
    (4010, 401, 0, 0, 2812,  0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3900, 390, 0, 'self',  'aura',           7, 0.0, '25780'),

    (3910, 391, 0, 'enemy', 'nearby_enemies', 5, 3.0, '10'),
    (3911, 391, 1, 'self',  'aura',           7, 0.0, '20375'),

    (3920, 392, 0, 'enemy', 'nearby_enemies', 2, 3.0, '10'),
    (3921, 392, 1, 'self',  'aura',           7, 0.0, '31801'),

    (3950, 395, 0, 'enemy', 'distance',       5, 10.0, ''),

    (3960, 396, 0, 'self',  'is_aggro_holder', 0, 0.0, ''),

    (4000, 400, 0, 'enemy', 'nearby_enemies', 5, 2.0, '10'),

    (4010, 401, 0, 'enemy', 'creature_type',  6, 0.0, 'undead|demon');
