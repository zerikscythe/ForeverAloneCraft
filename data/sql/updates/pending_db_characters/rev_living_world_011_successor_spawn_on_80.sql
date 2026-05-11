-- rev_living_world_011_successor_spawn_on_80 (characters DB)
--
-- Tracks whether a world-bot identity has already spawned its one-time
-- successor on first reaching level 80.
--
-- Existing identities that are already at max level are grandfathered as having
-- already consumed this one-time transition, since retroactively creating
-- successor rows in SQL would diverge from the runtime's C++ naming/spec logic.

SET @tbl = 'living_world_bot_identity';

SET @has_successor_spawned = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'successor_spawned');

SET @sql = IF(@has_successor_spawned = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `successor_spawned` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `is_retired`'),
    'SELECT 1');

PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

UPDATE living_world_bot_identity
SET successor_spawned = 1
WHERE level >= 80
  AND successor_spawned = 0;