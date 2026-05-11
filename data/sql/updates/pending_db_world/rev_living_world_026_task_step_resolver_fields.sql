-- rev_living_world_026_task_step_resolver_fields (world DB)
--
-- Extends task-template steps with generic intent resolver metadata while
-- preserving backward compatibility for existing point/zone-authored tasks.

SET @tbl = 'living_world_task_template_step';

SET @has_resolver_kind = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'resolver_kind');
SET @sql = IF(@has_resolver_kind = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `resolver_kind` VARCHAR(32) NOT NULL DEFAULT ''zone'' AFTER `target_point_key`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_subject_kind = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'subject_kind');
SET @sql = IF(@has_subject_kind = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `subject_kind` VARCHAR(32) NULL AFTER `resolver_kind`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_subject_id = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'subject_id');
SET @sql = IF(@has_subject_id = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `subject_id` INT UNSIGNED NULL AFTER `subject_kind`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_subject_key = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'subject_key');
SET @sql = IF(@has_subject_key = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `subject_key` VARCHAR(64) NULL AFTER `subject_id`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_return_anchor_role = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'return_anchor_role');
SET @sql = IF(@has_return_anchor_role = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `return_anchor_role` VARCHAR(32) NULL AFTER `subject_key`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_cycle_count = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'cycle_count');
SET @sql = IF(@has_cycle_count = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `cycle_count` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `return_anchor_role`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

UPDATE living_world_task_template_step
SET resolver_kind = CASE
        WHEN target_point_key IS NOT NULL AND target_point_key <> '' THEN 'point'
        ELSE 'zone'
    END;

UPDATE living_world_task_template_step
SET subject_kind = CASE
        WHEN step_type = 'gather_herb' THEN 'herb'
        WHEN step_type = 'gather_ore'  THEN 'ore'
        WHEN step_type = 'fish'        THEN 'fish'
        WHEN step_type IN ('idle_city', 'idle_inn') THEN 'city_service'
        ELSE subject_kind
    END
WHERE subject_kind IS NULL OR subject_kind = '';

UPDATE living_world_task_template_step
SET cycle_count = 1
WHERE cycle_count = 0;