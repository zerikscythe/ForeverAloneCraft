-- rev_living_world_031_world_bot_virtual_loadouts (world DB)
--
-- Introduces V1 virtual loadout stat packages for Creature-backed world bots.
-- This slice intentionally covers only the safe stat subset that the current
-- Creature runtime can honor honestly: primary stats, health/mana, armor, and
-- physical attack power. Player-only rating semantics are deferred.

CREATE TABLE IF NOT EXISTS `living_world_bot_virtual_loadout` (
    `loadout_id` bigint unsigned NOT NULL AUTO_INCREMENT,
    `class_id` tinyint unsigned NOT NULL,
    `spec_key` varchar(32) DEFAULT NULL,
    `loadout_key` varchar(64) DEFAULT NULL,
    `gear_tier` tinyint unsigned NOT NULL DEFAULT 1,
    `display_name` varchar(64) NOT NULL,
    `description` varchar(255) DEFAULT NULL,
    `bonus_strength` int NOT NULL DEFAULT 0,
    `bonus_agility` int NOT NULL DEFAULT 0,
    `bonus_stamina` int NOT NULL DEFAULT 0,
    `bonus_intellect` int NOT NULL DEFAULT 0,
    `bonus_spirit` int NOT NULL DEFAULT 0,
    `bonus_health` int NOT NULL DEFAULT 0,
    `bonus_mana` int NOT NULL DEFAULT 0,
    `bonus_armor` int NOT NULL DEFAULT 0,
    `bonus_attack_power` int NOT NULL DEFAULT 0,
    `bonus_ranged_attack_power` int NOT NULL DEFAULT 0,
    PRIMARY KEY (`loadout_id`),
    UNIQUE KEY `uk_lw_bot_virtual_loadout_resolution`
        (`class_id`, `spec_key`, `loadout_key`, `gear_tier`),
    KEY `idx_lw_bot_virtual_loadout_class_tier` (`class_id`, `gear_tier`),
    KEY `idx_lw_bot_virtual_loadout_keys` (`loadout_key`, `spec_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `living_world_bot_virtual_loadout`
    (`class_id`, `spec_key`, `loadout_key`, `gear_tier`, `display_name`, `description`,
     `bonus_strength`, `bonus_agility`, `bonus_stamina`, `bonus_intellect`, `bonus_spirit`,
     `bonus_health`, `bonus_mana`, `bonus_armor`, `bonus_attack_power`, `bonus_ranged_attack_power`)
VALUES
    -- Class fallback rows: guaranteed coverage for existing questing/dungeon/raid gear bands.
    (1,  '', '', 1, 'Warrior Tier 1 Class Fallback',      'Questing fallback package for warrior world bots.', 80, 35, 110,  0,  0, 1400,    0, 550, 220,   0),
    (1,  '', '', 2, 'Warrior Tier 2 Class Fallback',      'Dungeon fallback package for warrior world bots.', 100, 45, 135,  0,  0, 1750,    0, 700, 300,   0),
    (1,  '', '', 3, 'Warrior Tier 3 Class Fallback',      'Raid fallback package for warrior world bots.', 125, 55, 165,  0,  0, 2200,    0, 900, 390,   0),
    (2,  '', '', 1, 'Paladin Tier 1 Class Fallback',      'Questing fallback package for paladin world bots.', 55, 20, 100, 45, 30, 1200, 350, 500, 160,   0),
    (2,  '', '', 2, 'Paladin Tier 2 Class Fallback',      'Dungeon fallback package for paladin world bots.', 70, 25, 120, 60, 40, 1500, 500, 620, 220,   0),
    (2,  '', '', 3, 'Paladin Tier 3 Class Fallback',      'Raid fallback package for paladin world bots.', 85, 30, 145, 75, 50, 1850, 650, 760, 290,   0),
    (3,  '', '', 1, 'Hunter Tier 1 Class Fallback',       'Questing fallback package for hunter world bots.', 20, 90,  85,  0,  0, 1000,   0, 300,  60, 220),
    (3,  '', '', 2, 'Hunter Tier 2 Class Fallback',       'Dungeon fallback package for hunter world bots.', 25, 110, 100, 10,  0, 1200, 100, 360,  80, 290),
    (3,  '', '', 3, 'Hunter Tier 3 Class Fallback',       'Raid fallback package for hunter world bots.', 30, 130, 120, 15,  0, 1450, 150, 430, 100, 370),
    (4,  '', '', 1, 'Rogue Tier 1 Class Fallback',        'Questing fallback package for rogue world bots.', 25, 100,  80,  0,  0,  950,   0, 250, 200,   0),
    (4,  '', '', 2, 'Rogue Tier 2 Class Fallback',        'Dungeon fallback package for rogue world bots.', 35, 120,  95,  0,  0, 1150,   0, 300, 270,   0),
    (4,  '', '', 3, 'Rogue Tier 3 Class Fallback',        'Raid fallback package for rogue world bots.', 45, 145, 110,  0,  0, 1400,   0, 360, 350,   0),
    (5,  '', '', 1, 'Priest Tier 1 Class Fallback',       'Questing fallback package for priest world bots.', 0, 10,  70, 95, 70,  850, 650, 150,   0,   0),
    (5,  '', '', 2, 'Priest Tier 2 Class Fallback',       'Dungeon fallback package for priest world bots.', 0, 10,  85, 115, 80, 1050, 820, 190,   0,   0),
    (5,  '', '', 3, 'Priest Tier 3 Class Fallback',       'Raid fallback package for priest world bots.', 0, 15, 100, 135, 90, 1300, 1000, 240,   0,   0),
    (6,  '', '', 1, 'Death Knight Tier 1 Class Fallback', 'Questing fallback package for death knight world bots.', 85, 25, 115,  0,  0, 1450,   0, 500, 240,   0),
    (6,  '', '', 2, 'Death Knight Tier 2 Class Fallback', 'Dungeon fallback package for death knight world bots.', 105, 30, 140,  0,  0, 1800,   0, 640, 320,   0),
    (6,  '', '', 3, 'Death Knight Tier 3 Class Fallback', 'Raid fallback package for death knight world bots.', 125, 35, 165,  0,  0, 2200,   0, 780, 400,   0),
    (7,  '', '', 1, 'Shaman Tier 1 Class Fallback',       'Questing fallback package for shaman world bots.', 35, 35,  90, 70, 45, 1050, 450, 350, 100, 120),
    (7,  '', '', 2, 'Shaman Tier 2 Class Fallback',       'Dungeon fallback package for shaman world bots.', 45, 45, 110, 85, 55, 1250, 550, 420, 140, 160),
    (7,  '', '', 3, 'Shaman Tier 3 Class Fallback',       'Raid fallback package for shaman world bots.', 55, 55, 130, 100, 65, 1500, 700, 500, 180, 210),
    (8,  '', '', 1, 'Mage Tier 1 Class Fallback',         'Questing fallback package for mage world bots.', 0, 10,  65, 110, 55,  800, 700, 120,   0,   0),
    (8,  '', '', 2, 'Mage Tier 2 Class Fallback',         'Dungeon fallback package for mage world bots.', 0, 10,  80, 130, 65,  980, 880, 150,   0,   0),
    (8,  '', '', 3, 'Mage Tier 3 Class Fallback',         'Raid fallback package for mage world bots.', 0, 15,  95, 155, 75, 1200, 1080, 190,   0,   0),
    (9,  '', '', 1, 'Warlock Tier 1 Class Fallback',      'Questing fallback package for warlock world bots.', 0, 10,  80, 100, 45, 1000, 650, 160,   0,   0),
    (9,  '', '', 2, 'Warlock Tier 2 Class Fallback',      'Dungeon fallback package for warlock world bots.', 0, 10,  95, 120, 55, 1200, 820, 200,   0,   0),
    (9,  '', '', 3, 'Warlock Tier 3 Class Fallback',      'Raid fallback package for warlock world bots.', 0, 15, 110, 145, 65, 1450, 980, 250,   0,   0),
    (11, '', '', 1, 'Druid Tier 1 Class Fallback',        'Questing fallback package for druid world bots.', 25, 45,  90, 70, 45, 1050, 450, 280,  80,  80),
    (11, '', '', 2, 'Druid Tier 2 Class Fallback',        'Dungeon fallback package for druid world bots.', 35, 60, 110, 85, 55, 1250, 560, 340, 120, 120),
    (11, '', '', 3, 'Druid Tier 3 Class Fallback',        'Raid fallback package for druid world bots.', 45, 75, 130, 100, 65, 1500, 700, 410, 160, 160),

    -- Spec-specific normal tier-1 rows.
    (1,  'Arms', '', 1, 'Warrior Arms Tier 1',                 'Normal tier-1 warrior arms loadout.', 90, 35, 110,  0,  0, 1400,   0, 520, 260,   0),
    (1,  'Fury', '', 1, 'Warrior Fury Tier 1',                 'Normal tier-1 warrior fury loadout.', 85, 45, 105,  0,  0, 1350,   0, 500, 280,   0),
    (1,  'Protection', '', 1, 'Warrior Protection Tier 1',     'Normal tier-1 warrior protection loadout.', 70, 20, 135,  0,  0, 1800,   0, 760, 150,   0),
    (2,  'Holy', '', 1, 'Paladin Holy Tier 1',                 'Normal tier-1 paladin holy loadout.', 20, 10,  95, 80, 55, 1150, 700, 420,  40,   0),
    (2,  'Protection', '', 1, 'Paladin Protection Tier 1',     'Normal tier-1 paladin protection loadout.', 65, 15, 130, 35, 20, 1700, 250, 760, 140,   0),
    (2,  'Retribution', '', 1, 'Paladin Retribution Tier 1',   'Normal tier-1 paladin retribution loadout.', 75, 25, 105, 35, 20, 1300, 300, 520, 210,   0),
    (3,  'Beast Mastery', '', 1, 'Hunter Beast Mastery Tier 1', 'Normal tier-1 hunter beast mastery loadout.', 20, 85,  90,  5,  0, 1050,  50, 320,  40, 240),
    (3,  'Marksmanship', '', 1, 'Hunter Marksmanship Tier 1',   'Normal tier-1 hunter marksmanship loadout.', 20, 95,  85, 10,  0, 1000, 100, 300,  40, 260),
    (3,  'Survival', '', 1, 'Hunter Survival Tier 1',           'Normal tier-1 hunter survival loadout.', 20, 90,  90, 20, 10, 1050, 120, 320,  50, 230),
    (4,  'Assassination', '', 1, 'Rogue Assassination Tier 1', 'Normal tier-1 rogue assassination loadout.', 20, 105, 80,  0,  0,  950,   0, 240, 210,   0),
    (4,  'Combat', '', 1, 'Rogue Combat Tier 1',               'Normal tier-1 rogue combat loadout.', 35, 95,  90,  0,  0, 1050,   0, 280, 240,   0),
    (4,  'Subtlety', '', 1, 'Rogue Subtlety Tier 1',           'Normal tier-1 rogue subtlety loadout.', 20, 100, 80,  0,  0,  950,   0, 230, 200,   0),
    (5,  'Discipline', '', 1, 'Priest Discipline Tier 1',      'Normal tier-1 priest discipline loadout.', 0, 10,  75, 100, 70,  900, 700, 170,   0,   0),
    (5,  'Holy', '', 1, 'Priest Holy Tier 1',                  'Normal tier-1 priest holy loadout.', 0, 10,  70,  95, 85,  850, 750, 150,   0,   0),
    (5,  'Shadow', '', 1, 'Priest Shadow Tier 1',              'Normal tier-1 priest shadow loadout.', 0, 10,  75, 105, 55,  900, 650, 160,   0,   0),
    (6,  'Blood', '', 1, 'Death Knight Blood Tier 1',          'Normal tier-1 death knight blood loadout.', 80, 20, 135,  0,  0, 1800,   0, 650, 220,   0),
    (6,  'Frost', '', 1, 'Death Knight Frost Tier 1',          'Normal tier-1 death knight frost loadout.', 90, 20, 115,  0,  0, 1500,   0, 520, 260,   0),
    (6,  'Unholy', '', 1, 'Death Knight Unholy Tier 1',        'Normal tier-1 death knight unholy loadout.', 85, 25, 115,  0,  0, 1500,   0, 500, 250,   0),
    (7,  'Elemental', '', 1, 'Shaman Elemental Tier 1',        'Normal tier-1 shaman elemental loadout.', 10, 20,  85,  90, 55, 1000, 600, 300,  20,  40),
    (7,  'Enhancement', '', 1, 'Shaman Enhancement Tier 1',    'Normal tier-1 shaman enhancement loadout.', 40, 60,  95,  35, 20, 1100, 350, 360, 160, 120),
    (7,  'Restoration', '', 1, 'Shaman Restoration Tier 1',    'Normal tier-1 shaman restoration loadout.', 20, 20,  85,  80, 70, 1000, 650, 320,  40,  40),
    (8,  'Arcane', '', 1, 'Mage Arcane Tier 1',                'Normal tier-1 mage arcane loadout.', 0, 10,  65, 120, 55,  800, 800, 120,   0,   0),
    (8,  'Fire', '', 1, 'Mage Fire Tier 1',                    'Normal tier-1 mage fire loadout.', 0, 10,  65, 110, 45,  780, 720, 120,   0,   0),
    (8,  'Frost', '', 1, 'Mage Frost Tier 1',                  'Normal tier-1 mage frost loadout.', 0, 15,  75, 105, 50,  900, 700, 160,   0,   0),
    (9,  'Affliction', '', 1, 'Warlock Affliction Tier 1',     'Normal tier-1 warlock affliction loadout.', 0, 10,  85,  95, 55, 1050, 650, 170,   0,   0),
    (9,  'Demonology', '', 1, 'Warlock Demonology Tier 1',     'Normal tier-1 warlock demonology loadout.', 0, 10,  95,  85, 45, 1150, 550, 200,   0,   0),
    (9,  'Destruction', '', 1, 'Warlock Destruction Tier 1',   'Normal tier-1 warlock destruction loadout.', 0, 10,  80, 110, 35,  950, 700, 150,   0,   0),
    (11, 'Balance', '', 1, 'Druid Balance Tier 1',             'Normal tier-1 druid balance loadout.', 10, 25,  90,  85, 60, 1050, 550, 260,  30,  30),
    (11, 'Feral', '', 1, 'Druid Feral Tier 1',                 'Normal tier-1 druid feral loadout.', 35, 60, 105,  20, 20, 1250, 150, 420, 140,  60),
    (11, 'Restoration', '', 1, 'Druid Restoration Tier 1',     'Normal tier-1 druid restoration loadout.', 15, 20,  85,  80, 75, 1000, 650, 260,  20,  20),

    -- Explicit loadout-key examples for finer normal variants.
    (8,  'Arcane', 'Mage_Arcane_PVE_Starter', 1, 'Mage Arcane PvE Starter', 'Variant-specific starter arcane mage loadout.', 0, 10, 70, 135, 55, 850, 950, 130, 0, 0),
    (11, 'Feral', 'Druid_Feral_PVE_01', 1, 'Druid Feral Cat PvE', 'Variant-specific cat-focused feral loadout.', 25, 80, 90, 20, 20, 1050, 100, 300, 180, 80)
ON DUPLICATE KEY UPDATE
    `display_name` = VALUES(`display_name`),
    `description` = VALUES(`description`),
    `bonus_strength` = VALUES(`bonus_strength`),
    `bonus_agility` = VALUES(`bonus_agility`),
    `bonus_stamina` = VALUES(`bonus_stamina`),
    `bonus_intellect` = VALUES(`bonus_intellect`),
    `bonus_spirit` = VALUES(`bonus_spirit`),
    `bonus_health` = VALUES(`bonus_health`),
    `bonus_mana` = VALUES(`bonus_mana`),
    `bonus_armor` = VALUES(`bonus_armor`),
    `bonus_attack_power` = VALUES(`bonus_attack_power`),
    `bonus_ranged_attack_power` = VALUES(`bonus_ranged_attack_power`);