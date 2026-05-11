-- rev_living_world_009_world_bot_ledger_progression  (characters DB)
--
-- Extends the persistent world-bot ledger with online-time and retirement
-- tracking so recurring creature bots can age, level, and eventually retire.

SET @tbl = 'living_world_bot_identity';

SET @has_total_world_online_ms = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'total_world_online_ms');
SET @sql = IF(@has_total_world_online_ms = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `total_world_online_ms` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `session_count`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_world_online_ms_since_level = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'world_online_ms_since_level');
SET @sql = IF(@has_world_online_ms_since_level = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `world_online_ms_since_level` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `total_world_online_ms`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_post_max_world_online_ms = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'post_max_world_online_ms');
SET @sql = IF(@has_post_max_world_online_ms = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `post_max_world_online_ms` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `world_online_ms_since_level`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_active_world_session_ms = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'active_world_session_ms');
SET @sql = IF(@has_active_world_session_ms = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `active_world_session_ms` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `post_max_world_online_ms`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_active_world_session_start = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'active_world_session_start');
SET @sql = IF(@has_active_world_session_start = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `active_world_session_start` DATETIME NULL AFTER `active_world_session_ms`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_is_retired = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'is_retired');
SET @sql = IF(@has_is_retired = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `is_retired` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `active_world_session_start`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_retired_at = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'retired_at');
SET @sql = IF(@has_retired_at = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `retired_at` DATETIME NULL AFTER `is_retired`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_idx_active = (
    SELECT COUNT(*) FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND INDEX_NAME   = 'idx_active');
SET @sql = IF(@has_idx_active = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD KEY `idx_active` (`is_available`, `is_retired`)'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_idx_retired = (
    SELECT COUNT(*) FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND INDEX_NAME   = 'idx_retired');
SET @sql = IF(@has_idx_retired = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD KEY `idx_retired` (`is_retired`, `retired_at`)'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
