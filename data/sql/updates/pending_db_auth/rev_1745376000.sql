CREATE TABLE IF NOT EXISTS `living_world_bot_account_pool` (
    `account_id`   INT UNSIGNED NOT NULL,
    `is_available` TINYINT(1)   NOT NULL DEFAULT 1,
    `reserved_for` BIGINT UNSIGNED NULL,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB;
