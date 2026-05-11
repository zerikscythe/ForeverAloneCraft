-- rev_living_world_015_task_templates_generic_gathering (world DB)
--
-- First DB-authored task-template/task-step foundation for composable world-bot
-- work chains. Initial slice: travel -> generic gathering.

CREATE TABLE IF NOT EXISTS living_world_task_template (
    template_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    template_key         VARCHAR(64)       NOT NULL,
    display_name         VARCHAR(100)      NOT NULL,
    task_family          VARCHAR(32)       NOT NULL DEFAULT 'misc',
    required_faction     TINYINT UNSIGNED  NOT NULL DEFAULT 0,
    min_level            TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    max_level            TINYINT UNSIGNED  NOT NULL DEFAULT 80,
    requires_herbalism   TINYINT(1)        NOT NULL DEFAULT 0,
    requires_mining      TINYINT(1)        NOT NULL DEFAULT 0,
    requires_fishing     TINYINT(1)        NOT NULL DEFAULT 0,
    weight               TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    is_enabled           TINYINT(1)        NOT NULL DEFAULT 1,
    UNIQUE KEY uq_template_key (template_key),
    KEY idx_template_match (is_enabled, required_faction, min_level, max_level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS living_world_task_template_step (
    template_id          INT UNSIGNED      NOT NULL,
    step_order           SMALLINT UNSIGNED NOT NULL,
    step_type            VARCHAR(32)       NOT NULL,
    target_zone_id       INT UNSIGNED      NOT NULL,
    duration_min_sec     INT UNSIGNED      NOT NULL DEFAULT 0,
    duration_max_sec     INT UNSIGNED      NOT NULL DEFAULT 0,
    label                VARCHAR(100)      NOT NULL,
    PRIMARY KEY (template_id, step_order),
    KEY idx_step_zone (target_zone_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO living_world_task_template
    (template_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('gather_herb_low_alliance',  'Gather Herbs in Ashenvale',         'gathering', 1, 18, 30, 1, 0, 0, 4, 1),
    ('gather_herb_low_horde',     'Gather Herbs in Silverpine',        'gathering', 2, 10, 20, 1, 0, 0, 4, 1),
    ('gather_ore_mid_horde',      'Gather Ore in Hillsbrad',           'gathering', 2, 20, 30, 0, 1, 0, 4, 1),
    ('fish_mid_neutral',          'Fish in Feralas',                   'fishing',   0, 40, 50, 0, 0, 1, 3, 1),
    ('gather_herb_outland',       'Gather Herbs in Nagrand',           'gathering', 0, 64, 67, 1, 0, 0, 4, 1),
    ('gather_ore_northrend',      'Gather Ore in Grizzly Hills',       'gathering', 0, 73, 78, 0, 1, 0, 4, 1),
    ('fish_northrend',            'Fish in Grizzly Hills',             'fishing',   0, 73, 78, 0, 0, 1, 3, 1)
ON DUPLICATE KEY UPDATE
    display_name       = VALUES(display_name),
    task_family        = VALUES(task_family),
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    requires_herbalism = VALUES(requires_herbalism),
    requires_mining    = VALUES(requires_mining),
    requires_fishing   = VALUES(requires_fishing),
    weight             = VALUES(weight),
    is_enabled         = VALUES(is_enabled);

DELETE s
FROM living_world_task_template_step s
JOIN living_world_task_template t ON t.template_id = s.template_id
WHERE t.template_key IN (
    'gather_herb_low_alliance',
    'gather_herb_low_horde',
    'gather_ore_mid_horde',
    'fish_mid_neutral',
    'gather_herb_outland',
    'gather_ore_northrend',
    'fish_northrend');

INSERT INTO living_world_task_template_step
    (template_id, step_order, step_type, target_zone_id, duration_min_sec, duration_max_sec, label)
SELECT t.template_id, 1, 'gather_herb', 331,  600, 1200, 'Gather Herbs in Ashenvale'
FROM living_world_task_template t WHERE t.template_key = 'gather_herb_low_alliance'
UNION ALL
SELECT t.template_id, 1, 'gather_herb', 130,  600, 1200, 'Gather Herbs in Silverpine'
FROM living_world_task_template t WHERE t.template_key = 'gather_herb_low_horde'
UNION ALL
SELECT t.template_id, 1, 'gather_ore', 267,  600, 1200, 'Gather Ore in Hillsbrad'
FROM living_world_task_template t WHERE t.template_key = 'gather_ore_mid_horde'
UNION ALL
SELECT t.template_id, 1, 'fish', 357, 600, 1200, 'Fish in Feralas'
FROM living_world_task_template t WHERE t.template_key = 'fish_mid_neutral'
UNION ALL
SELECT t.template_id, 1, 'gather_herb', 3518, 900, 1500, 'Gather Herbs in Nagrand'
FROM living_world_task_template t WHERE t.template_key = 'gather_herb_outland'
UNION ALL
SELECT t.template_id, 1, 'gather_ore', 394, 900, 1500, 'Gather Ore in Grizzly Hills'
FROM living_world_task_template t WHERE t.template_key = 'gather_ore_northrend'
UNION ALL
SELECT t.template_id, 1, 'fish', 394, 900, 1500, 'Fish in Grizzly Hills'
FROM living_world_task_template t WHERE t.template_key = 'fish_northrend';