-- rev_living_world_067_generic_potion_charges (characters DB)
--
-- Add a small fake generic potion stock to the world-bot identity ledger.
-- This stock is consumed by simulated self-heal / self-mana potion use and is
-- only replenished by authored city mailbox / auction-house errand routines.

SET @tbl = 'living_world_bot_identity';
SET @col = 'generic_potion_charges';

SET @sql = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = @tbl
      AND COLUMN_NAME = @col
  ),
  'SELECT 1',
  'ALTER TABLE living_world_bot_identity ADD COLUMN generic_potion_charges TINYINT UNSIGNED NOT NULL DEFAULT 5 AFTER home_bind_point_key'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

UPDATE living_world_bot_identity
SET generic_potion_charges = 5
WHERE generic_potion_charges IS NULL OR generic_potion_charges > 5;
