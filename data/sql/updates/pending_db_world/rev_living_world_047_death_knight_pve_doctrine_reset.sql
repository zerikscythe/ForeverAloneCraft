-- rev_living_world_047_death_knight_pve_doctrine_reset (world DB)
--
-- Clean Death Knight PvE doctrine reset:
--   * replace the old starter Unholy profile with a class-specific PvE profile
--   * replace the old Blood tank starter rows with an explicit PvE Blood tank profile
--   * rebuild the Frost placeholder profile into a class-specific PvE doctrine
--   * keep the strong local rune / runic-power condition model in play instead of
--     treating Death Knight like a mana-bar melee class
--   * refresh Blood / Unholy talent template metadata and add a dedicated Frost
--     talent template sourced from the current Icy Veins WotLK Classic build
--
-- Human/source notes:
--   Blood PvE:
--     https://www.icy-veins.com/wotlk-classic/blood-death-knight-tank-pve-guide
--     https://www.icy-veins.com/wotlk-classic/blood-death-knight-tank-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/blood-death-knight-tank-pve-spec-builds-talents-glyphs
--   Frost PvE:
--     https://www.icy-veins.com/wotlk-classic/frost-death-knight-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/frost-death-knight-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/frost-death-knight-dps-pve-spec-builds-talents-glyphs
--   Unholy PvE:
--     https://www.icy-veins.com/wotlk-classic/unholy-death-knight-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/unholy-death-knight-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/unholy-death-knight-dps-pve-spec-builds-talents-glyphs
--
-- Local DBC-backed spell references used here:
--   Mind Freeze        = 47528
--   Icy Touch          = 45477
--   Plague Strike      = 45462
--   Pestilence         = 50842
--   Death and Decay    = 49938
--   Death Strike       = 49998
--   Rune Strike        = 56815
--   Heart Strike       = 55050
--   Rune Tap           = 48982
--   Vampiric Blood     = 55233
--   Scourge Strike     = 55090
--   Death Coil         = 47541
--   Blood Strike       = 49930
--   Summon Gargoyle    = 49206
--   Obliterate         = 49020
--   Frost Strike       = 49143
--   Howling Blast      = 49184
--
-- Design note:
--   Blood / Frost / Unholy all lean on disease upkeep, nearby-enemy branching,
--   and runic-power dumps. The runtime already understands those condition
--   families well enough that we can keep this family mostly data-driven on the
--   first pass.

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
    ( 6, 'Unholy', 'DPS',  'Death Knight', 'PvE', '', 'Death Knight Unholy DPS',
      'Priority-based PvE Unholy Death Knight profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (14, 'Blood',  'TANK', 'Death Knight', 'PvE', '', 'Death Knight Blood Tank',
      'Priority-based PvE Blood Death Knight tank profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 2, 10, 1, 115, 210, 95, 235, 260, 145, 190),
    (34, 'Frost',  'DPS',  'Death Knight', 'PvE', '', 'Death Knight Frost DPS',
      'Priority-based PvE Frost Death Knight profile rebuilt from current WotLK Classic guide material.',
      0, 0, 100, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175)
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
-- DEATH KNIGHT TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    (11, 'Blood',  6, '', 'Blood Death Knight Tank',
        'PvE Blood Death Knight tank template refreshed against current WotLK Classic guide material.'),
    (12, 'Unholy', 6, '', 'Unholy Death Knight DPS',
        'PvE Unholy Death Knight DPS template refreshed against current WotLK Classic guide material.'),
    (23, 'Frost',  6, '', 'Frost Death Knight DPS',
        'PvE Frost Death Knight DPS template based on current WotLK Classic guide material.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (11, 12, 23);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Blood Death Knight Tank
    (11,   0, 1939, 'Butchery', 2),
    (11,   1, 1945, 'Subversion', 3),
    (11,   2, 2017, 'Blade Barrier', 5),
    (11,  10, 1938, 'Bladed Armor', 5),
    (11,  11, 1948, 'Scent of Blood', 3),
    (11,  12, 2217, 'Two-Handed Weapon Specialization', 2),
    (11,  20, 1941, 'Rune Tap', 1),
    (11,  21, 1943, 'Dark Conviction', 5),
    (11,  22, 2086, 'Death Rune Mastery', 3),
    (11,  30, 1942, 'Improved Rune Tap', 3),
    (11,  33, 1953, 'Vendetta', 3),
    (11,  40, 2015, 'Bloody Strikes', 3),
    (11,  42, 1950, 'Veteran of the Third War', 3),
    (11,  51, 1944, 'Bloody Vengeance', 3),
    (11,  52, 2105, 'Abomination''s Might', 2),
    (11,  61, 1954, 'Hysteria', 1),
    (11,  62, 1936, 'Improved Blood Presence', 2),
    (11,  70, 2259, 'Improved Death Strike', 2),
    (11,  71, 1955, 'Sudden Doom', 3),
    (11,  72, 2019, 'Vampiric Blood', 1),
    (11,  80, 1959, 'Will of the Necropolis', 3),
    (11,  81, 1957, 'Heart Strike', 1),
    (11,  82, 1958, 'Might of Mograine', 3),
    (11,  91, 2034, 'Blood Gorged', 5),
    (11, 101, 1961, 'Dancing Rune Weapon', 1),
    (11, 200, 2031, 'Improved Icy Touch', 3),
    (11, 201, 2020, 'Runic Power Mastery', 2),
    (11, 212, 1973, 'Black Ice', 5),
    (11, 220, 2042, 'Icy Talons', 5),

    -- Unholy Death Knight DPS
    (12,   0, 2082, 'Vicious Strikes', 2),
    (12,   1, 1932, 'Virulence', 3),
    (12,  11, 1933, 'Morbidity', 3),
    (12,  13, 1934, 'Ravenous Dead', 3),
    (12,  21, 2047, 'Necrosis', 5),
    (12,  32, 2004, 'Blood-Caked Blade', 3),
    (12,  33, 2225, 'Night of the Dead', 2),
    (12,  40, 1996, 'Unholy Blight', 1),
    (12,  41, 2005, 'Impurity', 5),
    (12,  42, 2011, 'Dirge', 2),
    (12,  53, 1984, 'Master of Ghouls', 1),
    (12,  60, 2285, 'Desolation', 5),
    (12,  63, 2085, 'Ghoul Frenzy', 1),
    (12,  71, 1962, 'Crypt Fever', 3),
    (12,  72, 2007, 'Bone Shield', 1),
    (12,  80, 2003, 'Wandering Plague', 3),
    (12,  81, 2043, 'Ebon Plaguebringer', 3),
    (12,  91, 2036, 'Rage of Rivendare', 5),
    (12, 101, 2000, 'Summon Gargoyle', 1),
    (12, 200, 2031, 'Improved Icy Touch', 3),
    (12, 201, 2020, 'Runic Power Mastery', 2),
    (12, 212, 1973, 'Black Ice', 4),
    (12, 213, 2022, 'Nerves of Cold Steel', 3),
    (12, 220, 2042, 'Icy Talons', 5),
    (12, 233, 1971, 'Endless Winter', 2),

    -- Frost Death Knight DPS
    (23,   0, 1939, 'Butchery', 2),
    (23,   1, 1945, 'Subversion', 3),
    (23,  10, 1938, 'Bladed Armor', 5),
    (23,  21, 1943, 'Dark Conviction', 5),
    (23, 200, 2031, 'Improved Icy Touch', 3),
    (23, 201, 2020, 'Runic Power Mastery', 2),
    (23, 212, 1973, 'Black Ice', 5),
    (23, 213, 2022, 'Nerves of Cold Steel', 3),
    (23, 220, 2042, 'Icy Talons', 5),
    (23, 222, 2048, 'Annihilation', 3),
    (23, 231, 2044, 'Killing Machine', 5),
    (23, 232, 1981, 'Chill of the Grave', 2),
    (23, 233, 1971, 'Endless Winter', 2),
    (23, 242, 2030, 'Glacier Rot', 3),
    (23, 250, 2223, 'Improved Icy Talons', 1),
    (23, 251, 1993, 'Merciless Combat', 2),
    (23, 252, 1992, 'Rime', 3),
    (23, 270, 2284, 'Threat of Thassarian', 3),
    (23, 271, 2210, 'Blood of the North', 3),
    (23, 272, 1979, 'Unbreakable Armor', 1),
    (23, 281, 1975, 'Frost Strike', 1),
    (23, 282, 2040, 'Guile of Gorefiend', 3),
    (23, 291, 1998, 'Tundra Stalker', 5),
    (23, 301, 1989, 'Howling Blast', 1);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (
    23, 24, 25, 26, 27,
    65, 66, 67, 68, 69, 70,
    340, 341, 342, 343, 344, 345, 346, 347, 348, 349,
    500, 501, 502, 503, 504, 505, 506, 507, 508, 509,
    510, 511, 512, 513, 514, 515, 516, 517, 518,
    520, 521, 522, 523, 524, 525, 526, 527, 528, 529
);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (
    23, 24, 25, 26, 27,
    65, 66, 67, 68, 69, 70,
    340, 341, 342, 343, 344, 345, 346, 347, 348, 349,
    500, 501, 502, 503, 504, 505, 506, 507, 508, 509,
    510, 511, 512, 513, 514, 515, 516, 517, 518,
    520, 521, 522, 523, 524, 525, 526, 527, 528, 529
);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (
    23, 24, 25, 26, 27,
    65, 66, 67, 68, 69, 70,
    340, 341, 342, 343, 344, 345, 346, 347, 348, 349,
    500, 501, 502, 503, 504, 505, 506, 507, 508, 509,
    510, 511, 512, 513, 514, 515, 516, 517, 518,
    520, 521, 522, 523, 524, 525, 526, 527, 528, 529
);

-- -----------------------------------------------------------------------
-- BLOOD DEATH KNIGHT PVE PROFILE (14)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (500, 14,  0, 'Mind Freeze',                    1, 1, 1, 0),
    (501, 14, 10, 'Vampiric Blood',                 0, 0, 1, 0),
    (502, 14, 20, 'Rune Tap',                       0, 0, 1, 0),
    (503, 14, 30, 'Death and Decay (3+ targets)',  0, 0, 1, 0),
    (504, 14, 40, 'Icy Touch upkeep',              0, 0, 1, 1),
    (505, 14, 45, 'Plague Strike upkeep',          0, 0, 1, 1),
    (506, 14, 50, 'Pestilence spread',             0, 0, 1, 0),
    (507, 14, 60, 'Death Strike',                  0, 0, 1, 0),
    (508, 14, 70, 'Rune Strike',                   0, 0, 1, 0),
    (509, 14, 80, 'Heart Strike',                  0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5000, 500, 0, 0, 47528, 0, 0, 0, 'enemy_primary'),
    (5010, 501, 0, 0, 55233, 0, 0, 0, 'self'),
    (5020, 502, 0, 0, 48982, 0, 1, 0, 'self'),
    (5030, 503, 0, 0, 49938, 0, 0, 0, 'enemy_primary'),
    (5040, 504, 0, 0, 45477, 0, 0, 0, 'enemy_primary'),
    (5050, 505, 0, 0, 45462, 0, 0, 0, 'enemy_primary'),
    (5060, 506, 0, 0, 50842, 0, 0, 0, 'enemy_primary'),
    (5070, 507, 0, 0, 49998, 0, 1, 0, 'enemy_primary'),
    (5080, 508, 0, 0, 56815, 0, 1, 0, 'enemy_primary'),
    (5090, 509, 0, 0, 55050, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5010, 501, 0, 'self',  'hp_pct',             3, 45.0, ''),
    (5020, 502, 0, 'self',  'hp_pct',             3, 65.0, ''),
    (5030, 503, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (5040, 504, 0, 'enemy', 'aura',               7, 55095.0, ''),
    (5041, 504, 1, 'enemy', 'aura_remaining_secs',3,   3.0, '55095'),
    (5050, 505, 0, 'enemy', 'aura',               7, 55078.0, ''),
    (5051, 505, 1, 'enemy', 'aura_remaining_secs',3,   3.0, '55078'),
    (5060, 506, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (5061, 506, 1, 'enemy', 'aura',               6, 55095.0, ''),
    (5062, 506, 2, 'enemy', 'aura',               6, 55078.0, ''),
    (5070, 507, 0, 'self',  'hp_pct',             3, 85.0, ''),
    (5080, 508, 0, 'self',  'runic_power',        5, 20.0, ''),
    (5090, 509, 0, 'enemy', 'nearby_enemies',     3, 2.0,  '10');

-- -----------------------------------------------------------------------
-- UNHOLY DEATH KNIGHT PVE PROFILE (6)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (510, 6,  0, 'Mind Freeze',                     1, 1, 1, 0),
    (511, 6, 10, 'Summon Gargoyle',                 0, 0, 1, 0),
    (512, 6, 20, 'Death and Decay (3+ targets)',    0, 0, 1, 0),
    (513, 6, 30, 'Icy Touch upkeep',                0, 0, 1, 1),
    (514, 6, 35, 'Plague Strike upkeep',            0, 0, 1, 1),
    (515, 6, 40, 'Pestilence spread',               0, 0, 1, 0),
    (516, 6, 50, 'Scourge Strike',                  0, 0, 1, 0),
    (517, 6, 60, 'Death Coil (Runic Power dump)',   0, 0, 1, 0),
    (518, 6, 70, 'Blood Strike',                    0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5100, 510, 0, 0, 47528, 0, 0, 0, 'enemy_primary'),
    (5110, 511, 0, 0, 49206, 0, 1, 0, 'enemy_primary'),
    (5120, 512, 0, 0, 49938, 0, 0, 0, 'enemy_primary'),
    (5130, 513, 0, 0, 45477, 0, 0, 0, 'enemy_primary'),
    (5140, 514, 0, 0, 45462, 0, 0, 0, 'enemy_primary'),
    (5150, 515, 0, 0, 50842, 0, 0, 0, 'enemy_primary'),
    (5160, 516, 0, 0, 55090, 0, 0, 0, 'enemy_primary'),
    (5170, 517, 0, 0, 47541, 0, 0, 0, 'enemy_primary'),
    (5180, 518, 0, 0, 49930, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5110, 511, 0, 'self',  'runic_power',        5, 60.0, ''),
    (5111, 511, 1, 'enemy', 'nearby_enemies',     3, 2.0,  '10'),
    (5120, 512, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (5130, 513, 0, 'enemy', 'aura',               7, 55095.0, ''),
    (5131, 513, 1, 'enemy', 'aura_remaining_secs',3,   3.0, '55095'),
    (5140, 514, 0, 'enemy', 'aura',               7, 55078.0, ''),
    (5141, 514, 1, 'enemy', 'aura_remaining_secs',3,   3.0, '55078'),
    (5150, 515, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (5151, 515, 1, 'enemy', 'aura',               6, 55095.0, ''),
    (5152, 515, 2, 'enemy', 'aura',               6, 55078.0, ''),
    (5170, 517, 0, 'self',  'runic_power',        5, 40.0, ''),
    (5180, 518, 0, 'enemy', 'nearby_enemies',     3, 2.0,  '10');

-- -----------------------------------------------------------------------
-- FROST DEATH KNIGHT PVE PROFILE (34)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (520, 34,  0, 'Mind Freeze',                      1, 1, 1, 0),
    (521, 34, 10, 'Death and Decay (4+ targets)',     0, 0, 1, 0),
    (522, 34, 20, 'Icy Touch upkeep',                 0, 0, 1, 1),
    (523, 34, 25, 'Plague Strike upkeep',             0, 0, 1, 1),
    (524, 34, 30, 'Howling Blast (2+ targets)',       0, 0, 1, 0),
    (525, 34, 35, 'Pestilence spread',                0, 0, 1, 0),
    (526, 34, 40, 'Obliterate',                       0, 0, 1, 0),
    (527, 34, 45, 'Howling Blast (Rime proc)',        0, 0, 1, 0),
    (528, 34, 50, 'Frost Strike (Killing Machine)',   0, 0, 1, 0),
    (529, 34, 60, 'Frost Strike (Runic Power dump)',  0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5200, 520, 0, 0, 47528, 0, 0, 0, 'enemy_primary'),
    (5210, 521, 0, 0, 49938, 0, 0, 0, 'enemy_primary'),
    (5220, 522, 0, 0, 45477, 0, 0, 0, 'enemy_primary'),
    (5230, 523, 0, 0, 45462, 0, 0, 0, 'enemy_primary'),
    (5240, 524, 0, 0, 49184, 0, 0, 0, 'enemy_primary'),
    (5250, 525, 0, 0, 50842, 0, 0, 0, 'enemy_primary'),
    (5260, 526, 0, 0, 49020, 0, 0, 0, 'enemy_primary'),
    (5270, 527, 0, 0, 49184, 0, 0, 0, 'enemy_primary'),
    (5280, 528, 0, 0, 49143, 0, 0, 0, 'enemy_primary'),
    (5290, 529, 0, 0, 49143, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5210, 521, 0, 'enemy', 'nearby_enemies',     5, 4.0,  '10'),
    (5220, 522, 0, 'enemy', 'aura',               7, 55095.0, ''),
    (5221, 522, 1, 'enemy', 'aura_remaining_secs',3,   3.0, '55095'),
    (5230, 523, 0, 'enemy', 'aura',               7, 55078.0, ''),
    (5231, 523, 1, 'enemy', 'aura_remaining_secs',3,   3.0, '55078'),
    (5240, 524, 0, 'enemy', 'nearby_enemies',     5, 2.0,  '10'),
    (5250, 525, 0, 'enemy', 'nearby_enemies',     5, 3.0,  '10'),
    (5251, 525, 1, 'enemy', 'aura',               6, 55095.0, ''),
    (5252, 525, 2, 'enemy', 'aura',               6, 55078.0, ''),
    (5270, 527, 0, 'self',  'aura',               6, 59057.0, ''),
    (5280, 528, 0, 'self',  'aura',               6, 51130.0, ''),
    (5281, 528, 1, 'self',  'runic_power',        5, 40.0, ''),
    (5290, 529, 0, 'self',  'runic_power',        5, 40.0, '');
