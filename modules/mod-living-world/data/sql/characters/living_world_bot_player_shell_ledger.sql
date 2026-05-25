-- living_world_bot_player_shell_ledger (acore_characters)
--
-- Sidecar tables for player-shell materialization. The identity ledger remains
-- canonical; these tables store shell-facing visual state, small runtime
-- snapshots, active shell leases, and rebuild audit history.

CREATE TABLE IF NOT EXISTS living_world_bot_display_loadout (
    identity_id         INT UNSIGNED    NOT NULL PRIMARY KEY,
    display_loadout_key VARCHAR(64)     NOT NULL DEFAULT '',
    helm_item_id        INT UNSIGNED    NOT NULL DEFAULT 0,
    shoulder_item_id    INT UNSIGNED    NOT NULL DEFAULT 0,
    shirt_item_id       INT UNSIGNED    NOT NULL DEFAULT 0,
    chest_item_id       INT UNSIGNED    NOT NULL DEFAULT 0,
    waist_item_id       INT UNSIGNED    NOT NULL DEFAULT 0,
    legs_item_id        INT UNSIGNED    NOT NULL DEFAULT 0,
    feet_item_id        INT UNSIGNED    NOT NULL DEFAULT 0,
    wrist_item_id       INT UNSIGNED    NOT NULL DEFAULT 0,
    hands_item_id       INT UNSIGNED    NOT NULL DEFAULT 0,
    back_item_id        INT UNSIGNED    NOT NULL DEFAULT 0,
    tabard_item_id      INT UNSIGNED    NOT NULL DEFAULT 0,
    mainhand_item_id    INT UNSIGNED    NOT NULL DEFAULT 0,
    offhand_item_id     INT UNSIGNED    NOT NULL DEFAULT 0,
    ranged_item_id      INT UNSIGNED    NOT NULL DEFAULT 0,
    hide_helm           TINYINT(1)      NOT NULL DEFAULT 0,
    hide_cloak          TINYINT(1)      NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Canonical visual equipment package for rebuilding player shells';

CREATE TABLE IF NOT EXISTS living_world_bot_runtime_snapshot (
    identity_id              INT UNSIGNED    NOT NULL PRIMARY KEY,
    map_id                   SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    zone_id                  INT UNSIGNED    NOT NULL DEFAULT 0,
    pos_x                    FLOAT           NOT NULL DEFAULT 0,
    pos_y                    FLOAT           NOT NULL DEFAULT 0,
    pos_z                    FLOAT           NOT NULL DEFAULT 0,
    orientation              FLOAT           NOT NULL DEFAULT 0,
    runtime_state            VARCHAR(64)     NOT NULL DEFAULT '',
    runtime_detail           VARCHAR(255)    NOT NULL DEFAULT '',
    last_task_family         VARCHAR(64)     NOT NULL DEFAULT '',
    last_task_activity_key   VARCHAR(96)     NOT NULL DEFAULT '',
    last_task_target_zone_id INT UNSIGNED    NOT NULL DEFAULT 0,
    home_bind_point_key      VARCHAR(64)     NOT NULL DEFAULT '',
    generic_potion_charges   TINYINT UNSIGNED NOT NULL DEFAULT 0,
    updated_at               DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP
                                                     ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Small resumable runtime state for abstract bots and player shells';

CREATE TABLE IF NOT EXISTS living_world_bot_shell_runtime (
    identity_id           INT UNSIGNED      NOT NULL PRIMARY KEY,
    shell_account_id      INT UNSIGNED      NOT NULL,
    shell_character_guid  BIGINT UNSIGNED   NOT NULL,
    is_materialized       TINYINT(1)        NOT NULL DEFAULT 0,
    shell_state_version   INT UNSIGNED      NOT NULL DEFAULT 0,
    leased_at             DATETIME          NULL,
    last_sync_at          DATETIME          NULL,
    last_dismissed_at     DATETIME          NULL,
    UNIQUE KEY uq_shell (shell_account_id, shell_character_guid),
    KEY idx_materialized (is_materialized, leased_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Current player-shell lease mapping for one abstract bot identity';

CREATE TABLE IF NOT EXISTS living_world_bot_rebuild_log (
    id                    BIGINT UNSIGNED   NOT NULL AUTO_INCREMENT PRIMARY KEY,
    identity_id           INT UNSIGNED      NOT NULL,
    shell_account_id      INT UNSIGNED      NULL,
    shell_character_guid  BIGINT UNSIGNED   NULL,
    rebuild_reason        VARCHAR(64)       NOT NULL DEFAULT '',
    level                 TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    gear_tier             TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    spec_key              VARCHAR(32)       NOT NULL DEFAULT '',
    loadout_key           VARCHAR(64)       NOT NULL DEFAULT '',
    display_loadout_key   VARCHAR(64)       NOT NULL DEFAULT '',
    doctrine_profile_key  VARCHAR(64)       NOT NULL DEFAULT '',
    shell_state_version   INT UNSIGNED      NOT NULL DEFAULT 0,
    notes                 VARCHAR(255)      NOT NULL DEFAULT '',
    rebuilt_at            DATETIME          NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_identity_time (identity_id, rebuilt_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Append-only audit log for player-shell rebuilds';
