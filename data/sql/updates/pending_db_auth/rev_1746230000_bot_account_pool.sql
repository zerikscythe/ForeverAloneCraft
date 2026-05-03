CREATE TABLE IF NOT EXISTS `living_world_bot_account_pool` (
    `account_id`   INT UNSIGNED    NOT NULL,
    `is_available` TINYINT(1)      NOT NULL DEFAULT 1,
    `reserved_for` BIGINT UNSIGNED NULL,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Pool of dedicated bot accounts for null-socket companion spawning.';

-- Reset any accounts that were left locked from a previous run (no release path existed before this patch).
UPDATE `living_world_bot_account_pool` SET `is_available` = 1, `reserved_for` = NULL;
