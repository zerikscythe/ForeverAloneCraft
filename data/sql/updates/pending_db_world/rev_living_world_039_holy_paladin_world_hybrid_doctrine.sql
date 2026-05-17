-- ---------------------------------------------------------------------------
-- Holy Paladin world-hybrid doctrine pass
-- ---------------------------------------------------------------------------
-- Goals:
--   * keep dungeon/raid Holy more mana-aware and heal-first
--   * let open-world Holy contribute sane pressure when nobody needs help
--   * heal earlier so ambient Holy Paladins stop behaving like late-reacting
--     Judgement bots

UPDATE `living_world_bot_combat_default_profile`
SET
    `description` = 'Context-aware Holy Paladin healer profile: conservative in dungeon or raid combat, looser hybrid support in the open world.',
    `conservation_mode` = 2,
    `resource_low_water` = 35,
    `resource_high_water` = 55
WHERE `default_profile_id` = 11;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Judgement of Wisdom (world stable pressure)'
WHERE `entry_id` = 383;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Shock (self emergency)'
WHERE `entry_id` = 375;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Shock (self while moving)'
WHERE `entry_id` = 377;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Flash of Light (self support)'
WHERE `entry_id` = 379;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Flash of Light (ally support)'
WHERE `entry_id` = 380;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Light (self stable)'
WHERE `entry_id` = 381;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Light (ally stable)'
WHERE `entry_id` = 382;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 40
WHERE `condition_id` = 3750;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 45
WHERE `condition_id` = 3761;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 90
WHERE `condition_id` = 3771;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 85
WHERE `condition_id` = 3782;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 70
WHERE `condition_id` = 3790;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 75
WHERE `condition_id` = 3801;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 85
WHERE `condition_id` = 3810;

UPDATE `living_world_bot_combat_default_condition`
SET `numeric_value` = 88
WHERE `condition_id` = 3821;

DELETE FROM `living_world_bot_combat_default_condition`
WHERE `entry_id` IN (383, 386, 387, 388, 390);

DELETE FROM `living_world_bot_combat_default_action`
WHERE `entry_id` IN (386, 387, 388, 390);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `entry_id` IN (386, 387, 388, 390);

INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    (386, 11, 44, 'Hammer of Wrath (world execute)', 0, 0, 1, 0),
    (387, 11, 46, 'Holy Shock (enemy pressure)',     0, 0, 1, 0),
    (388, 11, 56, 'Consecration (world melee pack)', 0, 0, 1, 0),
    (390, 11, 52, 'Judgement of Wisdom (dungeon lull)', 0, 0, 1, 0);

INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    (3860, 386, 0, 0, 24275, 0, 0, 0, 'enemy_primary'),
    (3870, 387, 0, 0, 20473, 0, 0, 0, 'enemy_primary'),
    (3880, 388, 0, 0, 20116, 0, 0, 0, 'self'),
    (3900, 390, 0, 0, 20186, 0, 0, 0, 'enemy_primary');

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    -- World Judgement: only when the party is healthy and mana is comfortable.
    (3830, 383, 0, 'self',  'combat_environment',      0, 0,  'open_world'),
    (3831, 383, 1, 'self',  'mana_pct',                5, 20, ''),
    (3832, 383, 2, 'self',  'party_members_below_hp_pct', 0, 0,  '85'),
    (3833, 383, 3, 'enemy', 'distance',                3, 15, ''),

    -- World execute: if nobody urgently needs heals, finish the target.
    (3860, 386, 0, 'self',  'combat_environment',      0, 0,  'open_world'),
    (3861, 386, 1, 'enemy', 'hp_pct',                  3, 20, ''),
    (3862, 386, 2, 'self',  'mana_pct',                5, 15, ''),
    (3863, 386, 3, 'self',  'party_members_below_hp_pct', 0, 0,  '65'),

    -- Offensive Holy Shock: instant pressure during quiet world windows.
    (3870, 387, 0, 'self',  'combat_environment',      0, 0,  'open_world'),
    (3871, 387, 1, 'self',  'mana_pct',                5, 35, ''),
    (3872, 387, 2, 'self',  'party_members_below_hp_pct', 0, 0,  '80'),
    (3873, 387, 3, 'enemy', 'distance',                3, 20, ''),

    -- Consecration is a luxury button for open-world melee piles only.
    (3880, 388, 0, 'self',  'combat_environment',      0, 0,  'open_world'),
    (3881, 388, 1, 'enemy', 'nearby_enemies',          5, 2,  '10'),
    (3882, 388, 2, 'self',  'mana_pct',                5, 50, ''),
    (3883, 388, 3, 'self',  'party_members_below_hp_pct', 0, 0,  '90'),

    -- Dungeon Judgement: still possible, but only in a very safe lull.
    (3900, 390, 0, 'self',  'combat_environment',      0, 0,  'dungeon_or_raid'),
    (3901, 390, 1, 'self',  'mana_pct',                5, 55, ''),
    (3902, 390, 2, 'self',  'party_members_below_hp_pct', 0, 0,  '92'),
    (3903, 390, 3, 'enemy', 'distance',                3, 15, '');
