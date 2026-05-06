-- LivingWorld: per-bot talent template preference (characters DB)
-- source_character_guid is the bot's own character GUID (same key as
-- living_world_bot_ooc_config and living_world_bot_combat_runtime_selection).
-- template_id = 0 means auto-detect from spec_key + class_id via
-- SimpleBotCombatSpecRoleResolver; any other value pins a specific template.
-- auto_apply_on_level = 1 causes the applicator to fire on OnPlayerLevelChanged.

CREATE TABLE IF NOT EXISTS `living_world_bot_talent_preference` (
    `source_character_guid` BIGINT UNSIGNED NOT NULL,
    `template_id`           BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `auto_apply_on_level`   TINYINT(1) NOT NULL DEFAULT 1,
    PRIMARY KEY (`source_character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
