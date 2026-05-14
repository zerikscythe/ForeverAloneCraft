-- rev_living_world_030_profile_template_variants (world DB)
--
-- Adds optional variant metadata so multiple loadouts can coexist under the same
-- canonical spec/role/class/context combination.

SET @has_profile_variant_key := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_combat_default_profile'
      AND COLUMN_NAME = 'variant_key'
);
SET @sql := IF(@has_profile_variant_key = 0,
    'ALTER TABLE living_world_bot_combat_default_profile ADD COLUMN variant_key VARCHAR(64) NOT NULL DEFAULT ''''',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_profile_description := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_combat_default_profile'
      AND COLUMN_NAME = 'description'
);
SET @sql := IF(@has_profile_description = 0,
    'ALTER TABLE living_world_bot_combat_default_profile ADD COLUMN description VARCHAR(255) NULL',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_profile_variant_index := (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name = 'living_world_bot_combat_default_profile'
      AND index_name = 'idx_lwbdp_variant_lookup'
);
SET @sql := IF(@has_profile_variant_index = 0,
    'ALTER TABLE living_world_bot_combat_default_profile ADD INDEX idx_lwbdp_variant_lookup (spec_key, role_key, variant_key)',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_template_variant_key := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_talent_template'
      AND COLUMN_NAME = 'variant_key'
);
SET @sql := IF(@has_template_variant_key = 0,
    'ALTER TABLE living_world_bot_talent_template ADD COLUMN variant_key VARCHAR(64) NOT NULL DEFAULT ''''',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_template_description := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'living_world_bot_talent_template'
      AND COLUMN_NAME = 'description'
);
SET @sql := IF(@has_template_description = 0,
    'ALTER TABLE living_world_bot_talent_template ADD COLUMN description VARCHAR(255) NULL',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_old_template_unique := (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name = 'living_world_bot_talent_template'
      AND index_name = 'uk_lwbt_template_spec_class'
);
SET @sql := IF(@has_old_template_unique > 0,
    'ALTER TABLE living_world_bot_talent_template DROP INDEX uk_lwbt_template_spec_class',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_new_template_unique := (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name = 'living_world_bot_talent_template'
      AND index_name = 'uk_lwbt_template_spec_class_variant'
);
SET @sql := IF(@has_new_template_unique = 0,
    'ALTER TABLE living_world_bot_talent_template ADD UNIQUE INDEX uk_lwbt_template_spec_class_variant (spec_key, class_id, variant_key)',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;