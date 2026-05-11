-- rev_living_world_014_activity_selection_bias (world DB)
--
-- Moves session-shaping preferences into DB-authored activity metadata so the
-- opener/follow-up mix can be tuned without recompiling the server.

SET @tbl = 'living_world_activity_library';

SET @has_opener_bias = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'opener_bias');
SET @sql = IF(@has_opener_bias = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `opener_bias` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `max_per_session`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_followup_bias = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'followup_bias');
SET @sql = IF(@has_followup_bias = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `followup_bias` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `opener_bias`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Baseline: keep most tasks neutral unless explicitly tuned below.
UPDATE living_world_activity_library
SET opener_bias = 1,
    followup_bias = 1;

-- City errands are good fallback/return anchors, but should not dominate the
-- first task selection when outdoor work is available.
UPDATE living_world_activity_library
SET opener_bias = 0,
    followup_bias = 3
WHERE task_family = 'city_errand';

-- Patrol/gathering/fishing are stronger openers for visible world activity.
UPDATE living_world_activity_library
SET opener_bias = 4,
    followup_bias = 2
WHERE task_family IN ('patrol', 'gathering', 'fishing');

-- Dalaran/Shattrath city idles are still valid high-level opener fallbacks, but
-- should remain less preferred than real outdoor work.
UPDATE living_world_activity_library
SET opener_bias = 1,
    followup_bias = 2
WHERE activity_key IN ('idle_dalaran', 'idle_shattrath', 'idle_inn_shattrath');