-- rev_living_world_042_paladin_protection_judgement_discipline (world DB)
--
-- Protection Paladin should treat Judgement of Wisdom as upkeep, not as a
-- primary rotational button that crowds out real tank pressure. Keep Holy
-- Shield / Hammer of the Righteous / Shield of Righteousness ahead of it and
-- only refresh Judgement when the debuff is missing or nearly expired.

-- Lower Judgement below the core tank buttons.
UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 55
WHERE `entry_id` = 398;

UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 25
WHERE `entry_id` = 399;

-- Refresh Judgement of Wisdom only when the debuff is gone or close to falling
-- off. SPELL_PALADIN_JUDGEMENT_OF_WISDOM is 20186 in core script constants.
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `condition_id` = 3981;

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3981, 398, 1, 'enemy', 'aura_remaining_secs', 3, 3.0, '20186')
ON DUPLICATE KEY UPDATE
    `entry_id` = VALUES(`entry_id`),
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);
