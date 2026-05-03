-- LivingWorld: default combat profile rotation entries for all 10 starter class doctrines.
-- Enum encodings (match rev_living_world_002_runtime_and_profiles.sql):
--   BotCombatActionType:      0=Spell 1=Item
--   BotCombatRankMode:        0=BestKnown 1=ExactSpellId 2=SpecificRank
--   BotCombatConditionLogic:  0=All 1=Any
--   BotCombatConditionOperator: 0=Equal 1=NotEqual 2=LessThan 3=LessThanOrEqual
--                               4=GreaterThan 5=GreaterThanOrEqual 6=Has 7=NotHas 8=Exists
--
-- Entry IDs 1-48, action IDs entry*10+slot, condition IDs entry*10.
-- All rotation entries target enemy_primary unless otherwise noted.
-- DoT spells refresh on re-cast in WotLK so aura conditions are omitted on first pass.
-- combo_points condition (stat_key='combo_points') requires BotCombatRuntimeEvaluator
-- support added alongside this SQL (v004 patch).

-- -----------------------------------------------------------------------
-- ENTRIES
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_entry`
    (`entry_id`, `default_profile_id`, `priority`, `label`, `is_interrupt`,
     `breaks_current_cast`, `enabled`, `condition_logic`)
VALUES
    -- Profile 1: Warrior Arms DPS
    ( 1, 1,  0, 'Execute',       0, 0, 1, 0),
    ( 2, 1, 10, 'Mortal Strike', 0, 0, 1, 0),
    ( 3, 1, 20, 'Rend',          0, 0, 1, 0),
    ( 4, 1, 25, 'Slam',          0, 0, 1, 0),
    ( 5, 1, 30, 'Heroic Strike', 0, 0, 1, 0),

    -- Profile 2: Paladin Retribution DPS
    ( 6, 2,  0, 'Hammer of Wrath', 0, 0, 1, 0),
    ( 7, 2, 10, 'Crusader Strike', 0, 0, 1, 0),
    ( 8, 2, 20, 'Judgement',       0, 0, 1, 0),
    ( 9, 2, 30, 'Divine Storm',    0, 0, 1, 0),
    (10, 2, 40, 'Consecration',    0, 0, 1, 0),

    -- Profile 3: Hunter Beast Mastery DPS
    (11, 3,  0, 'Serpent Sting', 0, 0, 1, 0),
    (12, 3, 10, 'Multi-Shot',    0, 0, 1, 0),
    (13, 3, 20, 'Steady Shot',   0, 0, 1, 0),

    -- Profile 4: Rogue Combat DPS
    (14, 4,  0, 'Eviscerate (4+ CP)',      0, 0, 1, 0),
    (15, 4, 10, 'Slice and Dice (2+ CP)',  0, 0, 1, 0),
    (16, 4, 20, 'Sinister Strike',         0, 0, 1, 0),
    (17, 4, 25, 'Hemorrhage',              0, 0, 1, 0),

    -- Profile 5: Priest Shadow DPS
    (18, 5,  0, 'Vampiric Touch',   0, 0, 1, 0),
    (19, 5,  5, 'Shadow Word Pain', 0, 0, 1, 0),
    (20, 5, 10, 'Devouring Plague', 0, 0, 1, 0),
    (21, 5, 20, 'Mind Blast',       0, 0, 1, 0),
    (22, 5, 30, 'Mind Flay',        0, 0, 1, 0),

    -- Profile 6: Death Knight Unholy DPS
    (23, 6,  0, 'Icy Touch',     0, 0, 1, 0),
    (24, 6,  5, 'Plague Strike', 0, 0, 1, 0),
    (25, 6, 10, 'Scourge Strike', 0, 0, 1, 0),
    (26, 6, 20, 'Death Strike',  0, 0, 1, 0),
    (27, 6, 30, 'Death Coil',    0, 0, 1, 0),

    -- Profile 7: Shaman Elemental DPS
    (28, 7,  0, 'Flame Shock',       0, 0, 1, 0),
    (29, 7, 10, 'Lava Burst',        0, 0, 1, 0),
    (30, 7, 20, 'Chain Lightning',   0, 0, 1, 0),
    (31, 7, 30, 'Lightning Bolt',    0, 0, 1, 0),
    (32, 7, 40, 'Earth Shock',       0, 0, 1, 0),

    -- Profile 8: Mage Frost DPS
    (33, 8,  0, 'Frostfire Bolt',   0, 0, 1, 0),
    (34, 8, 10, 'Frostbolt',        0, 0, 1, 0),
    (35, 8, 20, 'Ice Lance',        0, 0, 1, 0),
    (36, 8, 30, 'Arcane Missiles',  0, 0, 1, 0),
    (37, 8, 40, 'Fire Blast',       0, 0, 1, 0),

    -- Profile 9: Warlock Affliction DPS
    (38, 9,  0, 'Haunt',                0, 0, 1, 0),
    (39, 9,  5, 'Unstable Affliction',  0, 0, 1, 0),
    (40, 9, 10, 'Curse of Agony',       0, 0, 1, 0),
    (41, 9, 15, 'Corruption',           0, 0, 1, 0),
    (42, 9, 20, 'Immolate',             0, 0, 1, 0),
    (43, 9, 30, 'Shadow Bolt',          0, 0, 1, 0),

    -- Profile 10: Druid Balance DPS
    (44, 10,  0, 'Starsurge',     0, 0, 1, 0),
    (45, 10,  5, 'Moonfire',      0, 0, 1, 0),
    (46, 10, 10, 'Insect Swarm',  0, 0, 1, 0),
    (47, 10, 20, 'Starfire',      0, 0, 1, 0),
    (48, 10, 30, 'Wrath',         0, 0, 1, 0)

ON DUPLICATE KEY UPDATE
    `default_profile_id`    = VALUES(`default_profile_id`),
    `priority`              = VALUES(`priority`),
    `label`                 = VALUES(`label`),
    `is_interrupt`          = VALUES(`is_interrupt`),
    `breaks_current_cast`   = VALUES(`breaks_current_cast`),
    `enabled`               = VALUES(`enabled`),
    `condition_logic`       = VALUES(`condition_logic`);

-- -----------------------------------------------------------------------
-- ACTIONS  (action_id = entry_id*10 + slot)
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_action`
    (`action_id`, `entry_id`, `slot`, `action_type`, `spell_base_id`, `item_id`,
     `rank_mode`, `rank_value`, `target_key`)
