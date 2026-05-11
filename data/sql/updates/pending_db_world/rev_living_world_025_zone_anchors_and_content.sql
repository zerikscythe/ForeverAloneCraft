-- rev_living_world_025_zone_anchors_and_content (world DB)
--
-- Adds semantic zone anchors and broad zone content rows so playlists can remain
-- ordered lists of authored tasks while each built task still resolves its
-- concrete city, zone, and travel solution from bot context.

CREATE TABLE IF NOT EXISTS living_world_zone_anchor (
    anchor_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    zone_id            INT UNSIGNED      NOT NULL,
    point_key          VARCHAR(64)       NOT NULL,
    anchor_role        VARCHAR(32)       NOT NULL,
    required_faction   TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    min_level          TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level          TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    weight             TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    notes              VARCHAR(255)      NULL,
    UNIQUE KEY uq_zone_anchor_role_point (zone_id, anchor_role, point_key),
    KEY idx_zone_anchor_lookup (zone_id, anchor_role, required_faction, min_level, max_level),
    KEY idx_zone_anchor_point (point_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS living_world_zone_content (
    content_id         INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    zone_id            INT UNSIGNED      NOT NULL,
    content_kind       VARCHAR(32)       NOT NULL,
    subject_id         INT UNSIGNED      NULL,
    subject_key        VARCHAR(64)       NULL,
    display_name       VARCHAR(100)      NOT NULL,
    required_faction   TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    min_level          TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level          TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    min_skill          SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    max_skill          SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    weight             TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    anchor_point_key   VARCHAR(64)       NULL,
    return_anchor_role VARCHAR(32)       NULL,
    notes              VARCHAR(255)      NULL,
    UNIQUE KEY uq_zone_content_kind_name (zone_id, content_kind, display_name),
    KEY idx_zone_content_lookup (content_kind, zone_id, required_faction, min_level, max_level),
    KEY idx_zone_content_subject (content_kind, subject_id, subject_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO living_world_zone_anchor
    (zone_id, point_key, anchor_role, required_faction, min_level, max_level, weight, notes)
VALUES
    (1519, 'stormwind_inn',     'home',                       1,  1, 80, 3, 'Alliance default home/bind anchor'),
    (1519, 'stormwind_inn',     'town',                       1,  1, 80, 2, 'Alliance city return anchor'),
    (1519, 'stormwind_mailbox', 'city_service_mailbox',       1,  1, 80, 2, 'Stormwind mailbox for generic city chores'),
    (1519, 'stormwind_ah',      'city_service_auction_house', 1,  1, 80, 2, 'Stormwind AH for generic city chores'),
    (1519, 'stormwind_bank',    'city_service_bank',          1,  1, 80, 2, 'Stormwind bank for generic city chores'),
    (1519, 'stormwind_inn',     'city_service_inn',           1,  1, 80, 2, 'Stormwind inn for generic city chores'),

    (1637, 'orgrimmar_inn',     'home',                       2,  1, 80, 3, 'Horde default home/bind anchor'),
    (1637, 'orgrimmar_inn',     'town',                       2,  1, 80, 2, 'Horde city return anchor'),
    (1637, 'orgrimmar_mailbox', 'city_service_mailbox',       2,  1, 80, 2, 'Orgrimmar mailbox for generic city chores'),
    (1637, 'orgrimmar_ah',      'city_service_auction_house', 2,  1, 80, 2, 'Orgrimmar AH for generic city chores'),
    (1637, 'orgrimmar_bank',    'city_service_bank',          2,  1, 80, 2, 'Orgrimmar bank for generic city chores'),
    (1637, 'orgrimmar_inn',     'city_service_inn',           2,  1, 80, 2, 'Orgrimmar inn for generic city chores'),

    (3703, 'shattrath_inn',     'home',                       0, 58, 80, 3, 'Shared Outland home/bind anchor'),
    (3703, 'shattrath_inn',     'town',                       0, 58, 80, 2, 'Outland city return anchor'),
    (3703, 'shattrath_arrival', 'portal_arrival',             0, 58, 80, 2, 'Outland portal arrival anchor'),
    (3703, 'shattrath_mailbox', 'city_service_mailbox',       0, 58, 80, 2, 'Shattrath mailbox for generic city chores'),
    (3703, 'shattrath_bank',    'city_service_bank',          0, 58, 80, 2, 'Shattrath bank for generic city chores'),
    (3703, 'shattrath_inn',     'city_service_inn',           0, 58, 80, 2, 'Shattrath inn for generic city chores'),

    (4395, 'dalaran_inn',       'home',                       0, 68, 80, 3, 'Shared Northrend home/bind anchor'),
    (4395, 'dalaran_inn',       'town',                       0, 68, 80, 2, 'Northrend city return anchor'),
    (4395, 'dalaran_mailbox',   'city_service_mailbox',       0, 68, 80, 2, 'Dalaran mailbox for generic city chores'),
    (4395, 'dalaran_ah',        'city_service_auction_house', 0, 68, 80, 2, 'Dalaran AH for generic city chores'),
    (4395, 'dalaran_bank',      'city_service_bank',          0, 68, 80, 2, 'Dalaran bank for generic city chores'),
    (4395, 'dalaran_inn',       'city_service_inn',           0, 68, 80, 2, 'Dalaran inn for generic city chores')
ON DUPLICATE KEY UPDATE
    required_faction = VALUES(required_faction),
    min_level        = VALUES(min_level),
    max_level        = VALUES(max_level),
    weight           = VALUES(weight),
    notes            = VALUES(notes);

INSERT INTO living_world_zone_content
    (zone_id, content_kind, subject_id, subject_key, display_name, required_faction,
     min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes)
SELECT z.zone_id, 'herb', NULL, 'auto_herb', CONCAT('Herbalism in ', z.zone_name), z.faction,
       z.min_level, z.max_level, 0, 0, 2, NULL, 'town',
       'Broad herb-capable zone content derived from living_world_zone_index'
FROM living_world_zone_index z
WHERE z.has_herbs = 1
ON DUPLICATE KEY UPDATE
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    min_skill          = VALUES(min_skill),
    max_skill          = VALUES(max_skill),
    weight             = VALUES(weight),
    anchor_point_key   = VALUES(anchor_point_key),
    return_anchor_role = VALUES(return_anchor_role),
    notes              = VALUES(notes);

INSERT INTO living_world_zone_content
    (zone_id, content_kind, subject_id, subject_key, display_name, required_faction,
     min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes)
SELECT z.zone_id, 'ore', NULL, 'auto_ore', CONCAT('Mining in ', z.zone_name), z.faction,
       z.min_level, z.max_level, 0, 0, 2, NULL, 'town',
       'Broad ore-capable zone content derived from living_world_zone_index'
FROM living_world_zone_index z
WHERE z.has_ore = 1
ON DUPLICATE KEY UPDATE
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    min_skill          = VALUES(min_skill),
    max_skill          = VALUES(max_skill),
    weight             = VALUES(weight),
    anchor_point_key   = VALUES(anchor_point_key),
    return_anchor_role = VALUES(return_anchor_role),
    notes              = VALUES(notes);

INSERT INTO living_world_zone_content
    (zone_id, content_kind, subject_id, subject_key, display_name, required_faction,
     min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes)
SELECT z.zone_id, 'fish', NULL, 'auto_fish', CONCAT('Fishing in ', z.zone_name), z.faction,
       z.min_level, z.max_level, 0, 0, 2, NULL, 'town',
       'Broad fish-capable zone content derived from living_world_zone_index'
FROM living_world_zone_index z
WHERE z.has_fish = 1
ON DUPLICATE KEY UPDATE
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    min_skill          = VALUES(min_skill),
    max_skill          = VALUES(max_skill),
    weight             = VALUES(weight),
    anchor_point_key   = VALUES(anchor_point_key),
    return_anchor_role = VALUES(return_anchor_role),
    notes              = VALUES(notes);

INSERT INTO living_world_zone_content
    (zone_id, content_kind, subject_id, subject_key, display_name, required_faction,
     min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes)
VALUES
    (1519, 'city_service', NULL, 'city_chores', 'City chores in Stormwind', 1,  1, 80, 0, 0, 3, 'stormwind_inn', 'town', 'Broad Alliance city-chore content row'),
    (1637, 'city_service', NULL, 'city_chores', 'City chores in Orgrimmar', 2,  1, 80, 0, 0, 3, 'orgrimmar_inn', 'town', 'Broad Horde city-chore content row'),
    (3703, 'city_service', NULL, 'city_chores', 'City chores in Shattrath', 0, 58, 80, 0, 0, 3, 'shattrath_inn', 'town', 'Broad Outland city-chore content row'),
    (4395, 'city_service', NULL, 'city_chores', 'City chores in Dalaran',   0, 68, 80, 0, 0, 3, 'dalaran_inn',   'town', 'Broad Northrend city-chore content row')
ON DUPLICATE KEY UPDATE
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    min_skill          = VALUES(min_skill),
    max_skill          = VALUES(max_skill),
    weight             = VALUES(weight),
    anchor_point_key   = VALUES(anchor_point_key),
    return_anchor_role = VALUES(return_anchor_role),
    notes              = VALUES(notes);