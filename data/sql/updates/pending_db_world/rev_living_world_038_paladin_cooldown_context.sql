-- ---------------------------------------------------------------------------
-- Paladin cooldown context refinement
-- ---------------------------------------------------------------------------
-- Teach the rebuilt Paladin profiles a little more about when offensive and
-- defensive cooldown families conflict.
--
-- Key intent:
--   * Ret should not pop Avenging Wrath while already under unsafe pressure.
--   * Divine Shield should know that the Avenging Wrath marker blocks it.
--   * Keep the rules data-driven so the same doctrine language can expand into
--     other class cooldown conflicts later.

-- Ret/Holy bubble entries: Divine Shield is invalid while the Avenging Wrath
-- marker aura is active.
INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3853, 385, 3, 'self', 'aura', 7, 0.0, '61987'),
    (3883, 388, 3, 'self', 'aura', 7, 0.0, '61987')
ON DUPLICATE KEY UPDATE
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);

-- Ret Avenging Wrath: keep Wings for healthier, lower-pressure windows so the
-- bot does not burn its immunity lockout family before it needs to survive.
INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3630, 363, 0, 'self', 'hp_pct', 5, 55.0, ''),
    (3631, 363, 1, 'self', 'nearby_enemies', 3, 2.0, '10'),
    (3632, 363, 2, 'self', 'hazard_repeat', 0, 0.0, ''),
    (3633, 363, 3, 'self', 'hazard_aura', 0, 0.0, ''),
    (3634, 363, 4, 'self', 'aura', 7, 0.0, '61987')
ON DUPLICATE KEY UPDATE
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);
