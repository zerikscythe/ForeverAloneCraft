-- rev_living_world_028_arcane_phase2_doctrine (world DB)
--
-- Phase 2 doctrine refinement for Arcane Mage.
--
-- Important source-of-truth rule:
--   Spell IDs below are validated against the locally extracted DBC-derived data
--   under tools/lw-editor/data/ (class_spells.json and spell_names.json), not
--   trusted from Icy Veins directly.
--
-- Local DBC-backed references used in this slice:
--   Arcane Blast      = 30451 / 42894 / 42896 / 42897
--   Arcane Missiles   = 5143 ... 42846
--   Arcane Barrage    = 44425 / 44780 / 44781
--   Fire Blast        = 2136 ... 42873
--   Blizzard          = 10 ... 42940
--   Arcane Explosion  = 1449 ... 42921
--   Flamestrike       = 2120 ... 42926
--   Mirror Image      = 55342
--   Icy Veins         = 12472
--   Arcane Power      = 12042
--   Presence of Mind  = 12043
--   Evocation         = 12051
--   Mana Sapphire     = item 33312 -> Replenish Mana 42987 (120s item cooldown)
--   Arcane Intellect  = 1459 ... 42995
--   Arcane Brilliance = 23028 / 27127 / 43002

-- -----------------------------------------------------------------------
-- ARCANE MAGE PROFILE 26
-- -----------------------------------------------------------------------
-- Goals for this phase-2 pass:
--   * keep the current AoE branch in place
--   * add explicit movement fallbacks for Arcane Barrage / Fire Blast
--   * add self-cast burst cooldowns
--   * add an Evocation escape valve for low mana
--   * add a Mana Sapphire self-use item rule for the new shared item runtime
--
-- Runtime note:
--   Ground-target spell execution now honors action-level aoe_mode /
--   aoe_min_targets / aoe_radius metadata, so Blizzard can cast at an enemy
--   cluster centroid instead of degrading to a plain unit-target spell call.

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270);

INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (270, 26,  0, 'Mana Sapphire (mana recovery)',              0, 0, 1, 0),
    (269, 26,  1, 'Evocation (low mana)',                       0, 0, 1, 0),
    (265, 26,  2, 'Mirror Image',                               0, 0, 1, 0),
    (266, 26,  4, 'Icy Veins',                                  0, 0, 1, 0),
    (267, 26,  6, 'Arcane Power',                               0, 0, 1, 0),
    (268, 26,  8, 'Presence of Mind',                           0, 0, 1, 0),
    (260, 26, 10, 'Blizzard / Arcane Explosion (3+ AoE)',       0, 0, 1, 0),
    (261, 26, 20, 'Arcane Missiles (Missile Barrage 3+ stacks)',0, 0, 1, 0),
    (262, 26, 30, 'Arcane Blast (stationary filler)',           0, 0, 1, 0),
    (263, 26, 40, 'Arcane Barrage (moving)',                    0, 0, 1, 0),
    (264, 26, 50, 'Fire Blast (moving fallback)',               0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    `default_profile_id`  = VALUES(`default_profile_id`),
    `priority`            = VALUES(`priority`),
    `label`               = VALUES(`label`),
    `is_interrupt`        = VALUES(`is_interrupt`),
    `breaks_current_cast` = VALUES(`breaks_current_cast`),
    `enabled`             = VALUES(`enabled`),
    `condition_logic`     = VALUES(`condition_logic`);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`, `aoe_mode`, `aoe_min_targets`, `aoe_radius`)
VALUES
    -- Burst / cooldown layer
    (2650, 265, 0, 0, 55342, 0, 1, 0, 'self',          NULL, NULL, NULL), -- Mirror Image (ExactSpellId)
    (2660, 266, 0, 0, 12472, 0, 1, 0, 'self',          NULL, NULL, NULL), -- Icy Veins (ExactSpellId)
    (2670, 267, 0, 0, 12042, 0, 1, 0, 'self',          NULL, NULL, NULL), -- Arcane Power (ExactSpellId)
    (2680, 268, 0, 0, 12043, 0, 1, 0, 'self',          NULL, NULL, NULL), -- Presence of Mind (ExactSpellId)
    (2690, 269, 0, 0, 12051, 0, 1, 0, 'self',          NULL, NULL, NULL), -- Evocation (ExactSpellId)
    (2700, 270, 0, 1,     0, 33312, 0, 0, 'self',      NULL, NULL, NULL), -- Mana Sapphire item (shared real/fake item runtime)

    -- AoE / ST layer
    (2600, 260, 0, 0, 42940, 0, 0, 0, 'enemy_primary', 0,    3,   10),   -- Blizzard (BestKnown, centroid AoE)
    (2601, 260, 1, 0, 42920, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL), -- Arcane Explosion fallback (BestKnown)
    (2610, 261, 0, 0,  5143, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL), -- Arcane Missiles (BestKnown)
    (2620, 262, 0, 0, 30451, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL), -- Arcane Blast (BestKnown)
    (2630, 263, 0, 0, 44781, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL), -- Arcane Barrage (BestKnown)
    (2640, 264, 0, 0, 42873, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL)  -- Fire Blast (BestKnown)
ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `slot`          = VALUES(`slot`),
    `action_type`   = VALUES(`action_type`),
    `spell_base_id` = VALUES(`spell_base_id`),
    `item_id`       = VALUES(`item_id`),
    `rank_mode`     = VALUES(`rank_mode`),
    `rank_value`    = VALUES(`rank_value`),
    `target_key`    = VALUES(`target_key`),
    `aoe_mode`      = VALUES(`aoe_mode`),
    `aoe_min_targets` = VALUES(`aoe_min_targets`),
    `aoe_radius`    = VALUES(`aoe_radius`);

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    -- Evocation when mana is critically low and the mage is not moving.
    (2690, 269, 0, 'self',  'mana_pct',    3, 15.0, ''),
    (2691, 269, 1, 'self',  'is_moving',   3,  0.0, ''),

    -- Mana Sapphire before almost any real mana deficit so validation can force
    -- the bagless simulated-item path deterministically.
    (2700, 270, 0, 'self',  'mana_pct',    3, 99.0, ''),

    -- AoE branch: 3+ nearby enemies in 10 yards.
    (2600, 260, 0, 'enemy', 'nearby_enemies', 5, 3.0, '10'),

    -- Missile Barrage consume after building Arcane Blast stacks.
    (2610, 261, 0, 'self',  'aura',        6, 44401.0, ''),
    (2611, 261, 1, 'self',  'aura_stacks', 5,   3.0, '36032'),

    -- Arcane Blast filler should only fire while stationary.
    (2620, 262, 0, 'self',  'is_moving',   3,   0.0, ''),

    -- Arcane Barrage / Fire Blast are explicit movement fallbacks.
    (2630, 263, 0, 'self',  'is_moving',   5,   1.0, ''),
    (2640, 264, 0, 'self',  'is_moving',   5,   1.0, '')
ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `sequence`      = VALUES(`sequence`),
    `subject_key`   = VALUES(`subject_key`),
    `stat_key`      = VALUES(`stat_key`),
    `comparison`    = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value`  = VALUES(`string_value`);