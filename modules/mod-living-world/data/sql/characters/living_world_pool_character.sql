-- living_world_pool_character
--
-- Server-managed pool of pre-built, pre-geared characters available to any
-- player via `.lwbot raid request <class> <spec_role> <level> <min_ilvl>`.
-- These bots have no player-account ownership and never appear in `.lwbot roster`.
--
-- bot_account_id : the pool account in living_world_bot_account_pool that
--                  owns this character.
-- character_guid : guid from the `characters` table.
-- class_id       : WoW class numeric ID (1=warrior … 11=druid; 10 unused).
-- spec_role      : 'tank', 'healer', or 'dps'.
-- spec_key       : specific build label, e.g. 'warrior_prot', 'mage_fire'.
-- level          : character level.
-- avg_ilvl       : average equipped item level used for pool queries.
-- faction        : 1=Alliance, 2=Horde.
-- is_available   : 1 = free; 0 = currently spawned. Reset by OnPlayerLogout.

CREATE TABLE IF NOT EXISTS living_world_pool_character (
    id             INT UNSIGNED     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_account_id INT UNSIGNED     NOT NULL,
    character_guid BIGINT UNSIGNED  NOT NULL,
    class_id       TINYINT UNSIGNED NOT NULL,
    spec_role      VARCHAR(16)      NOT NULL,
    spec_key       VARCHAR(32)      NOT NULL,
    level          TINYINT UNSIGNED NOT NULL,
    avg_ilvl       SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    faction        TINYINT UNSIGNED NOT NULL,
    is_available   TINYINT UNSIGNED NOT NULL DEFAULT 1,
    UNIQUE KEY uq_char (character_guid),
    KEY idx_role   (class_id, spec_role, level, avg_ilvl, faction, is_available)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Server-pool raid bots spawnable via .lwbot raid request';

-- Example rows (replace with real account/character values):
-- INSERT INTO living_world_pool_character
--     (bot_account_id, character_guid, class_id, spec_role, spec_key, level, avg_ilvl, faction)
-- VALUES
--     (2, <guid>, 1, 'tank',   'warrior_prot', 80, 213, 2),
--     (3, <guid>, 5, 'healer', 'priest_holy',  80, 213, 2),
--     (4, <guid>, 8, 'dps',    'mage_fire',    80, 213, 2);