VALUES
    -- Warrior Arms
    (10,  1, 0, 0,  5308, 0, 0, 0, 'enemy_primary'),  -- Execute
    (20,  2, 0, 0, 12294, 0, 0, 0, 'enemy_primary'),  -- Mortal Strike
    (21,  2, 1, 0, 23881, 0, 0, 0, 'enemy_primary'),  -- Bloodthirst (secondary)
    (30,  3, 0, 0,   772, 0, 0, 0, 'enemy_primary'),  -- Rend
    (40,  4, 0, 0,  1464, 0, 0, 0, 'enemy_primary'),  -- Slam
    (50,  5, 0, 0,    78, 0, 0, 0, 'enemy_primary'),  -- Heroic Strike

    -- Paladin Retribution
    (60,  6, 0, 0, 24275, 0, 0, 0, 'enemy_primary'),  -- Hammer of Wrath
    (70,  7, 0, 0, 35395, 0, 1, 0, 'enemy_primary'),  -- Crusader Strike (ExactSpellId)
    (80,  8, 0, 0, 20271, 0, 0, 0, 'enemy_primary'),  -- Judgement of Light
    (90,  9, 0, 0, 53385, 0, 1, 0, 'enemy_primary'),  -- Divine Storm (ExactSpellId)
    (100,10, 0, 0, 20116, 0, 0, 0, 'enemy_primary'),  -- Consecration

    -- Hunter Beast Mastery
    (110,11, 0, 0,  1978, 0, 0, 0, 'enemy_primary'),  -- Serpent Sting
    (120,12, 0, 0,  2643, 0, 0, 0, 'enemy_primary'),  -- Multi-Shot
    (130,13, 0, 0, 34120, 0, 1, 0, 'enemy_primary'),  -- Steady Shot (ExactSpellId)
    (131,13, 1, 0,  3044, 0, 0, 0, 'enemy_primary'),  -- Arcane Shot (secondary)

    -- Rogue Combat
    (140,14, 0, 0,  2098, 0, 0, 0, 'enemy_primary'),  -- Eviscerate
    (150,15, 0, 0,  5171, 0, 0, 0, 'enemy_primary'),  -- Slice and Dice
    (160,16, 0, 0,  1752, 0, 0, 0, 'enemy_primary'),  -- Sinister Strike
    (170,17, 0, 0, 16511, 0, 1, 0, 'enemy_primary'),  -- Hemorrhage (ExactSpellId)

    -- Priest Shadow
    (180,18, 0, 0, 34914, 0, 0, 0, 'enemy_primary'),  -- Vampiric Touch
    (190,19, 0, 0,   589, 0, 0, 0, 'enemy_primary'),  -- Shadow Word: Pain
    (200,20, 0, 0,  2944, 0, 0, 0, 'enemy_primary'),  -- Devouring Plague
    (210,21, 0, 0,  8092, 0, 0, 0, 'enemy_primary'),  -- Mind Blast
    (220,22, 0, 0, 15407, 0, 0, 0, 'enemy_primary'),  -- Mind Flay
    (221,22, 1, 0,   585, 0, 0, 0, 'enemy_primary'),  -- Smite (secondary)

    -- Death Knight Unholy
    (230,23, 0, 0, 45477, 0, 1, 0, 'enemy_primary'),  -- Icy Touch (ExactSpellId)
    (240,24, 0, 0, 45462, 0, 1, 0, 'enemy_primary'),  -- Plague Strike (ExactSpellId)
    (250,25, 0, 0, 55090, 0, 0, 0, 'enemy_primary'),  -- Scourge Strike
    (260,26, 0, 0, 49998, 0, 1, 0, 'enemy_primary'),  -- Death Strike (ExactSpellId)
    (261,26, 1, 0, 55050, 0, 0, 0, 'enemy_primary'),  -- Heart Strike (secondary)
    (270,27, 0, 0, 47541, 0, 0, 0, 'enemy_primary'),  -- Death Coil

    -- Shaman Elemental
    (280,28, 0, 0,  8050, 0, 0, 0, 'enemy_primary'),  -- Flame Shock
    (290,29, 0, 0, 51505, 0, 1, 0, 'enemy_primary'),  -- Lava Burst (ExactSpellId)
    (300,30, 0, 0,   421, 0, 0, 0, 'enemy_primary'),  -- Chain Lightning
    (310,31, 0, 0,   403, 0, 0, 0, 'enemy_primary'),  -- Lightning Bolt
    (320,32, 0, 0,  8042, 0, 0, 0, 'enemy_primary'),  -- Earth Shock

    -- Mage Frost
    (330,33, 0, 0, 44614, 0, 1, 0, 'enemy_primary'),  -- Frostfire Bolt (ExactSpellId)
    (340,34, 0, 0,   116, 0, 0, 0, 'enemy_primary'),  -- Frostbolt
    (341,34, 1, 0,   133, 0, 0, 0, 'enemy_primary'),  -- Fireball (secondary, pre-Frostbolt levels)
    (350,35, 0, 0, 30455, 0, 0, 0, 'enemy_primary'),  -- Ice Lance
    (360,36, 0, 0,  5143, 0, 0, 0, 'enemy_primary'),  -- Arcane Missiles
    (370,37, 0, 0,  2136, 0, 0, 0, 'enemy_primary'),  -- Fire Blast

    -- Warlock Affliction
    (380,38, 0, 0, 48181, 0, 1, 0, 'enemy_primary'),  -- Haunt (ExactSpellId)
    (390,39, 0, 0, 30108, 0, 0, 0, 'enemy_primary'),  -- Unstable Affliction
    (400,40, 0, 0,   980, 0, 0, 0, 'enemy_primary'),  -- Curse of Agony
    (410,41, 0, 0,   172, 0, 0, 0, 'enemy_primary'),  -- Corruption
    (420,42, 0, 0,   348, 0, 0, 0, 'enemy_primary'),  -- Immolate
    (430,43, 0, 0,   686, 0, 0, 0, 'enemy_primary'),  -- Shadow Bolt

    -- Druid Balance
    (440,44, 0, 0, 49455, 0, 0, 0, 'enemy_primary'),  -- Starsurge
    (450,45, 0, 0,  8921, 0, 0, 0, 'enemy_primary'),  -- Moonfire
    (460,46, 0, 0,  5570, 0, 0, 0, 'enemy_primary'),  -- Insect Swarm
    (470,47, 0, 0,  2912, 0, 0, 0, 'enemy_primary'),  -- Starfire
    (480,48, 0, 0,  5176, 0, 0, 0, 'enemy_primary')   -- Wrath

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
-- CONDITIONS  (condition_id = entry_id*10, sequence=0)
-- Only entries that have meaningful gating conditions.
-- stat_key 'combo_points' requires BotCombatRuntimeEvaluator v004 patch.
-- stat_key 'aura' with comparison=7 (NotHas) gates DK disease applications.
-- -----------------------------------------------------------------------
INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    -- Warrior: Execute only when enemy hp <= 20%
    (10,  1, 0, 'enemy', 'hp_pct', 3, 20.0, ''),

    -- Paladin: Hammer of Wrath only when enemy hp <= 20%
    (60,  6, 0, 'enemy', 'hp_pct', 3, 20.0, ''),

    -- Rogue: Eviscerate only when bot has 4+ combo points
    (140, 14, 0, 'self', 'combo_points', 5, 4.0, ''),

    -- Rogue: Slice and Dice only when bot has 2+ combo points
    (150, 15, 0, 'self', 'combo_points', 5, 2.0, ''),

    -- DK: Icy Touch only when target does not have Frost Fever (aura 55095)
    (230, 23, 0, 'enemy', 'aura', 7, 55095.0, ''),

    -- DK: Plague Strike only when target does not have Blood Plague (aura 55078)
    (240, 24, 0, 'enemy', 'aura', 7, 55078.0, '')

ON DUPLICATE KEY UPDATE
    `entry_id`      = VALUES(`entry_id`),
    `sequence`      = VALUES(`sequence`),
    `subject_key`   = VALUES(`subject_key`),
    `stat_key`      = VALUES(`stat_key`),
    `comparison`    = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value`  = VALUES(`string_value`);
