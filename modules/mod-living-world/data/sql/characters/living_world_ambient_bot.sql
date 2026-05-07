-- living_world_ambient_bot (acore_characters)
-- Registers which pool characters are ambient world bots.
-- OnPlayerLogin checks this table to distinguish ambient from hostile bots.

CREATE TABLE IF NOT EXISTS living_world_ambient_bot (
    id               INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_account_id   INT UNSIGNED      NOT NULL,
    character_guid   BIGINT UNSIGNED   NOT NULL,
    faction          TINYINT UNSIGNED  NOT NULL,  -- 1=Alliance 2=Horde
    class_id         TINYINT UNSIGNED  NOT NULL,
    level            TINYINT UNSIGNED  NOT NULL,
    has_herbalism    TINYINT(1)        NOT NULL DEFAULT 0,
    has_mining       TINYINT(1)        NOT NULL DEFAULT 0,
    has_fishing      TINYINT(1)        NOT NULL DEFAULT 0,
    is_available     TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    last_activity_at DATETIME          NULL,
    UNIQUE KEY uq_char (character_guid),
    KEY idx_available (is_available, faction, level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Pool characters designated as ambient world bots';
