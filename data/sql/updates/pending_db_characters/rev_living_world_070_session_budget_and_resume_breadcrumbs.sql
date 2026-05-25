-- rev_living_world_070_session_budget_and_resume_breadcrumbs (characters DB)
--
-- Add per-activation session budget and quest/task resume breadcrumbs to the
-- world-bot identity ledger so abstract/materialized bots can continue a
-- multi-hour workday and resume the right quest pocket later.

SET @tbl = 'living_world_bot_identity';

SET @col = 'active_world_session_budget_ms';
SET @sql = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = @tbl
      AND COLUMN_NAME = @col
  ),
  'SELECT 1',
  'ALTER TABLE living_world_bot_identity ADD COLUMN active_world_session_budget_ms BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER active_world_session_ms'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @col = 'last_task_activity_key';
SET @sql = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = @tbl
      AND COLUMN_NAME = @col
  ),
  'SELECT 1',
  'ALTER TABLE living_world_bot_identity ADD COLUMN last_task_activity_key VARCHAR(128) NOT NULL DEFAULT '''' AFTER last_task_family'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @col = 'last_quest_hub_key';
SET @sql = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = @tbl
      AND COLUMN_NAME = @col
  ),
  'SELECT 1',
  'ALTER TABLE living_world_bot_identity ADD COLUMN last_quest_hub_key VARCHAR(128) NOT NULL DEFAULT '''' AFTER last_task_target_zone'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @col = 'last_quest_hub_elapsed_ms';
SET @sql = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = @tbl
      AND COLUMN_NAME = @col
  ),
  'SELECT 1',
  'ALTER TABLE living_world_bot_identity ADD COLUMN last_quest_hub_elapsed_ms BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER last_quest_hub_key'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

UPDATE living_world_bot_identity
SET active_world_session_budget_ms = 0
WHERE active_world_session_budget_ms IS NULL;

UPDATE living_world_bot_identity
SET last_task_activity_key = ''
WHERE last_task_activity_key IS NULL;

UPDATE living_world_bot_identity
SET last_quest_hub_key = ''
WHERE last_quest_hub_key IS NULL;

UPDATE living_world_bot_identity
SET last_quest_hub_elapsed_ms = 0
WHERE last_quest_hub_elapsed_ms IS NULL;
