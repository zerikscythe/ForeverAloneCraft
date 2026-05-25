-- rev_living_world_048_shaman_pve_doctrine_reset (world DB)
--
-- Clean Shaman PvE doctrine reset:
--   * replace the old starter Elemental profile with a class-specific PvE profile
--   * rebuild the shallow Enhancement placeholder profile into a real PvE doctrine
--   * replace the old class-split Restoration healer rows with a modernized PvE profile
--   * refresh Elemental / Restoration talent templates and add a dedicated
--     Enhancement talent template sourced from current WotLK Classic guide material
--
-- Human/source notes:
--   Elemental PvE:
--     https://www.icy-veins.com/wotlk-classic/elemental-shaman-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/elemental-shaman-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/elemental-shaman-dps-pve-spec-builds-talents-glyphs
--   Enhancement PvE:
--     https://www.icy-veins.com/wotlk-classic/enhancement-shaman-dps-pve-guide
--     https://www.icy-veins.com/wotlk-classic/enhancement-shaman-dps-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/enhancement-shaman-dps-pve-spec-builds-talents-glyphs
--   Restoration PvE:
--     https://www.icy-veins.com/wotlk-classic/restoration-shaman-healer-pve-guide
--     https://www.icy-veins.com/wotlk-classic/restoration-shaman-healer-pve-rotation-cooldowns-abilities
--     https://www.icy-veins.com/wotlk-classic/restoration-shaman-healer-pve-spec-builds-talents-glyphs
--
-- Local DBC-backed spell references used here:
--   Wind Shear          = 57994
--   Flame Shock         = 8050     (upkeep checks use 49233)
--   Lava Burst          = 51505
--   Chain Lightning     = 421
--   Lightning Bolt      = 403
--   Earth Shock         = 8042
--   Thunderstorm        = 51490
--   Elemental Mastery   = 16166
--   Stormstrike         = 17364
--   Lava Lash           = 60103
--   Feral Spirit        = 51533
--   Shamanistic Rage    = 30823
--   Lightning Shield    = 324
--   Maelstrom Weapon    = 53817 (buff aura)
--   Earth Shield        = 974
--   Water Shield        = 52127
--   Riptide             = 61295
--   Lesser Healing Wave = 8004
--   Chain Heal          = 1064
--   Healing Wave        = 331
--   Mana Tide Totem     = 16190
--
-- Design note:
--   This pass intentionally keeps totems and weapon-imbue ceremony light.
--   The goal is a strong class-specific spell priority spine first, with
--   richer totem / imbue / utility behavior layered in later.

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
    ( 7, 'Elemental',   'DPS',  'Shaman', 'PvE', '', 'Shaman Elemental DPS',
      'Priority-based PvE Elemental Shaman profile rebuilt from current WotLK Classic guide material.',
      1, 40, 70, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (25, 'Enhancement', 'DPS',  'Shaman', 'PvE', '', 'Shaman Enhancement DPS',
      'Priority-based PvE Enhancement Shaman profile rebuilt from current WotLK Classic guide material.',
      1, 30, 60, 0, 0, 0, 3, 10, 1, 100, 165, 80, 185, 240, 145, 175),
    (33, 'Restoration', 'HEAL', 'Shaman', 'PvE', '', 'Shaman Restoration Healer',
      'Priority-based PvE Restoration Shaman profile rebuilt from current WotLK Classic guide material.',
      1, 35, 60, 1, 2, 0, 3, 12, 1, 90, 170, 75, 245, 270, 130, 175)
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
-- SHAMAN TALENT TEMPLATES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_talent_template`
    (`template_id`, `spec_key`, `class_id`, `variant_key`, `display_name`, `description`)
VALUES
    (13, 'Elemental',   7, '', 'Elemental Shaman DPS',
        'PvE Elemental Shaman talent template refreshed against current WotLK Classic guide material.'),
    (14, 'Restoration', 7, '', 'Restoration Shaman Healer',
        'PvE Restoration Shaman healer template refreshed against current WotLK Classic guide material.'),
    (24, 'Enhancement', 7, '', 'Enhancement Shaman DPS',
        'PvE Enhancement Shaman talent template based on current WotLK Classic guide material.')
ON DUPLICATE KEY UPDATE
    `spec_key`     = VALUES(`spec_key`),
    `class_id`     = VALUES(`class_id`),
    `variant_key`  = VALUES(`variant_key`),
    `display_name` = VALUES(`display_name`),
    `description`  = VALUES(`description`);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` IN (13, 14, 24);

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    -- Elemental Shaman
    (13,   2,  563, 'Concussion', 5),
    (13,  10,  561, 'Call of Flame', 3),
    (13,  11, 1640, 'Elemental Warding', 3),
    (13,  21,  574, 'Elemental Focus', 1),
    (13,  22,  565, 'Elemental Fury', 5),
    (13,  30,  567, 'Improved Fire Nova', 2),
    (13,  33, 1642, 'Eye of the Storm', 3),
    (13,  40, 1641, 'Elemental Reach', 2),
    (13,  41,  562, 'Call of Thunder', 1),
    (13,  43, 1682, 'Unrelenting Storm', 3),
    (13,  50, 1685, 'Elemental Precision', 3),
    (13,  52,  721, 'Lightning Mastery', 5),
    (13,  61,  573, 'Elemental Mastery', 1),
    (13,  62, 2052, 'Storm, Earth and Fire', 3),
    (13,  70, 2262, 'Booming Echoes', 2),
    (13,  71, 2049, 'Elemental Oath', 2),
    (13,  72, 1686, 'Lightning Overload', 3),
    (13,  81, 1687, 'Totem of Wrath', 1),
    (13,  82, 2051, 'Lava Flows', 3),
    (13,  91, 2252, 'Shamanism', 5),
    (13, 101, 2053, 'Thunderstorm', 1),
    (13, 202,  614, 'Ancestral Knowledge', 5),
    (13, 211,  613, 'Thundering Strikes', 5),
    (13, 220,  611, 'Elemental Weapons', 3),
    (13, 222,  617, 'Shamanistic Focus', 1),

    -- Enhancement Shaman
    (24,   0,  610, 'Enhancing Totems', 3),
    (24, 202,  563, 'Concussion', 5),
    (24,   2,  614, 'Ancestral Knowledge', 5),
    (24, 210,  561, 'Call of Flame', 2),
    (24, 211, 1640, 'Elemental Warding', 3),
    (24,  11,  613, 'Thundering Strikes', 5),
    (24, 212, 1645, 'Elemental Devastation', 3),
    (24,  13,  607, 'Improved Shields', 1),
    (24,  20,  611, 'Elemental Weapons', 3),
    (24, 222,  565, 'Elemental Fury', 5),
    (24, 230,  567, 'Improved Fire Nova', 2),
    (24,  31,  602, 'Flurry', 5),
    (24,  41,  616, 'Spirit Weapons', 1),
    (24,  42, 2083, 'Mental Dexterity', 3),
    (24,  50, 1689, 'Unleashed Rage', 3),
    (24,  52, 1643, 'Weapon Mastery', 3),
    (24,  60, 1692, 'Dual Wield Specialization', 3),
    (24,  61, 1690, 'Dual Wield', 1),
    (24,  62,  901, 'Stormstrike', 1),
    (24,  70, 2055, 'Static Shock', 3),
    (24,  71, 2249, 'Lava Lash', 1),
    (24,  80, 1691, 'Mental Quickness', 3),
    (24,  81, 1693, 'Shamanistic Rage', 1),
    (24,  91, 2057, 'Maelstrom Weapon', 5),
    (24, 101, 2058, 'Feral Spirit', 1),

    -- Restoration Shaman
    (14, 200,  610, 'Enhancing Totems', 3),
    (14,   1,  586, 'Improved Healing Wave', 5),
    (14, 202,  614, 'Ancestral Knowledge', 2),
    (14, 211,  613, 'Thundering Strikes', 5),
    (14,  12,  593, 'Tidal Focus', 5),
    (14, 213,  607, 'Improved Shields', 3),
    (14,  20,  583, 'Improved Water Shield', 3),
    (14,  21,  587, 'Healing Focus', 3),
    (14,  22,  582, 'Tidal Force', 1),
    (14,  23,  581, 'Ancestral Healing', 3),
    (14,  31,  588, 'Restorative Totems', 3),
    (14,  32,  594, 'Tidal Mastery', 5),
    (14,  40, 1648, 'Healing Way', 3),
    (14,  42,  591, 'Nature''s Swiftness', 1),
    (14,  52,  592, 'Purification', 5),
    (14,  61,  590, 'Mana Tide Totem', 1),
    (14,  62, 2084, 'Cleanse Spirit', 1),
    (14,  70, 2060, 'Blessing of the Eternals', 2),
    (14,  71, 1697, 'Improved Chain Heal', 2),
    (14,  72, 1696, 'Nature''s Blessing', 3),
    (14,  80, 2061, 'Ancestral Awakening', 3),
    (14,  81, 1698, 'Earth Shield', 1),
    (14,  82, 2059, 'Improved Earth Shield', 2),
    (14,  91, 2063, 'Tidal Waves', 5),
    (14, 101, 2064, 'Riptide', 1);

-- -----------------------------------------------------------------------
-- ENTRY CLEANUP
-- -----------------------------------------------------------------------
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (
    28, 29, 30, 31, 32,
    91, 92, 93, 94, 95, 96, 97,
    250, 251, 252, 253, 254,
    530, 531, 532, 533, 534, 535, 536,
    540, 541, 542, 543, 544, 545, 546, 547, 548, 549,
    550, 551, 552, 553, 554, 555, 556, 557
);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (
    28, 29, 30, 31, 32,
    91, 92, 93, 94, 95, 96, 97,
    250, 251, 252, 253, 254,
    530, 531, 532, 533, 534, 535, 536,
    540, 541, 542, 543, 544, 545, 546, 547, 548, 549,
    550, 551, 552, 553, 554, 555, 556, 557
);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (
    28, 29, 30, 31, 32,
    91, 92, 93, 94, 95, 96, 97,
    250, 251, 252, 253, 254,
    530, 531, 532, 533, 534, 535, 536,
    540, 541, 542, 543, 544, 545, 546, 547, 548, 549,
    550, 551, 552, 553, 554, 555, 556, 557
);

-- -----------------------------------------------------------------------
-- ELEMENTAL SHAMAN PVE PROFILE (7)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (530,  7,  0, 'Wind Shear',                     1, 1, 1, 0),
    (531,  7, 10, 'Elemental Mastery',             0, 0, 1, 0),
    (532,  7, 20, 'Thunderstorm (mana / 3+ targets)', 0, 0, 1, 1),
    (533,  7, 30, 'Flame Shock upkeep',            0, 0, 1, 1),
    (534,  7, 40, 'Lava Burst',                    0, 0, 1, 0),
    (535,  7, 50, 'Chain Lightning (2+ targets)',  0, 0, 1, 0),
    (536,  7, 60, 'Lightning Bolt',                0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5300, 530, 0, 0, 57994, 0, 0, 0, 'enemy_primary'),
    (5310, 531, 0, 0, 16166, 0, 0, 0, 'self'),
    (5320, 532, 0, 0, 51490, 0, 0, 0, 'self'),
    (5330, 533, 0, 0,  8050, 0, 0, 0, 'enemy_primary'),
    (5340, 534, 0, 0, 51505, 0, 0, 0, 'enemy_primary'),
    (5350, 535, 0, 0,   421, 0, 0, 0, 'enemy_primary'),
    (5360, 536, 0, 0,   403, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5310, 531, 0, 'enemy', 'nearby_enemies',      3, 2.0,  '10'),
    (5320, 532, 0, 'self',  'power_pct',           3, 45.0, ''),
    (5321, 532, 1, 'enemy', 'nearby_enemies',      5, 3.0,  '10'),
    (5330, 533, 0, 'enemy', 'aura',                7, 49233.0, ''),
    (5331, 533, 1, 'enemy', 'aura_remaining_secs', 3, 3.0,  '49233'),
    (5350, 535, 0, 'enemy', 'nearby_enemies',      5, 2.0,  '10');

-- -----------------------------------------------------------------------
-- ENHANCEMENT SHAMAN PVE PROFILE (25)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (540, 25,  0, 'Wind Shear',                      1, 1, 1, 0),
    (541, 25, 10, 'Feral Spirit',                    0, 0, 1, 0),
    (5416,25, 15, 'Fire Elemental Totem',            0, 0, 1, 0),
    (542, 25, 20, 'Shamanistic Rage',                0, 0, 1, 0),
    (543, 25, 30, 'Lightning Shield upkeep',         0, 0, 1, 0),
    (544, 25, 40, 'Stormstrike',                     0, 0, 1, 0),
    (545, 25, 50, 'Flame Shock upkeep',              0, 0, 1, 1),
    (546, 25, 60, 'Chain Lightning (Maelstrom 5, 2+ targets)', 0, 0, 1, 0),
    (547, 25, 65, 'Lightning Bolt (Maelstrom 5)',    0, 0, 1, 0),
    (548, 25, 70, 'Earth Shock',                     0, 0, 1, 0),
    (549, 25, 80, 'Lava Lash',                       0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5400, 540, 0, 0, 57994, 0, 0, 0, 'enemy_primary'),
    (5410, 541, 0, 0, 51533, 0, 0, 0, 'self'),
    (54160, 5416, 0, 0, 2894, 0, 0, 0, 'self'),
    (5420, 542, 0, 0, 30823, 0, 0, 0, 'self'),
    (5430, 543, 0, 0,   324, 0, 0, 0, 'self'),
    (5440, 544, 0, 0, 17364, 0, 0, 0, 'enemy_primary'),
    (5450, 545, 0, 0,  8050, 0, 0, 0, 'enemy_primary'),
    (5460, 546, 0, 0,   421, 0, 0, 0, 'enemy_primary'),
    (5470, 547, 0, 0,   403, 0, 0, 0, 'enemy_primary'),
    (5480, 548, 0, 0,  8042, 0, 0, 0, 'enemy_primary'),
    (5490, 549, 0, 0, 60103, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5420, 542, 0, 'self',  'power_pct',           3, 35.0, ''),
    (5430, 543, 0, 'self',  'aura',                7, 324.0, ''),
    (5450, 545, 0, 'enemy', 'aura',                7, 49233.0, ''),
    (5451, 545, 1, 'enemy', 'aura_remaining_secs', 3, 3.0,  '49233'),
    (5460, 546, 0, 'self',  'aura_stacks',         5, 5.0,  '53817'),
    (5461, 546, 1, 'enemy', 'nearby_enemies',      5, 2.0,  '10'),
    (5470, 547, 0, 'self',  'aura_stacks',         5, 5.0,  '53817');

-- -----------------------------------------------------------------------
-- RESTORATION SHAMAN PVE PROFILE (33)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (550, 33,  0, 'Wind Shear',               1, 1, 1, 0),
    (551, 33, 10, 'Earth Shield',             0, 1, 1, 0),
    (552, 33, 20, 'Water Shield',             0, 0, 1, 0),
    (553, 33, 30, 'Riptide',                  0, 1, 1, 0),
    (554, 33, 40, 'Lesser Healing Wave Emergency', 0, 0, 1, 0),
    (555, 33, 50, 'Chain Heal',               0, 0, 1, 0),
    (556, 33, 60, 'Healing Wave Moderate',    0, 0, 1, 0),
    (557, 33, 70, 'Mana Tide Totem',          0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (5500, 550, 0, 0, 57994, 0, 0, 0, 'enemy_primary'),
    (5510, 551, 0, 0,   974, 0, 0, 0, 'owner'),
    (5520, 552, 0, 0, 52127, 0, 0, 0, 'self'),
    (5530, 553, 0, 0, 61295, 0, 0, 0, 'lowest_hp_party'),
    (5540, 554, 0, 0,  8004, 0, 0, 0, 'lowest_hp_party'),
    (5550, 555, 0, 0,  1064, 0, 0, 0, 'lowest_hp_party'),
    (5560, 556, 0, 0,   331, 0, 0, 0, 'lowest_hp_party'),
    (5570, 557, 0, 0, 16190, 0, 0, 0, 'self');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (5510, 551, 0, 'owner',           'aura',   7, 0.0,  '974'),
    (5520, 552, 0, 'self',            'aura',   7, 0.0,  '52127'),
    (5530, 553, 0, 'lowest_hp_party', 'hp_pct', 3, 75.0, ''),
    (5540, 554, 0, 'lowest_hp_party', 'hp_pct', 3, 40.0, ''),
    (5550, 555, 0, 'lowest_hp_party', 'hp_pct', 3, 68.0, ''),
    (5560, 556, 0, 'lowest_hp_party', 'hp_pct', 3, 82.0, ''),
    (5570, 557, 0, 'self',            'power_pct', 3, 30.0, '');
