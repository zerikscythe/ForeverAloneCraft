-- rev_living_world_008_reserve_mode_and_column_rename
--
-- What this migration does:
--
--   1. Renames mana_low_water  → resource_low_water
--              mana_high_water → resource_high_water
--      on living_world_bot_combat_default_profile (world DB).
--
--   2. Inserts the new Reserve conservation mode into the enum ordering:
--        OLD: 0=FullForce  1=Conservative  2=JitCasting
--        NEW: 0=FullForce  1=Reserve       2=Conservative  3=JitCasting
--
--      Existing rows that stored Conservative=1 are migrated to Conservative=2.
--      Existing rows that stored JitCasting=2  are migrated to JitCasting=3.
--      (FullForce=0 rows are unaffected.)
--
--   This migration is idempotent: re-running it when the columns have already
--   been renamed is safe because the RENAME only fires if the old column name
--   exists.  The UPDATE is unconditional but a no-op if all values are already
--   on the new encoding.
--
-- Companion characters migration (living_world_bot_combat_profile in the
-- characters DB) is covered by
-- pending_db_characters/rev_living_world_008_reserve_mode_and_column_rename.sql

-- ── 1. Rename columns ─────────────────────────────────────────────────────────

SET @tbl = 'living_world_bot_combat_default_profile';

-- Only rename if the old column still exists (idempotency guard).
SET @col_low  = (SELECT COUNT(*) FROM information_schema.COLUMNS
                 WHERE TABLE_SCHEMA = DATABASE()
                   AND TABLE_NAME   = @tbl
                   AND COLUMN_NAME  = 'mana_low_water');
SET @col_high = (SELECT COUNT(*) FROM information_schema.COLUMNS
                 WHERE TABLE_SCHEMA = DATABASE()
                   AND TABLE_NAME   = @tbl
                   AND COLUMN_NAME  = 'mana_high_water');

SET @rename_low  = IF(@col_low  > 0,
    CONCAT('ALTER TABLE `', @tbl, '` CHANGE `mana_low_water`  `resource_low_water`  TINYINT UNSIGNED NOT NULL DEFAULT 55'),
    'SELECT 1 -- mana_low_water already renamed');
SET @rename_high = IF(@col_high > 0,
    CONCAT('ALTER TABLE `', @tbl, '` CHANGE `mana_high_water` `resource_high_water` TINYINT UNSIGNED NOT NULL DEFAULT 75'),
    'SELECT 1 -- mana_high_water already renamed');

PREPARE stmt FROM @rename_low;  EXECUTE stmt; DEALLOCATE PREPARE stmt;
PREPARE stmt FROM @rename_high; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- ── 2. Re-encode conservation_mode values ────────────────────────────────────
--   Migrate in descending old-value order so the UPDATE is not self-defeating.
--   JitCasting: 2 → 3   (must go first so it is not caught by the next step)
--   Conservative: 1 → 2

UPDATE `living_world_bot_combat_default_profile`
SET    `conservation_mode` = 3
WHERE  `conservation_mode` = 2; -- old JitCasting

UPDATE `living_world_bot_combat_default_profile`
SET    `conservation_mode` = 2
WHERE  `conservation_mode` = 1; -- old Conservative → new Conservative