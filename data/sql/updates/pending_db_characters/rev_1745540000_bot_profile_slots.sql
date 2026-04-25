CREATE TABLE IF NOT EXISTS `character_bot_profile_slots` (
    `characterGuid` BIGINT UNSIGNED NOT NULL,
    `activeSlot` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`characterGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Tracks the active moveset profile slot (1-10) per bot character.';
