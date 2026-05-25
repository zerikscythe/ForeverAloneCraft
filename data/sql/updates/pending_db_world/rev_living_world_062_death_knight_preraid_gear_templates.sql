-- rev_living_world_062_death_knight_preraid_gear_templates (world DB)
--
-- Seeds stage-0 curated level-80 assigned gear for Death Knight using the
-- WotLK Classic Icy Veins pre-raid gear pages. Blood tanking gets its own set,
-- while Frost and Unholy receive their DPS variants with faction-themed paired
-- slots where the source page presents them.

DELETE FROM `living_world_bot_assigned_gear_template`
WHERE `class_id` = 6
  AND `loadout_key` = ''
  AND `endgame_stage` = 0
  AND `spec_key` IN ('Blood', 'Frost', 'Unholy');

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `race_mask`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
VALUES
-- Death Knight Blood
(6, 'Blood', '', 0, 0, 0, 50855, ''),   -- Scourgelord Faceguard
(6, 'Blood', '', 0, 0, 1, 45485, ''),   -- Bronze Pendant of the Vanir
(6, 'Blood', '', 0, 0, 2, 50853, ''),   -- Scourgelord Pauldrons
(6, 'Blood', '', 0, 0, 4, 50968, ''),   -- Cataclysmic Chestguard
(6, 'Blood', '', 0, 0, 5, 50991, ''),   -- Verdigris Chain Belt
(6, 'Blood', '', 0, 0, 6, 49904, ''),   -- Pillars of Might
(6, 'Blood', '', 0, 0, 7, 49907, ''),   -- Boots of Kingly Upheaval
(6, 'Blood', '', 1101, 0, 8, 47570, ''),-- Saronite Swordbreakers
(6, 'Blood', '', 690, 0, 8, 47571, ''), -- Saronite Swordbreakers
(6, 'Blood', '', 0, 0, 9, 50978, ''),   -- Gauntlets of the Kraken
(6, 'Blood', '', 0, 0, 10, 45471, ''),  -- Fate's Clutch
(6, 'Blood', '', 0, 0, 11, 47731, ''),  -- Clutch of Fortification
(6, 'Blood', '', 0, 0, 12, 47080, ''),  -- Satrina's Impeding Scarab
(6, 'Blood', '', 0, 0, 13, 50356, ''),  -- Corroded Skeleton Key
(6, 'Blood', '', 0, 0, 14, 45496, ''),  -- Titanskin Cloak
(6, 'Blood', '', 0, 0, 15, 46067, ''),  -- Hammer of Crushing Whispers
(6, 'Blood', '', 0, 0, 17, 50462, ''),  -- Sigil of the Bone Gryphon

-- Death Knight Frost
(6, 'Frost', '', 1101, 0, 0, 48472, ''), -- Thassarian's Helmet of Conquest
(6, 'Frost', '', 690, 0, 0, 48503, ''),  -- Koltira's Helmet of Conquest
(6, 'Frost', '', 0, 0, 1, 45459, ''),    -- Frigid Strength of Hodir
(6, 'Frost', '', 1101, 0, 2, 48478, ''), -- Thassarian's Shoulderplates of Conquest
(6, 'Frost', '', 690, 0, 2, 48505, ''),  -- Koltira's Shoulderplates of Conquest
(6, 'Frost', '', 1101, 0, 4, 48474, ''), -- Thassarian's Battleplate of Conquest
(6, 'Frost', '', 690, 0, 4, 48501, ''),  -- Koltira's Battleplate of Conquest
(6, 'Frost', '', 1101, 0, 5, 46999, ''), -- Bloodbath Belt
(6, 'Frost', '', 690, 0, 5, 47268, ''),  -- Bloodbath Girdle
(6, 'Frost', '', 0, 0, 6, 45982, ''),    -- Fused Alloy Legplates
(6, 'Frost', '', 1101, 0, 7, 47150, ''), -- Greaves of the 7th Legion
(6, 'Frost', '', 690, 0, 7, 47312, ''),  -- Greaves of the Saronite Citadel
(6, 'Frost', '', 0, 0, 8, 47253, ''),    -- Boneshatter Vambraces
(6, 'Frost', '', 1101, 0, 9, 48480, ''), -- Thassarian's Gauntlets of Conquest
(6, 'Frost', '', 690, 0, 9, 48502, ''),  -- Koltira's Gauntlets of Conquest
(6, 'Frost', '', 1101, 0, 10, 46959, ''),-- Band of the Violent Temperment
(6, 'Frost', '', 690, 0, 10, 47252, ''), -- Ring of the Violent Temperament
(6, 'Frost', '', 0, 0, 11, 45534, ''),   -- Seal of the Betrayed King
(6, 'Frost', '', 690, 0, 12, 47303, ''), -- Death's Choice
(6, 'Frost', '', 1101, 0, 12, 47115, ''),-- Death's Verdict
(6, 'Frost', '', 0, 0, 13, 45931, ''),   -- Mjolnir Runestone
(6, 'Frost', '', 0, 0, 14, 47320, ''),   -- Might of the Nerub
(6, 'Frost', '', 0, 0, 15, 46097, ''),   -- Caress of Insanity
(6, 'Frost', '', 0, 0, 16, 46097, ''),   -- Caress of Insanity
(6, 'Frost', '', 0, 0, 17, 40207, ''),   -- Sigil of Awareness

