-- LivingWorld: character-side runtime clone and player combat profile state
-- Enum encodings used here:
-- AccountAltRuntimeState:
--   0 PreparingClone
--   1 Active
--   2 SyncingBack
--   3 Recovering
--   4 Failed
--   5 SyncingEquipment
--   6 SyncingInventory
--   7 SyncingBank
-- BotCombatConservationMode:
--   0 FullForce
--   1 Conservative
--   2 JitCasting
-- BotCombatActionType:
--   0 Spell
--   1 Item
-- BotCombatRankMode:
--   0 BestKnown
--   1 ExactSpellId
--   2 SpecificRank
-- BotCombatConditionLogic:
--   0 All
--   1 Any
-- BotCombatAoEMode:
--   0 Centroid
--   1 Feet
-- BotCombatConditionOperator:
--   0 Equal
--   1 NotEqual
--   2 LessThan
--   3 LessThanOrEqual
--   4 GreaterThan
--   5 GreaterThanOrEqual
--   6 Has
--   7 NotHas
--   8 Exists

CREATE TABLE IF NOT EXISTS `living_world_account_alt_runtime` (
    `runtime_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `source_account_id` INT UNSIGNED NOT NULL,
    `source_character_guid` BIGINT UNSIGNED NOT NULL,
    `owner_character_guid` BIGINT UNSIGNED DEFAULT NULL,
    `clone_account_id` INT UNSIGNED NOT NULL,
    `clone_character_guid` BIGINT UNSIGNED DEFAULT NULL,
    `state` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `source_character_name` VARCHAR(12) NOT NULL,
    `reserved_source_character_name` VARCHAR(12) NOT NULL,
    `clone_character_name` VARCHAR(12) NOT NULL,
    `source_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `source_experience` INT UNSIGNED NOT NULL DEFAULT 0,
    `source_money` INT UNSIGNED NOT NULL DEFAULT 0,
    `clone_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `clone_experience` INT UNSIGNED NOT NULL DEFAULT 0,
    `clone_money` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_clean_sync_time` INT UNSIGNED DEFAULT NULL,
    `last_recovery_check_time` INT UNSIGNED DEFAULT NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`runtime_id`),
    UNIQUE KEY `uk_living_world_runtime_source` (`source_account_id`, `source_character_guid`),
    UNIQUE KEY `uk_living_world_runtime_clone` (`clone_account_id`, `clone_character_guid`),
    KEY `idx_living_world_runtime_account_state` (`source_account_id`, `state`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_profile` (
    `profile_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `source_character_guid` BIGINT UNSIGNED NOT NULL,
    `owner_account_id` INT UNSIGNED NOT NULL,
    `slot` TINYINT UNSIGNED NOT NULL,
    `profile_name` VARCHAR(64) NOT NULL DEFAULT '',
    `guessed_spec_key` VARCHAR(32) NOT NULL DEFAULT '',
    `guessed_role_key` VARCHAR(16) NOT NULL DEFAULT '',
    `spec_override_key` VARCHAR(32) NULL,
    `role_override_key` VARCHAR(16) NULL,
    `conservation_mode` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `mana_low_water` TINYINT UNSIGNED NOT NULL DEFAULT 55,
    `mana_high_water` TINYINT UNSIGNED NOT NULL DEFAULT 75,
    `enable_down_rank` TINYINT(1) NOT NULL DEFAULT 1,
    `down_rank_floor` TINYINT UNSIGNED NOT NULL DEFAULT 2,
    `default_aoe_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `default_aoe_min_targets` TINYINT UNSIGNED NOT NULL DEFAULT 2,
    `default_aoe_scan_radius` FLOAT NOT NULL DEFAULT 10,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`profile_id`),
    UNIQUE KEY `uk_living_world_bot_combat_profile_slot` (`source_character_guid`, `slot`),
    KEY `idx_living_world_bot_combat_profile_owner` (`owner_account_id`, `source_character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_profile_entry` (
    `entry_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `profile_id` BIGINT UNSIGNED NOT NULL,
    `priority` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `label` VARCHAR(64) NOT NULL DEFAULT '',
    `is_interrupt` TINYINT(1) NOT NULL DEFAULT 0,
    `breaks_current_cast` TINYINT(1) NOT NULL DEFAULT 0,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `condition_logic` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`entry_id`),
    KEY `idx_living_world_bot_combat_profile_entry_profile` (`profile_id`, `is_interrupt`, `priority`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_profile_action` (
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
    UNIQUE KEY `uk_living_world_bot_combat_profile_action_slot` (`entry_id`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_profile_condition` (
    `condition_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `entry_id` BIGINT UNSIGNED NOT NULL,
    `sequence` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `subject_key` VARCHAR(32) NOT NULL DEFAULT '',
    `stat_key` VARCHAR(32) NOT NULL DEFAULT '',
    `comparison` TINYINT UNSIGNED NOT NULL DEFAULT 4,
    `numeric_value` FLOAT NOT NULL DEFAULT 0,
    `string_value` VARCHAR(64) NOT NULL DEFAULT '',
    PRIMARY KEY (`condition_id`),
    UNIQUE KEY `uk_living_world_bot_combat_profile_condition_sequence` (`entry_id`, `sequence`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `living_world_bot_combat_runtime_selection` (
    `source_character_guid` BIGINT UNSIGNED NOT NULL,
    `active_profile_slot` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`source_character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
