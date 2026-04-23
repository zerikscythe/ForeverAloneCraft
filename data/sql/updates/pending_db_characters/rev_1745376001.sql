CREATE TABLE IF NOT EXISTS `living_world_account_alt_runtime` (
    `runtime_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `source_account_id` INT UNSIGNED NOT NULL,
    `source_character_guid` BIGINT UNSIGNED NOT NULL,
    `owner_character_guid` BIGINT UNSIGNED NULL,
    `clone_account_id` INT UNSIGNED NOT NULL,
    `clone_character_guid` BIGINT UNSIGNED NULL,
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
    `last_clean_sync_time` INT UNSIGNED NULL,
    `last_recovery_check_time` INT UNSIGNED NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`runtime_id`),
    UNIQUE KEY `uk_living_world_runtime_source`
        (`source_account_id`, `source_character_guid`),
    UNIQUE KEY `uk_living_world_runtime_clone`
        (`clone_account_id`, `clone_character_guid`),
    KEY `idx_living_world_runtime_account_state`
        (`source_account_id`, `state`)
) ENGINE=InnoDB;
