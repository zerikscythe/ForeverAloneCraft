-- rev_living_world_053_profile_variant_uniqueness (world DB)
--
-- Allow multiple doctrine variants for the same class/spec/context by making
-- variant_key part of the uniqueness rule. This lets us keep side-by-side
-- profiles like:
--   Frost Mage DPS / PvE / ''                -> normal live profile
--   Frost Mage DPS / PvE / 'test_frost_pet'  -> harness-only test profile
--
-- The default profile selector already understands variant fallback order in
-- code; the schema just needs to stop collapsing these rows together.

UPDATE `living_world_bot_combat_default_profile`
SET
    `class_key` = COALESCE(`class_key`, ''),
    `context_key` = COALESCE(`context_key`, ''),
    `variant_key` = COALESCE(`variant_key`, '');

SET @has_old_unique := (
    SELECT COUNT(*)
    FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name   = 'living_world_bot_combat_default_profile'
      AND index_name   = 'uq_class_spec_context'
);
SET @sql := IF(
    @has_old_unique > 0,
    'ALTER TABLE `living_world_bot_combat_default_profile` DROP INDEX `uq_class_spec_context`',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_new_unique := (
    SELECT COUNT(*)
    FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name   = 'living_world_bot_combat_default_profile'
      AND index_name   = 'uq_class_spec_role_context_variant'
);
SET @sql := IF(
    @has_new_unique = 0,
    'ALTER TABLE `living_world_bot_combat_default_profile` ADD UNIQUE INDEX `uq_class_spec_role_context_variant` (`class_key`, `spec_key`, `role_key`, `context_key`, `variant_key`)',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
