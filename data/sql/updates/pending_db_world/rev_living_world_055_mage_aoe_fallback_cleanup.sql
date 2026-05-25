-- rev_living_world_055_mage_aoe_fallback_cleanup
--
-- Secondary actions share the parent entry's condition block, so they should
-- only be used when they are genuinely the same job. Split Mage point-blank
-- Arcane Explosion out from ranged Blizzard entries so the close-range spell
-- no longer inherits target-centered AoE conditions.

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Blizzard (3+ AoE)'
WHERE `entry_id` IN (266, 276, 354);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `action_id` IN (2661, 2761, 3541);

INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (2666, 26, 25, 'Arcane Explosion (3+ close AoE)', 0, 0, 1, 0),
    (2766, 27, 27, 'Arcane Explosion (3+ close AoE)', 0, 0, 1, 0),
    (3546,  8, 25, 'Arcane Explosion (3+ close AoE)', 0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    `default_profile_id`   = VALUES(`default_profile_id`),
    `priority`             = VALUES(`priority`),
    `label`                = VALUES(`label`),
    `is_interrupt`         = VALUES(`is_interrupt`),
    `breaks_current_cast`  = VALUES(`breaks_current_cast`),
    `enabled`              = VALUES(`enabled`),
    `condition_logic`      = VALUES(`condition_logic`);

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (2666, 2766, 3546);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (2666, 2766, 3546);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    (26660, 2666, 0, 0, 42920, 0, 0, 0, 'self', NULL, NULL, NULL),
    (27660, 2766, 0, 0, 42920, 0, 0, 0, 'self', NULL, NULL, NULL),
    (35460, 3546, 0, 0, 42920, 0, 0, 0, 'self', NULL, NULL, NULL);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (26660, 2666, 0, 'self', 'nearby_enemies', 5, 3.0, '10'),
    (27660, 2766, 0, 'self', 'nearby_enemies', 5, 3.0, '10'),
    (35460, 3546, 0, 'self', 'nearby_enemies', 5, 3.0, '10');
