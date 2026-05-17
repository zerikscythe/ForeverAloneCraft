-- Add editable targeting policy knobs to per-character combat profiles.
SET @col := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'living_world_bot_combat_profile'
      AND column_name = 'targeting_mode'
);
SET @sql := IF(
    @col = 0,
    'ALTER TABLE living_world_bot_combat_profile
        ADD COLUMN targeting_mode TINYINT NOT NULL DEFAULT 0 COMMENT ''0=standard 1=assist 2=skirmish'',
        ADD COLUMN current_target_bias FLOAT NOT NULL DEFAULT 80,
        ADD COLUMN assist_target_bias FLOAT NOT NULL DEFAULT 140,
        ADD COLUMN focus_fire_bias FLOAT NOT NULL DEFAULT 55,
        ADD COLUMN protect_ally_bias FLOAT NOT NULL DEFAULT 170,
        ADD COLUMN prefer_healer_bias FLOAT NOT NULL DEFAULT 220,
        ADD COLUMN prefer_dps_bias FLOAT NOT NULL DEFAULT 140,
        ADD COLUMN avoid_tank_bias FLOAT NOT NULL DEFAULT 120',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
