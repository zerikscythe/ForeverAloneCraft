-- rev_living_world_022_assist_guard_targeting_rules (world DB)
--
-- Exposes assist / attack-lock / guard target-source decisions through
-- living_world_bot_global_config so operators can tune companion target
-- selection policy without recompiling.

CREATE TABLE IF NOT EXISTS living_world_bot_global_config (
    config_key   VARCHAR(64) NOT NULL,
    config_value FLOAT       NOT NULL,
    notes        VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (config_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES
    ('assist_use_current_victim', 1.0, 'Normal assist: keep fighting bot current victim if still valid'),
    ('assist_use_owner_victim',   1.0, 'Normal assist: consider owner current victim as follow-up target'),
    ('assist_owner_victim_must_target_owner', 1.0, 'Require owner victim to be actively fighting back against owner before assist picks it'),
    ('attack_lock_use_owner_victim', 1.0, 'During attack-lock, consider owner current victim if current bot victim is unavailable'),
    ('attack_lock_use_owner_selection', 1.0, 'During attack-lock, consider owner selected target if other sources are unavailable'),
    ('guard_use_current_victim', 1.0, 'Guard mode: keep bot current victim if still valid'),
    ('guard_use_owner_attackers', 1.0, 'Guard mode: consider units actively attacking the owner');