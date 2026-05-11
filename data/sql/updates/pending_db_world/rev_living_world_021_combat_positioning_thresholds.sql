-- rev_living_world_021_combat_positioning_thresholds (world DB)
--
-- Exposes ranged/healer combat positioning thresholds through
-- living_world_bot_global_config so operators can tune the movement/decision
-- policy without recompiling.

CREATE TABLE IF NOT EXISTS living_world_bot_global_config (
    config_key   VARCHAR(64) NOT NULL,
    config_value FLOAT       NOT NULL,
    notes        VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (config_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES
    ('combat_follow_override_distance', 20.0, 'If ranged/healer bots drift farther than this from owner, snap back to follow behaviour'),
    ('reposition_distance',             8.0, 'Passive-mode catch-up distance before reissuing follow'),
    ('ranged_min_distance',             8.0, 'Back away when a ranged/healer bot is closer than this to its target'),
    ('ranged_optimal_distance',        25.0, 'Preferred chase stop distance for ranged/healer combat positioning'),
    ('ranged_cast_range',              30.0, 'Approach target when farther than this spell-usage range'),
    ('ranged_retreat_distance',         5.0, 'Short backstep distance when retreating from melee range'),
    ('ranged_retreat_trigger_pct',     80.0, 'Retreat when ranged bot HP drops below this percent in melee range'),
    ('ranged_retreat_reset_pct',       60.0, 'Allow another retreat only after HP drops below this percent again');