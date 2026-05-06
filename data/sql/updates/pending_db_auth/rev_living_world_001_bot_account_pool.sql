-- LivingWorld: auth-side bot account pool for runtime clone leasing
CREATE TABLE IF NOT EXISTS `living_world_bot_account_pool` (
    `account_id` INT UNSIGNED NOT NULL,
    `account_name` VARCHAR(32) NOT NULL,
    `is_enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `assigned_source_account_id` INT UNSIGNED NULL,
    `assigned_source_character_guid` BIGINT UNSIGNED NULL,
    `last_runtime_id` BIGINT UNSIGNED NULL,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`account_id`),
    UNIQUE KEY `uk_living_world_bot_account_pool_account_name` (`account_name`),
    KEY `idx_living_world_bot_account_pool_assignment` (`assigned_source_account_id`, `assigned_source_character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
