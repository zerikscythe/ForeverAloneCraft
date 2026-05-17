-- rev_living_world_016_ambient_grouping (characters DB)
--
-- Adds optional ambient group metadata so world-bot parties can be authored
-- as one leader-led travel/combat bundle instead of three independent solos.

SET @has_ambient_group_id := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_identity'
      AND COLUMN_NAME = 'ambient_group_id'
);
SET @sql := IF(@has_ambient_group_id = 0,
    'ALTER TABLE living_world_bot_identity ADD COLUMN ambient_group_id INT UNSIGNED NULL AFTER reserve_city_zone_id',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_ambient_group_leader_identity_id := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_identity'
      AND COLUMN_NAME = 'ambient_group_leader_identity_id'
);
SET @sql := IF(@has_ambient_group_leader_identity_id = 0,
    'ALTER TABLE living_world_bot_identity ADD COLUMN ambient_group_leader_identity_id INT UNSIGNED NULL AFTER ambient_group_id',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_ambient_group_role := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_identity'
      AND COLUMN_NAME = 'ambient_group_role'
);
SET @sql := IF(@has_ambient_group_role = 0,
    'ALTER TABLE living_world_bot_identity ADD COLUMN ambient_group_role VARCHAR(32) NOT NULL DEFAULT '''' AFTER ambient_group_leader_identity_id',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_ambient_group_index := (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name = 'living_world_bot_identity'
      AND index_name = 'idx_ambient_group'
);
SET @sql := IF(@has_ambient_group_index = 0,
    'ALTER TABLE living_world_bot_identity ADD INDEX idx_ambient_group (ambient_group_id, ambient_group_leader_identity_id, is_available, is_retired)',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
