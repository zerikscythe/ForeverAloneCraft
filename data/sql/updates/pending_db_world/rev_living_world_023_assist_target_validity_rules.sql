-- rev_living_world_023_assist_target_validity_rules (world DB)
--
-- Exposes assist/guard versus command/attack-lock target-validity strictness
-- through living_world_bot_global_config so operators can tune whether bots
-- require the live attackable-for-attack gate before accepting a target.

CREATE TABLE IF NOT EXISTS living_world_bot_global_config (
    config_key   VARCHAR(64) NOT NULL,
    config_value FLOAT       NOT NULL,
    notes        VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (config_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES
    ('assist_require_targetable_for_attack', 1.0, 'Normal assist and guard: require candidate to currently pass attackable-for-attack checks'),
    ('command_require_targetable_for_attack', 0.0, 'Forced-target and attack-lock assist: require candidate to currently pass attackable-for-attack checks instead of allowing pull/setup flicker');