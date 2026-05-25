-- rev_living_world_052_assigned_gear_endgame_templates (world DB)
--
-- Adds a curated template table for level-80 world-bot assigned gear stages.
-- Fresh 80s should receive pre-raid sets immediately, then climb through
-- curated endgame stages over their final 50 world hours instead of relying on
-- the random assigned-gear generator.

CREATE TABLE IF NOT EXISTS `living_world_bot_assigned_gear_template` (
    `template_id` bigint unsigned NOT NULL AUTO_INCREMENT,
    `class_id` tinyint unsigned NOT NULL,
    `spec_key` varchar(32) NOT NULL DEFAULT '',
    `loadout_key` varchar(64) NOT NULL DEFAULT '',
    `endgame_stage` tinyint unsigned NOT NULL,
    `slot_id` tinyint unsigned NOT NULL,
    `item_id` int unsigned NOT NULL,
    `enchantments` varchar(512) NOT NULL DEFAULT '',
    PRIMARY KEY (`template_id`),
    UNIQUE KEY `uk_lw_bot_assigned_gear_template`
        (`class_id`, `spec_key`, `loadout_key`, `endgame_stage`, `slot_id`),
    KEY `idx_lw_bot_assigned_gear_template_lookup`
        (`class_id`, `endgame_stage`, `spec_key`, `loadout_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
