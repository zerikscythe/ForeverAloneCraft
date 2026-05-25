-- rev_living_world_050_priest_pve_doctrine_reset (world DB)
--
-- Clean Priest PvE doctrine reset:
--   * replace the legacy Shadow starter profile with a class-specific PvE profile
--   * replace the old class-split Holy healer rows with a modernized PvE profile
--   * add a dedicated Discipline PvE healer doctrine instead of relying on
--     the old shared/default scaffolding
--   * refresh Holy / Shadow talent templates and add a Discipline template
--     sourced from current WotLK Classic guide material
--
-- Human/source notes:
--   Discipline PvE:
--     https://www.icy-veins.com/wotlk-classic/discipline-priest-healer-pve-guide
--     https://www.icy-veins.com/wotlk-classic/discipline-priest-healer-pve-rotation-cooldowns-abilities
--   Holy PvE:
--     https://www.icy-veins.com/wotlk-classic/holy-priest-healer-pve-guide
--     https://www.icy-veins.com/wotlk-classic/holy-priest-healer-pve-rotation-cooldowns-abilities
--   Shadow PvE:
--     https://www.icy-veins.com/wotlk-classic/shadow-priest-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/shadow-priest-dps-pve-rotation-cooldowns-abilities
--
-- Local DBC-backed spell references used here:
--   Power Word: Shield = 17
--   Renew              = 139
--   Flash Heal         = 2061
--   Greater Heal       = 2060
--   Prayer of Healing  = 596
--   Prayer of Mending  = 33076
--   Penance            = 47540
--   Pain Suppression   = 33206
--   Circle of Healing  = 34861
--   Guardian Spirit    = 47788
--   Shadowfiend        = 34433
--   Dispersion         = 47585
--   Vampiric Touch     = 34914
--   Shadow Word: Pain  = 589
--   Devouring Plague   = 2944
--   Mind Blast         = 8092
--   Mind Flay          = 15407

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
    ( 5, 'Shadow',     'DPS',  'Priest', 'PvE', '', 'Shadow Priest DPS',
      'Priority-based PvE Shadow Priest profile rebuilt from current WotLK Classic guide material.',
      1, 35, 65, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (24, 'Discipline', 'HEAL', 'Priest', 'PvE', '', 'Discipline Priest Healer',
      'Priority-based PvE Discipline Priest profile rebuilt from current WotLK Classic guide material.',
      1, 35, 60, 1, 2, 0, 3, 12, 1, 90, 170, 75, 245, 270, 130, 175),
    (31, 'Holy',       'HEAL', 'Priest', 'PvE', '', 'Holy Priest Healer',
      'Priority-based PvE Holy Priest profile rebuilt from current WotLK Classic guide material.',
      1, 40, 65, 1, 2, 0, 3, 12, 1, 90, 170, 75, 245, 270, 130, 175)
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
-- PRIEST TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    ( 9, 'Holy',       5, '', 'Holy Priest Healer',
        'PvE Holy Priest healer talent template refreshed against current WotLK Classic guide material.'),
    (10, 'Shadow',     5, '', 'Shadow Priest DPS',
        'PvE Shadow Priest talent template refreshed against current WotLK Classic guide material.'),
    (25, 'Discipline', 5, '', 'Discipline Priest Healer',
        'PvE Discipline Priest healer talent template based on current WotLK Classic guide material.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (9, 10, 25);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Holy Priest (18 / 53 / 0)
    ( 9, 202, 1898, 'Twin Disciplines', 5),
    ( 9, 211,  346, 'Improved Inner Fire', 3),
    ( 9, 212,  344, 'Improved Power Word: Fortitude', 2),
    ( 9, 220,  347, 'Meditation', 3),
    ( 9, 221,  348, 'Inner Focus', 1),
    ( 9, 222,  343, 'Improved Power Word: Shield', 1),
    ( 9,   0,  410, 'Healing Focus', 2),
    ( 9,   1,  406, 'Improved Renew', 3),
    ( 9,   2,  401, 'Holy Specialization', 5),
    ( 9,  12, 1181, 'Divine Fury', 5),
    ( 9,  23,  361, 'Inspiration', 3),
    ( 9,  30, 1635, 'Holy Reach', 2),
    ( 9,  40,  413, 'Healing Prayers', 2),
    ( 9,  41, 1561, 'Spirit of Redemption', 1),
    ( 9,  42,  402, 'Spiritual Guidance', 5),
    ( 9,  50, 1766, 'Surge of Light', 2),
    ( 9,  52,  404, 'Spiritual Healing', 5),
    ( 9,  60, 1768, 'Holy Concentration', 3),
    ( 9,  62, 1765, 'Blessed Resilience', 2),
    ( 9,  72, 1904, 'Serendipity', 3),
    ( 9,  80, 1902, 'Empowered Renew', 3),
    ( 9,  81, 1815, 'Circle of Healing', 1),
    ( 9,  91, 1905, 'Divine Providence', 5),
    ( 9, 101, 1911, 'Guardian Spirit', 1),

    -- Shadow Priest (14 / 0 / 57)
    (10, 202, 1898, 'Twin Disciplines', 5),
    (10, 211,  346, 'Improved Inner Fire', 3),
    (10, 212,  344, 'Improved Power Word: Fortitude', 2),
    (10, 220,  347, 'Meditation', 3),
    (10, 221,  348, 'Inner Focus', 1),
    (10,   0,  465, 'Spirit Tap', 3),
    (10,   1, 2027, 'Improved Spirit Tap', 2),
    (10,   2,  462, 'Darkness', 5),
    (10,  11,  482, 'Improved Shadow Word: Pain', 2),
    (10,  12,  463, 'Shadow Focus', 3),
    (10,  21,  481, 'Improved Mind Blast', 5),
    (10,  22,  501, 'Mind Flay', 1),
    (10,  31,  483, 'Veiled Shadows', 2),
    (10,  32,  881, 'Shadow Reach', 2),
    (10,  33,  461, 'Shadow Weaving', 3),
    (10,  41,  484, 'Vampiric Embrace', 1),
    (10,  43, 1777, 'Focused Mind', 2),
    (10,  50, 1781, 'Mind Melt', 2),
    (10,  52, 2267, 'Improved Devouring Plague', 3),
    (10,  61,  521, 'Shadowform', 1),
    (10,  62, 1778, 'Shadow Power', 5),
    (10,  70, 1906, 'Improved Shadowform', 2),
    (10,  72, 1816, 'Misery', 3),
    (10,  81, 1779, 'Vampiric Touch', 1),
    (10,  82, 1909, 'Pain and Suffering', 3),
    (10,  92, 1907, 'Twisted Faith', 5),
    (10, 101, 1910, 'Dispersion', 1),

    -- Discipline Priest (57 / 14 / 0)
    (25,   2, 1898, 'Twin Disciplines', 5),
    (25,  11,  346, 'Improved Inner Fire', 3),
    (25,  12,  344, 'Improved Power Word: Fortitude', 2),
    (25,  20,  347, 'Meditation', 3),
    (25,  21,  348, 'Inner Focus', 1),
    (25,  22,  343, 'Improved Power Word: Shield', 3),
    (25,  31,  341, 'Mental Agility', 3),
    (25,  41, 1201, 'Mental Strength', 5),
    (25,  42,  351, 'Soul Warding', 1),
    (25,  50, 1771, 'Focused Power', 2),
    (25,  52, 1772, 'Enlightenment', 3),
    (25,  60, 1858, 'Focused Will', 3),
    (25,  61,  322, 'Power Infusion', 1),
    (25,  62, 1773, 'Improved Flash Heal', 3),
    (25,  70, 2235, 'Renewed Hope', 2),
    (25,  71, 1896, 'Rapture', 3),
    (25,  72, 1894, 'Aspiration', 2),
    (25,  80, 1895, 'Divine Aegis', 3),
    (25,  81, 1774, 'Pain Suppression', 1),
    (25,  82, 1901, 'Grace', 2),
    (25,  91, 1202, 'Borrowed Time', 5),
    (25, 101, 1897, 'Penance', 1),
    (25, 200,  410, 'Healing Focus', 2),
    (25, 201,  406, 'Improved Renew', 3),
    (25, 202,  401, 'Holy Specialization', 5),
    (25, 220,  442, 'Desperate Prayer', 1),
    (25, 223,  361, 'Inspiration', 3);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE `c`
FROM `living_world_bot_combat_default_condition` `c`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `c`.`entry_id`
WHERE `e`.`default_profile_id` IN (5, 24, 31);

DELETE `a`
FROM `living_world_bot_combat_default_action` `a`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `a`.`entry_id`
WHERE `e`.`default_profile_id` IN (5, 24, 31);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `default_profile_id` IN (5, 24, 31);

-- -----------------------------------------------------------------------
-- DISCIPLINE PRIEST PVE PROFILE (24)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (560, 24,  0, 'Pain Suppression emergency',    0, 1, 1, 0),
    (561, 24, 10, 'Power Word: Shield',            0, 1, 1, 0),
    (562, 24, 20, 'Penance emergency',             0, 1, 1, 0),
    (563, 24, 30, 'Prayer of Mending',             0, 0, 1, 0),
    (564, 24, 40, 'Prayer of Healing AoE',         0, 0, 1, 0),
    (565, 24, 50, 'Flash Heal urgent',             0, 0, 1, 0),
    (566, 24, 60, 'Greater Heal moderate',         0, 0, 1, 0),
    (567, 24, 70, 'Renew upkeep',                  0, 0, 1, 0),
    (568, 24, 80, 'Shadowfiend (mana)',            0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5600, 560, 0, 0, 33206, 0, 0, 0, 'lowest_hp_party'),
    (5610, 561, 0, 0,    17, 0, 0, 0, 'lowest_hp_party'),
    (5620, 562, 0, 0, 47540, 0, 0, 0, 'lowest_hp_party'),
    (5630, 563, 0, 0, 33076, 0, 0, 0, 'lowest_hp_party'),
    (5640, 564, 0, 0,   596, 0, 0, 0, 'lowest_hp_party'),
    (5650, 565, 0, 0,  2061, 0, 0, 0, 'lowest_hp_party'),
    (5660, 566, 0, 0,  2060, 0, 0, 0, 'lowest_hp_party'),
    (5670, 567, 0, 0,   139, 0, 0, 0, 'lowest_hp_party'),
    (5680, 568, 0, 0, 34433, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5600, 560, 0, 'lowest_hp_party', 'hp_pct',                 3, 20.0, ''),
    (5610, 561, 0, 'lowest_hp_party', 'hp_pct',                 3, 55.0, ''),
    (5611, 561, 1, 'lowest_hp_party', 'aura',                   7,  0.0, '17'),
    (5620, 562, 0, 'lowest_hp_party', 'hp_pct',                 3, 45.0, ''),
    (5630, 563, 0, 'lowest_hp_party', 'hp_pct',                 3, 80.0, ''),
    (5631, 563, 1, 'lowest_hp_party', 'aura',                   7,  0.0, '33076'),
    (5640, 564, 0, 'self',            'party_members_below_hp_pct', 5, 3.0, '70'),
    (5650, 565, 0, 'lowest_hp_party', 'hp_pct',                 3, 40.0, ''),
    (5660, 566, 0, 'lowest_hp_party', 'hp_pct',                 3, 70.0, ''),
    (5670, 567, 0, 'lowest_hp_party', 'hp_pct',                 3, 85.0, ''),
    (5671, 567, 1, 'lowest_hp_party', 'aura',                   7,  0.0, '139'),
    (5680, 568, 0, 'self',            'power_pct',              3, 25.0, '');

-- -----------------------------------------------------------------------
-- HOLY PRIEST PVE PROFILE (31)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (570, 31,  0, 'Guardian Spirit emergency',    0, 1, 1, 0),
    (571, 31, 10, 'Prayer of Mending',            0, 0, 1, 0),
    (572, 31, 20, 'Circle of Healing AoE',        0, 0, 1, 0),
    (573, 31, 30, 'Prayer of Healing AoE',        0, 0, 1, 0),
    (574, 31, 40, 'Flash Heal urgent',            0, 0, 1, 0),
    (575, 31, 50, 'Greater Heal moderate',        0, 0, 1, 0),
    (576, 31, 60, 'Renew upkeep',                 0, 0, 1, 0),
    (577, 31, 70, 'Power Word: Shield emergency', 0, 1, 1, 0),
    (578, 31, 80, 'Shadowfiend (mana)',           0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5700, 570, 0, 0, 47788, 0, 0, 0, 'lowest_hp_party'),
    (5710, 571, 0, 0, 33076, 0, 0, 0, 'lowest_hp_party'),
    (5720, 572, 0, 0, 34861, 0, 0, 0, 'lowest_hp_party'),
    (5730, 573, 0, 0,   596, 0, 0, 0, 'lowest_hp_party'),
    (5740, 574, 0, 0,  2061, 0, 0, 0, 'lowest_hp_party'),
    (5750, 575, 0, 0,  2060, 0, 0, 0, 'lowest_hp_party'),
    (5760, 576, 0, 0,   139, 0, 0, 0, 'lowest_hp_party'),
    (5770, 577, 0, 0,    17, 0, 0, 0, 'lowest_hp_party'),
    (5780, 578, 0, 0, 34433, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5700, 570, 0, 'lowest_hp_party', 'hp_pct',                 3, 20.0, ''),
    (5710, 571, 0, 'lowest_hp_party', 'hp_pct',                 3, 80.0, ''),
    (5711, 571, 1, 'lowest_hp_party', 'aura',                   7,  0.0, '33076'),
    (5720, 572, 0, 'self',            'party_members_below_hp_pct', 5, 3.0, '80'),
    (5730, 573, 0, 'self',            'party_members_below_hp_pct', 5, 3.0, '65'),
    (5740, 574, 0, 'lowest_hp_party', 'hp_pct',                 3, 40.0, ''),
    (5750, 575, 0, 'lowest_hp_party', 'hp_pct',                 3, 68.0, ''),
    (5760, 576, 0, 'lowest_hp_party', 'hp_pct',                 3, 85.0, ''),
    (5761, 576, 1, 'lowest_hp_party', 'aura',                   7,  0.0, '139'),
    (5770, 577, 0, 'lowest_hp_party', 'hp_pct',                 3, 50.0, ''),
    (5771, 577, 1, 'lowest_hp_party', 'aura',                   7,  0.0, '17'),
    (5780, 578, 0, 'self',            'power_pct',              3, 25.0, '');

-- -----------------------------------------------------------------------
-- SHADOW PRIEST PVE PROFILE (5)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (580,  5,  0, 'Dispersion (mana / panic)',    0, 1, 1, 1),
    (581,  5, 10, 'Shadowfiend (mana)',           0, 0, 1, 0),
    (582,  5, 20, 'Vampiric Touch upkeep',        0, 0, 1, 1),
    (583,  5, 30, 'Devouring Plague upkeep',      0, 0, 1, 1),
    (584,  5, 40, 'Shadow Word: Pain',            0, 0, 1, 0),
    (585,  5, 50, 'Mind Blast',                   0, 0, 1, 0),
    (586,  5, 60, 'Mind Flay',                    0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5800, 580, 0, 0, 47585, 0, 0, 0, 'self'),
    (5810, 581, 0, 0, 34433, 0, 0, 0, 'enemy_primary'),
    (5820, 582, 0, 0, 34914, 0, 0, 0, 'enemy_primary'),
    (5830, 583, 0, 0,  2944, 0, 0, 0, 'enemy_primary'),
    (5840, 584, 0, 0,   589, 0, 0, 0, 'enemy_primary'),
    (5850, 585, 0, 0,  8092, 0, 0, 0, 'enemy_primary'),
    (5860, 586, 0, 0, 15407, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5800, 580, 0, 'self',  'power_pct',           3, 20.0, ''),
    (5801, 580, 1, 'self',  'hp_pct',              3, 25.0, ''),
    (5810, 581, 0, 'self',  'power_pct',           3, 35.0, ''),
    (5820, 582, 0, 'enemy', 'aura',                7,  0.0, '34914'),
    (5821, 582, 1, 'enemy', 'aura_remaining_secs', 3,  3.0, '34914'),
    (5830, 583, 0, 'enemy', 'aura',                7,  0.0, '2944'),
    (5831, 583, 1, 'enemy', 'aura_remaining_secs', 3,  3.0, '2944'),
    (5840, 584, 0, 'enemy', 'aura',                7,  0.0, '589');
