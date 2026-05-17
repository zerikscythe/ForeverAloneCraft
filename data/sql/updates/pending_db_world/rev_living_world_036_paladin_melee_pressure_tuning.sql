-- rev_living_world_036_paladin_melee_pressure_tuning (world DB)
--
-- Sandbox pressure tuning after the first Paladin doctrine passes showed
-- excessive ranged Judgement loops and not enough actual melee commitment.
-- The goal here is not a final raid rotation; it is to:
--   * stop Ret/Prot from idling at 20-30 yards trading Judgements
--   * push melee buttons ahead of Judgement once they have closed
--   * make the combat sandbox lethal enough to exercise death/recovery logic

-- Retribution: prefer Divine Storm ahead of Judgement and require Judgement to
-- happen once the bot is actually in melee commitment range.
UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 35
WHERE `entry_id` = 367;

UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 45
WHERE `entry_id` = 366;

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `condition_id` = 3661;

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3661, 366, 1, 'enemy', 'distance', 3, 10.0, '')
ON DUPLICATE KEY UPDATE
    `entry_id` = VALUES(`entry_id`),
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);

-- Holy: keep emergency healing behavior intact, but stop idle ranged Judgement
-- spam from turning healers into weird pseudo-casters when no triage is needed.
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `condition_id` = 3830;

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3830, 383, 0, 'enemy', 'distance', 3, 15.0, '')
ON DUPLICATE KEY UPDATE
    `entry_id` = VALUES(`entry_id`),
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);

-- Protection: let Shield of Righteousness compete before Judgement once the bot
-- has a target in front of it, and keep Judgement from being the default ranged
-- fallback all fight long.
UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 25
WHERE `entry_id` = 399;

UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 35
WHERE `entry_id` = 398;

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `condition_id` = 3981;

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3981, 398, 1, 'enemy', 'distance', 3, 10.0, '')
ON DUPLICATE KEY UPDATE
    `entry_id` = VALUES(`entry_id`),
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);
