-- living_world_bot_activity_log (acore_characters)
-- Audit trail written by BotActivityLog::Record for every significant
-- bot lifecycle event. Used by the headless smoke-test report script.

CREATE TABLE IF NOT EXISTS living_world_bot_activity_log (
    id         BIGINT UNSIGNED   NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_guid   BIGINT UNSIGNED   NOT NULL,
    bot_name   VARCHAR(24)       NOT NULL,
    event_type VARCHAR(32)       NOT NULL,  -- spawned|ai_assigned|travel_start|travel_arrive|activity_start|activity_tick|despawn|idle
    detail     VARCHAR(255)      NULL,
    map_id     SMALLINT UNSIGNED NULL,
    zone_id    INT UNSIGNED      NULL,
    pos_x      FLOAT             NULL,
    pos_y      FLOAT             NULL,
    pos_z      FLOAT             NULL,
    logged_at  DATETIME(3)       NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    KEY idx_bot  (bot_guid),
    KEY idx_time (logged_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Headless test audit trail for bot lifecycle events';
