-- rev_living_world_015_world_bot_loadout_key (characters DB)
--
-- Adds an optional world-bot loadout key so identities can point at a specific
-- profile/template variant while keeping spec_key canonical for core logic.

SET @has_loadout_key := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_identity'
      AND COLUMN_NAME = 'loadout_key'
);
SET @sql := IF(@has_loadout_key = 0,
    'ALTER TABLE living_world_bot_identity ADD COLUMN loadout_key VARCHAR(64) NOT NULL DEFAULT '''' AFTER spec_key',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_loadout_index := (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name = 'living_world_bot_identity'
      AND index_name = 'idx_lwbi_loadout_key'
);
SET @sql := IF(@has_loadout_index = 0,
    'ALTER TABLE living_world_bot_identity ADD INDEX idx_lwbi_loadout_key (loadout_key)',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;