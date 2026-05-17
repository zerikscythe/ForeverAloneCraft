-- living_world_bot_identity (acore_characters)
--
-- Persistent identity ledger for creature-based world bots.
-- Each row is a named individual who may appear in the world more than once.
-- Creatures are spawned from this table instead of an anonymous template pool,
-- giving the server population a sense of recurring faces.
--
-- The bot is a Creature, not a Player session. No account, no character guid.
-- Identity fields drive the creature's display model, name, combat profile, and
-- activity eligibility. Session tracking fields let the system (and future GM
-- tools) show "last seen in Stranglethorn, 3 sessions total."

CREATE TABLE IF NOT EXISTS living_world_bot_identity (
    id              INT UNSIGNED     NOT NULL AUTO_INCREMENT PRIMARY KEY,

    -- Who they are
    name            VARCHAR(32)      NOT NULL,           -- display name shown in world
    race_id         TINYINT UNSIGNED NOT NULL,           -- WoW race ID (1=Human, 2=Orc, etc.)
    class_id        TINYINT UNSIGNED NOT NULL,           -- WoW class ID (1=Warrior, 4=Rogue, etc.)
    spec_key        VARCHAR(32)      NOT NULL,           -- e.g. 'Arms', 'Frost', 'Retribution'
    loadout_key     VARCHAR(64)      NOT NULL DEFAULT '', -- variant/loadout key, e.g. 'Druid_Feral_PVE_01'
    faction         TINYINT UNSIGNED NOT NULL,           -- 1=Alliance  2=Horde
    display_id      INT UNSIGNED     NOT NULL,           -- creature_template displayid for visual
    gender          TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 0=male 1=female

    -- Capability
    level           TINYINT UNSIGNED NOT NULL,
    gear_tier       TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 1=questing 2=dungeon 3=raid
    personality_key VARCHAR(32)      NOT NULL DEFAULT 'uninterested', -- uninterested|opportunistic|aggressive|coward
    has_herbalism   TINYINT(1)       NOT NULL DEFAULT 0,
    has_mining      TINYINT(1)       NOT NULL DEFAULT 0,
    has_fishing     TINYINT(1)       NOT NULL DEFAULT 0,
    population_role VARCHAR(32)      NOT NULL DEFAULT 'world', -- world|city_reserve
    reserve_city_zone_id INT UNSIGNED NULL,                    -- reserved city pool owner when population_role='city_reserve'
    ambient_group_id INT UNSIGNED NULL,                        -- optional ambient travel/combat group identifier
    ambient_group_leader_identity_id INT UNSIGNED NULL,        -- leader ledger id for this ambient group
    ambient_group_role VARCHAR(32) NOT NULL DEFAULT '',        -- tank|healer|melee_dps|ranged_dps|support

    -- Home-base / return routing
    home_zone_id          INT UNSIGNED     NULL,
    home_anchor_point_key VARCHAR(64)      NULL,
    home_bind_point_key   VARCHAR(64)      NULL,

    -- Session state
    is_available    TINYINT UNSIGNED NOT NULL DEFAULT 1, -- 1=ready to spawn  0=currently active
    session_count   INT UNSIGNED     NOT NULL DEFAULT 0, -- total times spawned
    total_world_online_ms       BIGINT UNSIGNED NOT NULL DEFAULT 0,
    world_online_ms_since_level BIGINT UNSIGNED NOT NULL DEFAULT 0,
    post_max_world_online_ms    BIGINT UNSIGNED NOT NULL DEFAULT 0,
    active_world_session_ms     BIGINT UNSIGNED NOT NULL DEFAULT 0,
    runtime_state               VARCHAR(64)     NOT NULL DEFAULT '',
    runtime_detail              VARCHAR(255)    NOT NULL DEFAULT '',
    active_world_session_start  DATETIME        NULL,
    is_retired     TINYINT UNSIGNED NOT NULL DEFAULT 0,
    successor_spawned TINYINT UNSIGNED NOT NULL DEFAULT 0,
    retired_at     DATETIME         NULL,
    last_seen_zone  INT UNSIGNED     NULL,               -- zone_id of last activity
    last_seen_at    DATETIME         NULL,               -- wall time of last despawn
    created_at      DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE KEY uq_name   (name),
    KEY idx_spawn        (faction, class_id, level, is_available),
    KEY idx_population_role (population_role, reserve_city_zone_id, faction, is_available, is_retired),
    KEY idx_ambient_group (ambient_group_id, ambient_group_leader_identity_id, is_available, is_retired),
    KEY idx_last_seen    (last_seen_at),
    KEY idx_active       (is_available, is_retired),
    KEY idx_retired      (is_retired, retired_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Persistent identity ledger for creature-based world bots';
