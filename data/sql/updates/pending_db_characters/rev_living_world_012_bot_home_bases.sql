-- rev_living_world_012_bot_home_bases (characters DB)
--
-- Adds persistent home-base metadata for creature-based world bots so broad
-- authored tasks like `City Chores <Home>` can resolve to a stable major city
-- or late-game hub.

SET @tbl = 'living_world_bot_identity';

SET @has_home_zone_id = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'home_zone_id');
SET @sql = IF(@has_home_zone_id = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `home_zone_id` INT UNSIGNED NULL AFTER `has_fishing`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_home_anchor_point_key = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'home_anchor_point_key');
SET @sql = IF(@has_home_anchor_point_key = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `home_anchor_point_key` VARCHAR(64) NULL AFTER `home_zone_id`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_home_bind_point_key = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'home_bind_point_key');
SET @sql = IF(@has_home_bind_point_key = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `home_bind_point_key` VARCHAR(64) NULL AFTER `home_anchor_point_key`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

UPDATE living_world_bot_identity
SET home_zone_id = CASE
        WHEN level >= 68 THEN 4395  -- Dalaran
        WHEN level >= 58 THEN 3703  -- Shattrath City
        WHEN faction = 1 THEN 1519  -- Stormwind City
        WHEN faction = 2 THEN 1637  -- Orgrimmar
        ELSE home_zone_id
    END,
    home_anchor_point_key = CASE
        WHEN level >= 68 THEN 'dalaran_inn'
        WHEN level >= 58 THEN 'shattrath_inn'
        WHEN faction = 1 THEN 'stormwind_inn'
        WHEN faction = 2 THEN 'orgrimmar_inn'
        ELSE home_anchor_point_key
    END,
    home_bind_point_key = CASE
        WHEN level >= 68 THEN 'dalaran_inn'
        WHEN level >= 58 THEN 'shattrath_inn'
        WHEN faction = 1 THEN 'stormwind_inn'
        WHEN faction = 2 THEN 'orgrimmar_inn'
        ELSE home_bind_point_key
    END
WHERE home_zone_id IS NULL
   OR home_anchor_point_key IS NULL
   OR home_bind_point_key IS NULL;