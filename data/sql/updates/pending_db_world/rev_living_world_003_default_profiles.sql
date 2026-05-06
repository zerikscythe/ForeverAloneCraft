-- LivingWorld: world-side default combat doctrine scaffolding and starter seeds
-- Enum encodings match `rev_living_world_002_runtime_and_profiles.sql`.

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_default_profile` (
    `default_profile_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `spec_key` VARCHAR(32) NOT NULL,
    `role_key` VARCHAR(16) NOT NULL,
    `display_name` VARCHAR(64) NOT NULL,
    `conservation_mode` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `mana_low_water` TINYINT UNSIGNED NOT NULL DEFAULT 55,
    `mana_high_water` TINYINT UNSIGNED NOT NULL DEFAULT 75,
    `enable_down_rank` TINYINT(1) NOT NULL DEFAULT 1,
    `down_rank_floor` TINYINT UNSIGNED NOT NULL DEFAULT 2,
    `default_aoe_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `default_aoe_min_targets` TINYINT UNSIGNED NOT NULL DEFAULT 2,
    `default_aoe_scan_radius` FLOAT NOT NULL DEFAULT 10,
    PRIMARY KEY (`default_profile_id`),
    UNIQUE KEY `uk_living_world_bot_combat_default_profile_spec_role` (`spec_key`, `role_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_default_entry` (
    `entry_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `default_profile_id` BIGINT UNSIGNED NOT NULL,
    `priority` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `label` VARCHAR(64) NOT NULL DEFAULT '',
    `is_interrupt` TINYINT(1) NOT NULL DEFAULT 0,
    `breaks_current_cast` TINYINT(1) NOT NULL DEFAULT 0,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `condition_logic` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`entry_id`),
    KEY `idx_living_world_bot_combat_default_entry_profile` (`default_profile_id`, `is_interrupt`, `priority`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_default_action` (
    `action_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `entry_id` BIGINT UNSIGNED NOT NULL,
    `slot` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `action_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `spell_base_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `item_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `rank_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `rank_value` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `target_key` VARCHAR(32) NOT NULL DEFAULT 'enemy',
    `aoe_mode` TINYINT UNSIGNED NULL,
    `aoe_min_targets` TINYINT UNSIGNED NULL,
    `aoe_radius` FLOAT NULL,
    PRIMARY KEY (`action_id`),
    UNIQUE KEY `uk_living_world_bot_combat_default_action_slot` (`entry_id`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_default_condition` (
    `condition_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `entry_id` BIGINT UNSIGNED NOT NULL,
    `sequence` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `subject_key` VARCHAR(32) NOT NULL DEFAULT '',
    `stat_key` VARCHAR(32) NOT NULL DEFAULT '',
    `comparison` TINYINT UNSIGNED NOT NULL DEFAULT 4,
    `numeric_value` FLOAT NOT NULL DEFAULT 0,
    `string_value` VARCHAR(64) NOT NULL DEFAULT '',
    PRIMARY KEY (`condition_id`),
    UNIQUE KEY `uk_living_world_bot_combat_default_condition_sequence` (`entry_id`, `sequence`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `living_world_bot_combat_default_profile` (
    `default_profile_id`,
    `spec_key`,
    `role_key`,
    `display_name`,
    `conservation_mode`,
    `mana_low_water`,
    `mana_high_water`,
    `enable_down_rank`,
    `down_rank_floor`,
    `default_aoe_mode`,
    `default_aoe_min_targets`,
    `default_aoe_scan_radius`
) VALUES
    (1, 'Arms',        'DPS', 'Warrior Arms DPS',             0,  0, 100, 0, 0, 0, 2, 10),
    (2, 'Retribution', 'DPS', 'Paladin Retribution DPS',      1, 55, 75, 1, 2, 0, 2, 10),
    (3, 'BeastMastery','DPS', 'Hunter Beast Mastery DPS',     0,  0, 100, 0, 0, 0, 2, 10),
    (4, 'Combat',      'DPS', 'Rogue Combat DPS',             0,  0, 100, 0, 0, 0, 2, 10),
    (5, 'Shadow',      'DPS', 'Priest Shadow DPS',            1, 50, 75, 1, 2, 0, 2, 10),
    (6, 'Unholy',      'DPS', 'Death Knight Unholy DPS',      0,  0, 100, 0, 0, 0, 2, 10),
    (7, 'Elemental',   'DPS', 'Shaman Elemental DPS',         1, 55, 75, 1, 2, 0, 2, 10),
    (8, 'Frost',       'DPS', 'Mage Frost DPS',               1, 50, 75, 1, 2, 0, 2, 10),
    (9, 'Affliction',  'DPS', 'Warlock Affliction DPS',       1, 45, 70, 0, 0, 0, 2, 10),
    (10,'Balance',     'DPS', 'Druid Balance DPS',            1, 55, 75, 1, 2, 0, 2, 10)
ON DUPLICATE KEY UPDATE
    `display_name` = VALUES(`display_name`),
    `conservation_mode` = VALUES(`conservation_mode`),
    `mana_low_water` = VALUES(`mana_low_water`),
    `mana_high_water` = VALUES(`mana_high_water`),
    `enable_down_rank` = VALUES(`enable_down_rank`),
    `down_rank_floor` = VALUES(`down_rank_floor`),
    `default_aoe_mode` = VALUES(`default_aoe_mode`),
    `default_aoe_min_targets` = VALUES(`default_aoe_min_targets`),
    `default_aoe_scan_radius` = VALUES(`default_aoe_scan_radius`);
