-- Create bot account pool table
CREATE TABLE IF NOT EXISTS `living_world_bot_account_pool` (
    `account_id`   INT UNSIGNED NOT NULL,
    `is_available` TINYINT(1)   NOT NULL DEFAULT 1,
    `reserved_for` BIGINT UNSIGNED NULL,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB;

-- Create BotHouse001-010 accounts (salt/verifier unused; bots log in via WorldSession directly)
INSERT IGNORE INTO `account`
    (`username`, `salt`, `verifier`, `email`, `reg_mail`, `expansion`)
VALUES
    ('BOTHOUSE001', UNHEX(SHA2('BOTHOUSE001_SALT', 256)), UNHEX(SHA2('BOTHOUSE001_VER', 256)), '', '', 2),
    ('BOTHOUSE002', UNHEX(SHA2('BOTHOUSE002_SALT', 256)), UNHEX(SHA2('BOTHOUSE002_VER', 256)), '', '', 2),
    ('BOTHOUSE003', UNHEX(SHA2('BOTHOUSE003_SALT', 256)), UNHEX(SHA2('BOTHOUSE003_VER', 256)), '', '', 2),
    ('BOTHOUSE004', UNHEX(SHA2('BOTHOUSE004_SALT', 256)), UNHEX(SHA2('BOTHOUSE004_VER', 256)), '', '', 2),
    ('BOTHOUSE005', UNHEX(SHA2('BOTHOUSE005_SALT', 256)), UNHEX(SHA2('BOTHOUSE005_VER', 256)), '', '', 2),
    ('BOTHOUSE006', UNHEX(SHA2('BOTHOUSE006_SALT', 256)), UNHEX(SHA2('BOTHOUSE006_VER', 256)), '', '', 2),
    ('BOTHOUSE007', UNHEX(SHA2('BOTHOUSE007_SALT', 256)), UNHEX(SHA2('BOTHOUSE007_VER', 256)), '', '', 2),
    ('BOTHOUSE008', UNHEX(SHA2('BOTHOUSE008_SALT', 256)), UNHEX(SHA2('BOTHOUSE008_VER', 256)), '', '', 2),
    ('BOTHOUSE009', UNHEX(SHA2('BOTHOUSE009_SALT', 256)), UNHEX(SHA2('BOTHOUSE009_VER', 256)), '', '', 2),
    ('BOTHOUSE010', UNHEX(SHA2('BOTHOUSE010_SALT', 256)), UNHEX(SHA2('BOTHOUSE010_VER', 256)), '', '', 2);

-- Populate the pool with the 10 accounts just created
INSERT IGNORE INTO `living_world_bot_account_pool` (`account_id`, `is_available`)
SELECT `id`, 1
FROM `account`
WHERE `username` IN (
    'BOTHOUSE001','BOTHOUSE002','BOTHOUSE003','BOTHOUSE004','BOTHOUSE005',
    'BOTHOUSE006','BOTHOUSE007','BOTHOUSE008','BOTHOUSE009','BOTHOUSE010'
);
