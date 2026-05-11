-- rev_living_world_016_task_points_city_services (world DB)
--
-- DB-authored named task points for city services such as bank, mailbox,
-- auction house, and inn. These let task-template steps target concrete points
-- instead of only generic zone anchors.

CREATE TABLE IF NOT EXISTS living_world_task_point (
    point_id      INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    point_key     VARCHAR(64)       NOT NULL,
    zone_id       INT UNSIGNED      NOT NULL,
    map_id        SMALLINT UNSIGNED NOT NULL,
    point_type    VARCHAR(32)       NOT NULL,
    point_name    VARCHAR(100)      NOT NULL,
    x             FLOAT             NOT NULL,
    y             FLOAT             NOT NULL,
    z             FLOAT             NOT NULL,
    UNIQUE KEY uq_point_key (point_key),
    KEY idx_zone_type (zone_id, point_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('stormwind_bank',     1519, 0,   'bank',          'Stormwind Bank',          -8916.2,  622.8,  99.5),
    ('stormwind_mailbox',  1519, 0,   'mailbox',       'Stormwind Mailbox',       -8871.8,  649.1,  97.9),
    ('stormwind_ah',       1519, 0,   'auction_house', 'Stormwind Auction House', -8811.9,  667.3,  97.9),
    ('stormwind_inn',      1519, 0,   'inn',           'Stormwind Inn',           -8869.0,  673.8,  97.9),
    ('orgrimmar_bank',     1637, 1,   'bank',          'Orgrimmar Bank',           1631.5, -4375.0,  31.5),
    ('orgrimmar_mailbox',  1637, 1,   'mailbox',       'Orgrimmar Mailbox',        1637.9, -4441.3,  15.4),
    ('orgrimmar_ah',       1637, 1,   'auction_house', 'Orgrimmar Auction House',  1673.9, -4452.5,  19.1),
    ('orgrimmar_inn',      1637, 1,   'inn',           'Orgrimmar Inn',            1639.5, -4441.9,  15.8),
    ('shattrath_bank',     3703, 530, 'bank',          'Shattrath Bank',         -1884.3,  5438.1,  -12.4),
    ('shattrath_mailbox',  3703, 530, 'mailbox',       'Shattrath Mailbox',      -1833.4,  5436.5,  -12.4),
    ('shattrath_inn',      3703, 530, 'inn',           'Shattrath Inn',          -1887.2,  5765.3,  -12.4),
    ('dalaran_bank',       4395, 571, 'bank',          'Dalaran Bank',            5765.2,   690.8,  647.1),
    ('dalaran_mailbox',    4395, 571, 'mailbox',       'Dalaran Mailbox',         5718.3,   690.1,  646.7),
    ('dalaran_ah',         4395, 571, 'auction_house', 'Dalaran Auction House',   5865.9,   707.0,  650.0),
    ('dalaran_inn',        4395, 571, 'inn',           'Dalaran Inn',             5858.6,   596.9,  651.0)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);

SET @tbl = 'living_world_task_template_step';

SET @has_target_point_key = (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = @tbl
      AND COLUMN_NAME  = 'target_point_key');
SET @sql = IF(@has_target_point_key = 0,
    CONCAT('ALTER TABLE `', @tbl, '` ADD COLUMN `target_point_key` VARCHAR(64) NULL AFTER `target_zone_id`'),
    'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

INSERT INTO living_world_task_template
    (template_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('stormwind_city_services', 'Stormwind City Services', 'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_city_services', 'Orgrimmar City Services', 'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('dalaran_city_services',   'Dalaran City Services',   'city_errand', 0, 68, 80, 0, 0, 0, 2, 1)
ON DUPLICATE KEY UPDATE
    display_name     = VALUES(display_name),
    task_family      = VALUES(task_family),
    required_faction = VALUES(required_faction),
    min_level        = VALUES(min_level),
    max_level        = VALUES(max_level),
    weight           = VALUES(weight),
    is_enabled       = VALUES(is_enabled);

DELETE s
FROM living_world_task_template_step s
JOIN living_world_task_template t ON t.template_id = s.template_id
WHERE t.template_key IN ('stormwind_city_services', 'orgrimmar_city_services', 'dalaran_city_services');

INSERT INTO living_world_task_template_step
    (template_id, step_order, step_type, target_zone_id, target_point_key, duration_min_sec, duration_max_sec, label)
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_mailbox', 20, 60, 'Check Mail in Stormwind'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_city_services'
UNION ALL
SELECT t.template_id, 2, 'idle_city', 1519, 'stormwind_ah', 30, 90, 'Use AH in Stormwind'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_city_services'
UNION ALL
SELECT t.template_id, 3, 'idle_city', 1519, 'stormwind_bank', 60, 180, 'Idle by Stormwind Bank'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_city_services'
UNION ALL
SELECT t.template_id, 4, 'idle_inn', 1519, 'stormwind_inn', 120, 300, 'Rest at Stormwind Inn'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_city_services'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_mailbox', 20, 60, 'Check Mail in Orgrimmar'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_city_services'
UNION ALL
SELECT t.template_id, 2, 'idle_city', 1637, 'orgrimmar_ah', 30, 90, 'Use AH in Orgrimmar'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_city_services'
UNION ALL
SELECT t.template_id, 3, 'idle_city', 1637, 'orgrimmar_bank', 60, 180, 'Idle by Orgrimmar Bank'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_city_services'
UNION ALL
SELECT t.template_id, 4, 'idle_inn', 1637, 'orgrimmar_inn', 120, 300, 'Rest at Orgrimmar Inn'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_city_services'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 4395, 'dalaran_mailbox', 20, 60, 'Check Mail in Dalaran'
FROM living_world_task_template t WHERE t.template_key = 'dalaran_city_services'
UNION ALL
SELECT t.template_id, 2, 'idle_city', 4395, 'dalaran_ah', 30, 90, 'Use AH in Dalaran'
FROM living_world_task_template t WHERE t.template_key = 'dalaran_city_services'
UNION ALL
SELECT t.template_id, 3, 'idle_city', 4395, 'dalaran_bank', 60, 180, 'Idle by Dalaran Bank'
FROM living_world_task_template t WHERE t.template_key = 'dalaran_city_services'
UNION ALL
SELECT t.template_id, 4, 'idle_inn', 4395, 'dalaran_inn', 120, 300, 'Rest at Dalaran Inn'
FROM living_world_task_template t WHERE t.template_key = 'dalaran_city_services';