-- Death Knight Unholy
(6, 'Unholy', '', 1101, 0, 0, 48472, ''), -- Thassarian's Helmet of Conquest
(6, 'Unholy', '', 690, 0, 0, 48503, ''),  -- Koltira's Helmet of Conquest
(6, 'Unholy', '', 0, 0, 1, 45459, ''),    -- Frigid Strength of Hodir
(6, 'Unholy', '', 1101, 0, 2, 48478, ''), -- Thassarian's Shoulderplates of Conquest
(6, 'Unholy', '', 690, 0, 2, 48505, ''),  -- Koltira's Shoulderplates of Conquest
(6, 'Unholy', '', 1101, 0, 4, 48474, ''), -- Thassarian's Battleplate of Conquest
(6, 'Unholy', '', 690, 0, 4, 48501, ''),  -- Koltira's Battleplate of Conquest
(6, 'Unholy', '', 1101, 0, 5, 46999, ''), -- Bloodbath Belt
(6, 'Unholy', '', 690, 0, 5, 47268, ''),  -- Bloodbath Girdle
(6, 'Unholy', '', 0, 0, 6, 45982, ''),    -- Fused Alloy Legplates
(6, 'Unholy', '', 1101, 0, 7, 47150, ''), -- Greaves of the 7th Legion
(6, 'Unholy', '', 690, 0, 7, 47312, ''),  -- Greaves of the Saronite Citadel
(6, 'Unholy', '', 0, 0, 8, 47253, ''),    -- Boneshatter Vambraces
(6, 'Unholy', '', 1101, 0, 9, 48480, ''), -- Thassarian's Gauntlets of Conquest
(6, 'Unholy', '', 690, 0, 9, 48502, ''),  -- Koltira's Gauntlets of Conquest
(6, 'Unholy', '', 1101, 0, 10, 46959, ''),-- Band of the Violent Temperment
(6, 'Unholy', '', 690, 0, 10, 47252, ''), -- Ring of the Violent Temperament
(6, 'Unholy', '', 0, 0, 11, 45534, ''),   -- Seal of the Betrayed King
(6, 'Unholy', '', 690, 0, 12, 47303, ''), -- Death's Choice
(6, 'Unholy', '', 1101, 0, 12, 47115, ''),-- Death's Verdict
(6, 'Unholy', '', 0, 0, 13, 45609, ''),   -- Comet's Trail
(6, 'Unholy', '', 0, 0, 14, 47320, ''),   -- Might of the Nerub
(6, 'Unholy', '', 0, 0, 15, 46097, ''),   -- Caress of Insanity
(6, 'Unholy', '', 0, 0, 16, 46097, ''),   -- Caress of Insanity
(6, 'Unholy', '', 0, 0, 17, 47673, '');   -- Sigil of Virulence
