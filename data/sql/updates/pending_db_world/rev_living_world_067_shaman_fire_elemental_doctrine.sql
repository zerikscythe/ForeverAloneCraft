-- Add the missing Enhancement Shaman Fire Elemental Totem cooldown to the
-- modern PvE doctrine. Icy Veins explicitly recommends pairing it with
-- Feral Spirit in the Wrath priority list, and our first-pass rebuild omitted it.

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` = 5416;

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` = 5416;

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` = 5416;

INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (5416, 25, 15, 'Fire Elemental Totem', 0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (54160, 5416, 0, 0, 2894, 0, 0, 0, 'self');
