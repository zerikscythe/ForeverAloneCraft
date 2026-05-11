-- rev_living_world_019_playlists (world DB)
--
-- Higher-level playlists/routines built from reusable task templates.

CREATE TABLE IF NOT EXISTS living_world_playlist (
    playlist_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    playlist_key         VARCHAR(64)       NOT NULL,
    display_name         VARCHAR(100)      NOT NULL,
    task_family          VARCHAR(32)       NOT NULL DEFAULT 'routine',
    required_faction     TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    min_level            TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level            TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    requires_herbalism   TINYINT(1)        NOT NULL DEFAULT 0,
    requires_mining      TINYINT(1)        NOT NULL DEFAULT 0,
    requires_fishing     TINYINT(1)        NOT NULL DEFAULT 0,
    weight               TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    is_enabled           TINYINT(1)        NOT NULL DEFAULT 1,
    UNIQUE KEY uq_playlist_key (playlist_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS living_world_playlist_entry (
    playlist_id          INT UNSIGNED      NOT NULL,
    entry_order          INT UNSIGNED      NOT NULL,
    task_template_id     INT UNSIGNED      NOT NULL,
    repeat_count         TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    note                 VARCHAR(255)      NULL,
    PRIMARY KEY (playlist_id, entry_order),
    KEY idx_playlist_template (task_template_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO living_world_playlist
    (playlist_key, display_name, task_family, required_faction, min_level, max_level, weight, is_enabled)
VALUES
    ('stormwind_city_errands_routine', 'Stormwind City Errands Routine', 'routine', 1, 1, 80, 2, 1),
    ('orgrimmar_city_errands_routine', 'Orgrimmar City Errands Routine', 'routine', 2, 1, 80, 2, 1),
    ('dalaran_city_errands_routine',   'Dalaran City Errands Routine',   'routine', 0, 68, 80, 2, 1)
ON DUPLICATE KEY UPDATE
    display_name     = VALUES(display_name),
    task_family      = VALUES(task_family),
    required_faction = VALUES(required_faction),
    min_level        = VALUES(min_level),
    max_level        = VALUES(max_level),
    weight           = VALUES(weight),
    is_enabled       = VALUES(is_enabled);

DELETE e
FROM living_world_playlist_entry e
JOIN living_world_playlist p ON p.playlist_id = e.playlist_id
WHERE p.playlist_key IN (
    'stormwind_city_errands_routine',
    'orgrimmar_city_errands_routine',
    'dalaran_city_errands_routine');

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Mail, AH, bank, inn circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'stormwind_city_errands_routine'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Repeat city circuit once more for a fuller routine'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_city_services'
WHERE p.playlist_key = 'stormwind_city_errands_routine'
UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Mail, AH, bank, inn circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'orgrimmar_city_errands_routine'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Repeat city circuit once more for a fuller routine'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_city_services'
WHERE p.playlist_key = 'orgrimmar_city_errands_routine'
UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Mail, AH, bank, inn circuit'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'dalaran_city_errands_routine'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Repeat city circuit once more for a fuller routine'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'dalaran_city_services'
WHERE p.playlist_key = 'dalaran_city_errands_routine';