-- rev_living_world_008_reserve_mode_and_column_rename  (characters DB)
--
-- Mirrors the world-DB migration for living_world_bot_combat_profile
-- (per-player profile rows stored in the characters database).
--
-- Changes:
--   1. mana_low_water  → resource_low_water
--      mana_high_water → resource_high_water
--
--   2. conservation_mode re-encoding:
--        OLD: 0=FullForce  1=Conservative  2=JitCasting
--        NEW: 0=FullForce  1=Reserve       2=Conservative  3=JitCasting
--
-- Idempotent: column rename is guarded by an information_schema check.

-- ── 1. Rename columns ─────────────────────────────────────────────────────────

SET @tbl = 'living_world_bot_combat_profile';

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
    'SELECT 1');
SET @rename_high = IF(@col_high > 0,
    CONCAT('ALTER TABLE `', @tbl, '` CHANGE `mana_high_water` `resource_high_water` TINYINT UNSIGNED NOT NULL DEFAULT 75'),
    'SELECT 1');

PREPARE stmt FROM @rename_low;  EXECUTE stmt; DEALLOCATE PREPARE stmt;
PREPARE stmt FROM @rename_high; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- ── 2. Re-encode conservation_mode values ────────────────────────────────────

UPDATE `living_world_bot_combat_profile`
SET    `conservation_mode` = 3
WHERE  `conservation_mode` = 2; -- old JitCasting → new JitCasting

UPDATE `living_world_bot_combat_profile`
SET    `conservation_mode` = 2
WHERE  `conservation_mode` = 1; -- old Conservative → new Conservative