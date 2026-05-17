-- ---------------------------------------------------------------------------
-- Paladin panic ordering and interrupt handling
-- ---------------------------------------------------------------------------
-- Emergency survival buttons should behave like true panic reactions, not
-- ordinary rotation rows. In particular, Ret/Holy should prefer bubbling
-- before burning Lay on Hands on themselves, otherwise Forbearance can lock
-- the bubble out before the profile gets another chance to save itself.

UPDATE `living_world_bot_combat_default_entry`
SET
    `priority` = 0,
    `is_interrupt` = 1,
    `breaks_current_cast` = 1
WHERE `entry_id` IN (385, 388, 403);

UPDATE `living_world_bot_combat_default_entry`
SET
    `priority` = 1,
    `is_interrupt` = 1,
    `breaks_current_cast` = 1
WHERE `entry_id` IN (384, 387, 402);

-- Do not consider Divine Shield / Divine Protection while Forbearance is up.
INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3852, 385, 2, 'self', 'aura', 7, 0.0, '25771'),
    (3882, 388, 2, 'self', 'aura', 7, 0.0, '25771'),
    (4032, 403, 2, 'self', 'aura', 7, 0.0, '25771')
ON DUPLICATE KEY UPDATE
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);
