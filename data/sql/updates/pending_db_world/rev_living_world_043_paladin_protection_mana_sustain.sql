-- rev_living_world_043_paladin_protection_mana_sustain (world DB)
--
-- Protection Paladin in creature PvE was entering reserve too early and had no
-- explicit Divine Plea sustain button, which left long stretches of passive
-- white-swinging once mana dipped. Keep reserve mode, but make it less hair
-- trigger and add a simple self-sustain line.

UPDATE `living_world_bot_combat_default_profile`
SET
    `resource_low_water` = 12,
    `resource_high_water` = 22
WHERE `default_profile_id` = 35;

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` = 404;

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` = 404;

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` = 404;

INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (404, 35, 5, 'Divine Plea', 0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (4040, 404, 0, 0, 54428, 0, 0, 0, 'self');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (4040, 404, 0, 'self', 'mana_pct', 3, 85.0, ''),
    (4041, 404, 1, 'self', 'aura',     7, 0.0, '54428');
