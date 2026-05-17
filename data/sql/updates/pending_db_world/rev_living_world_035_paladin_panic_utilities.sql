-- ---------------------------------------------------------------------------
-- Paladin panic utilities
-- ---------------------------------------------------------------------------
-- Add first-pass emergency buttons to the rebuilt Paladin PvE doctrines.
-- Notes:
--   * Lay on Hands is a save, not a resurrection.
--   * Holy/Ret use Divine Shield as their personal oh-no button.
--   * Protection uses Divine Protection instead so the tank does not bubble
--     itself out of threat.

INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (384, 11,  5, 'Lay on Hands (ally emergency)',     0, 1, 1, 0),
    (385, 11,  6, 'Divine Shield (self emergency)',    0, 1, 1, 0),
    (387,  2,  3, 'Lay on Hands (ally emergency)',     0, 1, 1, 0),
    (388,  2,  5, 'Divine Shield (self emergency)',    0, 1, 1, 0),
    (402, 35,  5, 'Lay on Hands (ally emergency)',     0, 1, 1, 0),
    (403, 35,  7, 'Divine Protection (self emergency)', 0, 1, 1, 0)
ON DUPLICATE KEY UPDATE
    `priority` = VALUES(`priority`),
    `label` = VALUES(`label`),
    `is_interrupt` = VALUES(`is_interrupt`),
    `breaks_current_cast` = VALUES(`breaks_current_cast`),
    `enabled` = VALUES(`enabled`),
    `condition_logic` = VALUES(`condition_logic`);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (3840, 384, 0, 0, 633, 0, 0, 0, 'lowest_hp_party'),
    (3850, 385, 0, 0, 642, 0, 0, 0, 'self'),
    (3870, 387, 0, 0, 633, 0, 0, 0, 'lowest_hp_party'),
    (3880, 388, 0, 0, 642, 0, 0, 0, 'self'),
    (4020, 402, 0, 0, 633, 0, 0, 0, 'lowest_hp_party'),
    (4030, 403, 0, 0, 498, 0, 0, 0, 'self')
ON DUPLICATE KEY UPDATE
    `action_type` = VALUES(`action_type`),
    `spell_base_id` = VALUES(`spell_base_id`),
    `item_id` = VALUES(`item_id`),
    `rank_mode` = VALUES(`rank_mode`),
    `rank_value` = VALUES(`rank_value`),
    `target_key` = VALUES(`target_key`);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3840, 384, 0, 'lowest_hp_party', 'exists', 8, 0.0, ''),
    (3841, 384, 1, 'lowest_hp_party', 'hp_pct', 3, 12.0, ''),
    (3850, 385, 0, 'self', 'hp_pct', 3, 20.0, ''),
    (3851, 385, 1, 'self', 'aura',   7, 0.0, '642'),

    (3870, 387, 0, 'lowest_hp_party', 'exists', 8, 0.0, ''),
    (3871, 387, 1, 'lowest_hp_party', 'hp_pct', 3, 12.0, ''),
    (3880, 388, 0, 'self', 'hp_pct', 3, 20.0, ''),
    (3881, 388, 1, 'self', 'aura',   7, 0.0, '642'),

    (4020, 402, 0, 'lowest_hp_party', 'exists', 8, 0.0, ''),
    (4021, 402, 1, 'lowest_hp_party', 'hp_pct', 3, 12.0, ''),
    (4030, 403, 0, 'self', 'hp_pct', 3, 20.0, ''),
    (4031, 403, 1, 'self', 'aura',   7, 0.0, '498')
ON DUPLICATE KEY UPDATE
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);
