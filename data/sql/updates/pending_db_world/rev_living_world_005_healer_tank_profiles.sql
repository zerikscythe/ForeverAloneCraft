-- LivingWorld: default healer and tank combat profiles (profiles 11-15).
-- Enum encodings match rev_living_world_002_runtime_and_profiles.sql.
--   BotCombatActionType:        0=Spell 1=Item
--   BotCombatRankMode:          0=BestKnown 1=ExactSpellId 2=SpecificRank
--   BotCombatConditionLogic:    0=All 1=Any
--   BotCombatConditionOperator: 0=Equal 1=NotEqual 2=LessThan 3=LessThanOrEqual
--                               4=GreaterThan 5=GreaterThanOrEqual 6=Has 7=NotHas 8=Exists
--
-- Profile mapping:
--   11  Holy:HEAL       Priest (Flash Heal / Renew / SWP / Mind Blast)
--                       Paladin Holy (Flash of Light / Holy Light / Holy Shock / Judgement)
--   12  Restoration:HEAL Shaman (Lesser Healing Wave / Healing Wave / Flame Shock / Earth Shock)
--                        Druid  (Healing Touch / Rejuvenation / Moonfire / Wrath)
--   13  Protection:TANK Warrior (Shield Slam / Revenge / Devastate / Thunder Clap / Heroic Strike)
--                       Paladin (Avenger's Shield / Hammer of Righteous / SoR / Consecration / Judgement)
--   14  Blood:TANK      Death Knight Blood
--   15  Feral:TANK      Druid Feral (bear form)
--
-- Entry IDs 49-73.  Action IDs = entry_id*10+slot.  Condition IDs = entry_id*10+seq.
-- Healing entries target 'lowest_hp_party'; offense/tank entries target 'enemy_primary'.
-- Primary/secondary action pairs cover two classes sharing one profile: the prep service
-- promotes secondary to primary if the bot does not know the primary spell.

-- -----------------------------------------------------------------------
-- DEFAULT PROFILES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_profile` (
    `default_profile_id`, `spec_key`, `role_key`, `display_name`,
    `conservation_mode`, `mana_low_water`, `mana_high_water`,
    `enable_down_rank`, `down_rank_floor`,
    `default_aoe_mode`, `default_aoe_min_targets`, `default_aoe_scan_radius`
) VALUES
    (11, 'Holy',         'HEAL', 'Holy Healer (Priest / Paladin)',      1, 40, 65, 1, 2, 0, 2, 10),
    (12, 'Restoration',  'HEAL', 'Restoration Healer (Shaman / Druid)', 1, 40, 65, 1, 2, 0, 2, 10),
    (13, 'Protection',   'TANK', 'Protection Tank (Warrior / Paladin)', 0,  0, 100, 0, 0, 0, 2, 10),
    (14, 'Blood',        'TANK', 'Blood Tank (Death Knight)',           0,  0, 100, 0, 0, 0, 2, 10),
    (15, 'Feral',        'TANK', 'Feral Tank (Druid Bear)',             0,  0, 100, 0, 0, 0, 2, 10)
ON DUPLICATE KEY UPDATE
    `display_name`          = VALUES(`display_name`),
    `conservation_mode`     = VALUES(`conservation_mode`),
    `mana_low_water`        = VALUES(`mana_low_water`),
    `mana_high_water`       = VALUES(`mana_high_water`),
    `enable_down_rank`      = VALUES(`enable_down_rank`),
    `down_rank_floor`       = VALUES(`down_rank_floor`),
    `default_aoe_mode`      = VALUES(`default_aoe_mode`),
    `default_aoe_min_targets` = VALUES(`default_aoe_min_targets`),
    `default_aoe_scan_radius` = VALUES(`default_aoe_scan_radius`);

-- -----------------------------------------------------------------------
-- ENTRIES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`,
     `is_interrupt`, `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    -- Profile 11: Holy:HEAL
    -- Healing entries use lowest_hp_party (party-aware); offense uses enemy_primary.
    (49, 11,  5, 'Emergency Heal',    0, 0, 1, 0),  -- Flash Heal / Flash of Light when target <= 40%
    (50, 11, 20, 'Moderate Heal',     0, 0, 1, 0),  -- Renew / Holy Light when target <= 80%
    (51, 11, 40, 'Shadow Word Pain',  0, 0, 1, 0),  -- SWP DoT (Priest)
    (52, 11, 50, 'Mind Blast',        0, 0, 1, 0),  -- Mind Blast nuke (Priest)
    (53, 11, 55, 'Holy Shock',        0, 0, 1, 0),  -- Holy Shock damage (Paladin)
    (54, 11, 60, 'Judgement',         0, 0, 1, 0),  -- Judgement (Paladin)
    (55, 11, 65, 'Smite',             0, 0, 1, 0),  -- Smite filler (Priest)

    -- Profile 12: Restoration:HEAL
    (56, 12,  5, 'Emergency Heal',    0, 0, 1, 0),  -- Lesser Healing Wave / Healing Touch when target <= 40%
    (57, 12, 20, 'Moderate Heal',     0, 0, 1, 0),  -- Healing Wave / Rejuvenation when target <= 80%
    (58, 12, 40, 'DoT / Opener',      0, 0, 1, 0),  -- Flame Shock (Shaman) / Moonfire (Druid)
    (59, 12, 50, 'Nuke Filler',       0, 0, 1, 0),  -- Earth Shock (Shaman) / Wrath (Druid)

    -- Profile 13: Protection:TANK
    (60, 13,  0, 'Shield Slam / Avenger Shield',      0, 0, 1, 0),
    (61, 13, 10, 'Revenge / Hammer of Righteous',     0, 0, 1, 0),
    (62, 13, 20, 'Devastate / Shield of Righteous',   0, 0, 1, 0),
    (63, 13, 30, 'Thunder Clap / Consecration',       0, 0, 1, 0),
    (64, 13, 40, 'Heroic Strike / Judgement',         0, 0, 1, 0),

    -- Profile 14: Blood:TANK
    (65, 14,  0, 'Icy Touch',         0, 0, 1, 0),  -- apply Frost Fever
    (66, 14,  5, 'Plague Strike',     0, 0, 1, 0),  -- apply Blood Plague
    (67, 14, 10, 'Death Strike',      0, 0, 1, 0),  -- primary damage + self-heal
    (68, 14, 15, 'Heart Strike',      0, 0, 1, 0),  -- blood rune spender
    (69, 14, 20, 'Rune Strike',       0, 0, 1, 0),  -- off-GCD proc spender
    (70, 14, 25, 'Rune Tap',          0, 0, 1, 0),  -- self-heal when below 80%

    -- Profile 15: Feral:TANK
    (71, 15,  0, 'Mangle Bear',       0, 0, 1, 0),
    (72, 15, 10, 'Lacerate',          0, 0, 1, 0),
    (73, 15, 20, 'Swipe Bear',        0, 0, 1, 0)

ON DUPLICATE KEY UPDATE
    `default_profile_id`  = VALUES(`default_profile_id`),
    `priority`            = VALUES(`priority`),
    `label`               = VALUES(`label`),
    `is_interrupt`        = VALUES(`is_interrupt`),
    `breaks_current_cast` = VALUES(`breaks_current_cast`),
    `enabled`             = VALUES(`enabled`),
    `condition_logic`     = VALUES(`condition_logic`);

-- -----------------------------------------------------------------------
-- ACTIONS  (action_id = entry_id*10 + slot)
-- Healing actions use target_key='lowest_hp_party'.
-- Primary/secondary pairs let one entry cover two classes:
--   the prep service promotes secondary to primary when the bot lacks the primary spell.
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    -- Profile 11: Holy:HEAL
    -- Entry 49: Flash Heal (Priest) / Flash of Light (Paladin)
    (490, 49, 0, 0,  2061, 0, 0, 0, 'lowest_hp_party'),  -- Flash Heal
    (491, 49, 1, 0, 19750, 0, 0, 0, 'lowest_hp_party'),  -- Flash of Light
    -- Entry 50: Renew HoT (Priest) / Holy Light (Paladin)
    (500, 50, 0, 0,   139, 0, 0, 0, 'lowest_hp_party'),  -- Renew
    (501, 50, 1, 0,   635, 0, 0, 0, 'lowest_hp_party'),  -- Holy Light
    -- Entry 51: Shadow Word: Pain
    (510, 51, 0, 0,   589, 0, 0, 0, 'enemy_primary'),    -- Shadow Word: Pain
    -- Entry 52: Mind Blast
    (520, 52, 0, 0,  8092, 0, 0, 0, 'enemy_primary'),    -- Mind Blast
    -- Entry 53: Holy Shock (Paladin talent)
    (530, 53, 0, 0, 20473, 0, 0, 0, 'enemy_primary'),    -- Holy Shock
    -- Entry 54: Judgement (Paladin)
    (540, 54, 0, 0, 20271, 0, 0, 0, 'enemy_primary'),    -- Judgement
    -- Entry 55: Smite filler (Priest)
    (550, 55, 0, 0,   585, 0, 0, 0, 'enemy_primary'),    -- Smite

    -- Profile 12: Restoration:HEAL
    -- Entry 56: Lesser Healing Wave (Shaman) / Healing Touch (Druid)
    (560, 56, 0, 0,  8004, 0, 0, 0, 'lowest_hp_party'),  -- Lesser Healing Wave
    (561, 56, 1, 0,  5185, 0, 0, 0, 'lowest_hp_party'),  -- Healing Touch
    -- Entry 57: Healing Wave (Shaman) / Rejuvenation (Druid)
    (570, 57, 0, 0,   331, 0, 0, 0, 'lowest_hp_party'),  -- Healing Wave
    (571, 57, 1, 0,   774, 0, 0, 0, 'lowest_hp_party'),  -- Rejuvenation
    -- Entry 58: Flame Shock (Shaman) / Moonfire (Druid)
    (580, 58, 0, 0,  8050, 0, 0, 0, 'enemy_primary'),    -- Flame Shock
    (581, 58, 1, 0,  8921, 0, 0, 0, 'enemy_primary'),    -- Moonfire
    -- Entry 59: Earth Shock (Shaman) / Wrath (Druid)
    (590, 59, 0, 0,  8042, 0, 0, 0, 'enemy_primary'),    -- Earth Shock
    (591, 59, 1, 0,  5176, 0, 0, 0, 'enemy_primary'),    -- Wrath

    -- Profile 13: Protection:TANK
    -- Entry 60: Shield Slam (Warrior) / Avenger's Shield (Paladin)
    (600, 60, 0, 0, 23922, 0, 0, 0, 'enemy_primary'),    -- Shield Slam
    (601, 60, 1, 0, 31935, 0, 0, 0, 'enemy_primary'),    -- Avenger's Shield
    -- Entry 61: Revenge (Warrior) / Hammer of the Righteous (Paladin)
    (610, 61, 0, 0,  6572, 0, 0, 0, 'enemy_primary'),    -- Revenge
    (611, 61, 1, 0, 53595, 0, 0, 0, 'enemy_primary'),    -- Hammer of the Righteous
    -- Entry 62: Devastate (Warrior) / Shield of Righteousness (Paladin)
    (620, 62, 0, 0, 20243, 0, 0, 0, 'enemy_primary'),    -- Devastate
    (621, 62, 1, 0, 61411, 0, 1, 0, 'enemy_primary'),    -- Shield of Righteousness (ExactSpellId)
    -- Entry 63: Thunder Clap (Warrior) / Consecration (Paladin, self-centered AoE)
    (630, 63, 0, 0,  6343, 0, 0, 0, 'enemy_primary'),    -- Thunder Clap
    (631, 63, 1, 0, 20116, 0, 0, 0, 'self'),             -- Consecration (cast on self)
    -- Entry 64: Heroic Strike (Warrior) / Judgement (Paladin)
    (640, 64, 0, 0,    78, 0, 0, 0, 'enemy_primary'),    -- Heroic Strike
    (641, 64, 1, 0, 20271, 0, 0, 0, 'enemy_primary'),    -- Judgement

    -- Profile 14: Blood:TANK
    (650, 65, 0, 0, 45477, 0, 1, 0, 'enemy_primary'),    -- Icy Touch (ExactSpellId rank 1)
    (660, 66, 0, 0, 45462, 0, 1, 0, 'enemy_primary'),    -- Plague Strike (ExactSpellId rank 1)
    (670, 67, 0, 0, 49998, 0, 1, 0, 'enemy_primary'),    -- Death Strike (ExactSpellId)
    (680, 68, 0, 0, 55050, 0, 0, 0, 'enemy_primary'),    -- Heart Strike
    (690, 69, 0, 0, 56815, 0, 1, 0, 'enemy_primary'),    -- Rune Strike (ExactSpellId)
    (700, 70, 0, 0, 48982, 0, 1, 0, 'self'),             -- Rune Tap (ExactSpellId, self-heal)

    -- Profile 15: Feral:TANK
    (710, 71, 0, 0, 33878, 0, 0, 0, 'enemy_primary'),    -- Mangle (Bear)
    (720, 72, 0, 0, 33745, 0, 0, 0, 'enemy_primary'),    -- Lacerate
    (730, 73, 0, 0,   779, 0, 0, 0, 'enemy_primary')     -- Swipe (Bear)

ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `slot`          = VALUES(`slot`),
    `action_type`   = VALUES(`action_type`),
    `spell_base_id` = VALUES(`spell_base_id`),
    `item_id`       = VALUES(`item_id`),
    `rank_mode`     = VALUES(`rank_mode`),
    `rank_value`    = VALUES(`rank_value`),
    `target_key`    = VALUES(`target_key`);

-- -----------------------------------------------------------------------
-- CONDITIONS  (condition_id = entry_id*10 + sequence)
-- Healing entries gate on target HP so spells only fire when someone needs them.
-- DK disease entries gate on the enemy not already having the debuff.
-- Rune Tap gates on low self HP so it is only used reactively.
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    -- Profile 11: Holy:HEAL
    -- Entry 49: only heal when the lowest-HP party member is critically low
    (490, 49, 0, 'lowest_hp_party', 'hp_pct', 3, 40.0, ''),
    -- Entry 50: only heal when the lowest-HP party member needs topping off
    (500, 50, 0, 'lowest_hp_party', 'hp_pct', 3, 80.0, ''),

    -- Profile 12: Restoration:HEAL
    -- Entry 56: emergency heal threshold
    (560, 56, 0, 'lowest_hp_party', 'hp_pct', 3, 40.0, ''),
    -- Entry 57: moderate heal threshold
    (570, 57, 0, 'lowest_hp_party', 'hp_pct', 3, 80.0, ''),

    -- Profile 14: Blood:TANK
    -- Entry 65: Icy Touch only when enemy does not already have Frost Fever (aura 55095)
    (650, 65, 0, 'enemy', 'aura', 7, 55095.0, ''),
    -- Entry 66: Plague Strike only when enemy does not already have Blood Plague (aura 55078)
    (660, 66, 0, 'enemy', 'aura', 7, 55078.0, ''),
    -- Entry 70: Rune Tap only when self HP is at or below 80%
    (700, 70, 0, 'self', 'hp_pct', 3, 80.0, '')

ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `sequence`      = VALUES(`sequence`),
    `subject_key`   = VALUES(`subject_key`),
    `stat_key`      = VALUES(`stat_key`),
    `comparison`    = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value`  = VALUES(`string_value`);
