-- rev_living_world_063_shaman_priest_mage_preraid_gear_templates (world DB)
--
-- Seeds stage-0 curated level-80 assigned gear for Shaman, Priest, and Mage
-- using the WotLK Classic Icy Veins pre-raid gear pages. This uses race-aware
-- template rows for faction-styled tier and paired catch-up items while keeping
-- shared neutral rows for the common slots.

DELETE FROM `living_world_bot_assigned_gear_template`
WHERE `loadout_key` = ''
  AND `endgame_stage` = 0
  AND (
        (`class_id` = 7 AND `spec_key` IN ('Elemental', 'Enhancement', 'Restoration'))
     OR (`class_id` = 5 AND `spec_key` IN ('Shadow', 'Discipline', 'Holy'))
     OR (`class_id` = 8 AND `spec_key` IN ('Arcane', 'Fire', 'Frost'))
  );

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `race_mask`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
VALUES
-- Shaman Elemental
(7, 'Elemental', '', 1101, 0, 0, 48318, ''), -- Nobundo's Helm of Triumph
(7, 'Elemental', '', 690, 0, 0, 48333, ''),  -- Thrall's Helm of Triumph
(7, 'Elemental', '', 0, 0, 1, 45133, ''),    -- Pendant of Fiery Havoc
(7, 'Elemental', '', 1101, 0, 2, 48320, ''), -- Nobundo's Shoulderpads of Triumph
(7, 'Elemental', '', 690, 0, 2, 48331, ''),  -- Thrall's Shoulderpads of Triumph
(7, 'Elemental', '', 1101, 0, 4, 48316, ''), -- Nobundo's Hauberk of Triumph
(7, 'Elemental', '', 690, 0, 4, 48335, ''),  -- Thrall's Hauberk of Triumph
(7, 'Elemental', '', 1101, 0, 5, 47081, ''), -- Cord of Biting Cold
(7, 'Elemental', '', 690, 0, 5, 47286, ''),  -- Belt of Biting Cold
(7, 'Elemental', '', 0, 0, 6, 49891, ''),    -- Leggings of Woven Death
(7, 'Elemental', '', 0, 0, 7, 49896, ''),    -- Earthsoul Boots
(7, 'Elemental', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(7, 'Elemental', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(7, 'Elemental', '', 1101, 0, 9, 48317, ''), -- Nobundo's Gloves of Triumph
(7, 'Elemental', '', 690, 0, 9, 48334, ''),  -- Thrall's Gloves of Triumph
(7, 'Elemental', '', 0, 0, 10, 45495, ''),   -- Conductive Seal
(7, 'Elemental', '', 0, 0, 11, 46046, ''),   -- Nebula Band
(7, 'Elemental', '', 0, 0, 12, 47316, ''),   -- Reign of the Dead
(7, 'Elemental', '', 0, 0, 13, 45518, ''),   -- Flare of the Heavens
(7, 'Elemental', '', 0, 0, 14, 45242, ''),   -- Drape of Mortal Downfall
(7, 'Elemental', '', 0, 0, 15, 46035, ''),   -- Aesuga, Hand of the Ardent Champion
(7, 'Elemental', '', 1101, 0, 16, 47079, ''),-- Bastion of Purity
(7, 'Elemental', '', 690, 0, 16, 47287, ''), -- Bastion of Resolve
(7, 'Elemental', '', 0, 0, 17, 47666, ''),   -- Totem of Electrifying Wind

-- Shaman Enhancement
(7, 'Enhancement', '', 1101, 0, 0, 48348, ''), -- Nobundo's Faceguard of Triumph
(7, 'Enhancement', '', 690, 0, 0, 48363, ''),  -- Thrall's Faceguard of Triumph
(7, 'Enhancement', '', 0, 0, 1, 45133, ''),    -- Pendant of Fiery Havoc
(7, 'Enhancement', '', 1101, 0, 2, 48320, ''), -- Nobundo's Shoulderpads of Triumph
(7, 'Enhancement', '', 690, 0, 2, 48331, ''),  -- Thrall's Shoulderpads of Triumph
(7, 'Enhancement', '', 1101, 0, 4, 48346, ''), -- Nobundo's Chestguard of Triumph
(7, 'Enhancement', '', 690, 0, 4, 48365, ''),  -- Thrall's Chestguard of Triumph
(7, 'Enhancement', '', 1101, 0, 5, 47107, ''), -- Belt of the Merciless Killer
(7, 'Enhancement', '', 690, 0, 5, 47299, ''),  -- Belt of the Pitiless Killer
(7, 'Enhancement', '', 0, 0, 6, 49901, ''),    -- Draconic Bonesplinter Legguards
(7, 'Enhancement', '', 0, 0, 7, 49897, ''),    -- Rock-Steady Treads
(7, 'Enhancement', '', 1101, 0, 8, 47576, ''), -- Crusader's Dragonscale Bracers
(7, 'Enhancement', '', 690, 0, 8, 47577, ''),  -- Crusader's Dragonscale Bracers
(7, 'Enhancement', '', 1101, 0, 9, 48347, ''), -- Nobundo's Grips of Triumph
(7, 'Enhancement', '', 690, 0, 9, 48364, ''),  -- Thrall's Grips of Triumph
(7, 'Enhancement', '', 0, 0, 10, 47282, ''),   -- Band of Callous Aggression
(7, 'Enhancement', '', 0, 0, 11, 46046, ''),   -- Nebula Band
(7, 'Enhancement', '', 0, 0, 12, 47316, ''),   -- Reign of the Dead
(7, 'Enhancement', '', 0, 0, 13, 45609, ''),   -- Comet's Trail
(7, 'Enhancement', '', 0, 0, 14, 45461, ''),   -- Drape of Icy Intent
(7, 'Enhancement', '', 0, 0, 15, 46035, ''),   -- Aesuga, Hand of the Ardent Champion
(7, 'Enhancement', '', 0, 0, 16, 46097, ''),   -- Caress of Insanity
(7, 'Enhancement', '', 0, 0, 17, 47666, ''),   -- Totem of Electrifying Wind

-- Shaman Restoration
(7, 'Restoration', '', 0, 0, 0, 46201, ''),    -- Conqueror's Worldbreaker Headpiece
(7, 'Restoration', '', 0, 0, 1, 45443, ''),    -- Charm of Meticulous Timing
(7, 'Restoration', '', 0, 0, 2, 45404, ''),    -- Valorous Worldbreaker Spaulders
(7, 'Restoration', '', 0, 0, 4, 46198, ''),    -- Conqueror's Worldbreaker Tunic
(7, 'Restoration', '', 1101, 0, 5, 46990, ''), -- Belt of the Ice Burrower
(7, 'Restoration', '', 690, 0, 5, 47265, ''),  -- Binding of the Ice Burrower
(7, 'Restoration', '', 0, 0, 6, 49891, ''),    -- Leggings of Woven Death
(7, 'Restoration', '', 0, 0, 7, 49896, ''),    -- Earthsoul Boots
(7, 'Restoration', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(7, 'Restoration', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(7, 'Restoration', '', 0, 0, 9, 45401, ''),    -- Valorous Worldbreaker Handguards
(7, 'Restoration', '', 1101, 0, 10, 47223, ''),-- Ring of the Darkmender
(7, 'Restoration', '', 690, 0, 10, 47278, ''), -- Circle of the Darkmender
(7, 'Restoration', '', 0, 0, 11, 46046, ''),   -- Nebula Band
(7, 'Restoration', '', 0, 0, 12, 47041, ''),   -- Solace of the Defeated
(7, 'Restoration', '', 0, 0, 13, 45535, ''),   -- Show of Faith
(7, 'Restoration', '', 0, 0, 14, 45486, ''),   -- Drape of the Sullen Goddess
(7, 'Restoration', '', 0, 0, 15, 46035, ''),   -- Aesuga, Hand of the Ardent Champion
(7, 'Restoration', '', 1101, 0, 16, 47079, ''),-- Bastion of Purity
(7, 'Restoration', '', 690, 0, 16, 47287, ''), -- Bastion of Resolve
(7, 'Restoration', '', 0, 0, 17, 47665, ''),   -- Totem of Calming Tides

-- Priest Shadow
(5, 'Shadow', '', 1101, 0, 0, 48078, ''), -- Velen's Circlet of Triumph
(5, 'Shadow', '', 690, 0, 0, 48095, ''),  -- Zabra's Circlet of Triumph
(5, 'Shadow', '', 0, 0, 1, 45133, ''),    -- Pendant of Fiery Havoc
(5, 'Shadow', '', 0, 0, 2, 46068, ''),    -- Amice of Inconceivable Horror
(5, 'Shadow', '', 0, 0, 4, 47603, ''),    -- Merlin's Robe
(5, 'Shadow', '', 0, 0, 5, 45557, ''),    -- Sash of Ancient Power
(5, 'Shadow', '', 0, 0, 6, 49891, ''),    -- Leggings of Woven Death
(5, 'Shadow', '', 0, 0, 7, 49890, ''),    -- Deathfrost Boots
(5, 'Shadow', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(5, 'Shadow', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(5, 'Shadow', '', 1101, 0, 9, 48077, ''), -- Velen's Handwraps of Triumph
(5, 'Shadow', '', 690, 0, 9, 48096, ''),  -- Zabra's Handwraps of Triumph
(5, 'Shadow', '', 0, 0, 10, 46046, ''),   -- Nebula Band
(5, 'Shadow', '', 0, 0, 11, 45297, ''),   -- Shimmering Seal
(5, 'Shadow', '', 0, 0, 12, 47182, ''),   -- Reign of the Unliving
(5, 'Shadow', '', 0, 0, 13, 45518, ''),   -- Flare of the Heavens
(5, 'Shadow', '', 0, 0, 14, 46042, ''),   -- Drape of the Messenger
(5, 'Shadow', '', 0, 0, 15, 45886, ''),   -- Icecore Staff
(5, 'Shadow', '', 0, 0, 17, 45294, ''),   -- Petrified Ivy Sprig

-- Priest Discipline
(5, 'Discipline', '', 0, 0, 0, 46197, ''),    -- Conqueror's Cowl of Sanctification
(5, 'Discipline', '', 0, 0, 1, 46047, ''),    -- Pendant of the Somber Witness
(5, 'Discipline', '', 0, 0, 2, 45390, ''),    -- Valorous Shoulderpads of Sanctification
(5, 'Discipline', '', 0, 0, 4, 46193, ''),    -- Conqueror's Robe of Sanctification
(5, 'Discipline', '', 0, 0, 5, 45558, ''),    -- Cord of the White Dawn
(5, 'Discipline', '', 0, 0, 6, 49892, ''),    -- Lightweave Leggings
(5, 'Discipline', '', 0, 0, 7, 49893, ''),    -- Sandals of Consecration
(5, 'Discipline', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(5, 'Discipline', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(5, 'Discipline', '', 0, 0, 9, 45387, ''),    -- Valorous Gloves of Sanctification
(5, 'Discipline', '', 0, 0, 10, 46096, ''),   -- Signet of Soft Lament
(5, 'Discipline', '', 0, 0, 11, 47733, ''),   -- Heartmender Circle
(5, 'Discipline', '', 0, 0, 12, 46051, ''),   -- Meteorite Crystal
(5, 'Discipline', '', 0, 0, 13, 47041, ''),   -- Solace of the Defeated
(5, 'Discipline', '', 1101, 0, 14, 47225, ''),-- Maiden's Favor
(5, 'Discipline', '', 690, 0, 14, 47328, ''), -- Maiden's Adoration
(5, 'Discipline', '', 0, 0, 15, 45886, ''),   -- Icecore Staff
(5, 'Discipline', '', 0, 0, 17, 45294, ''),   -- Petrified Ivy Sprig

-- Priest Holy
(5, 'Holy', '', 1101, 0, 0, 47984, ''), -- Velen's Cowl of Triumph
(5, 'Holy', '', 690, 0, 0, 48065, ''),  -- Zabra's Cowl of Triumph
(5, 'Holy', '', 0, 0, 1, 45447, ''),    -- Watchful Eye of Fate
(5, 'Holy', '', 0, 0, 2, 46068, ''),    -- Amice of Inconceivable Horror
(5, 'Holy', '', 0, 0, 4, 47603, ''),    -- Merlin's Robe
(5, 'Holy', '', 0, 0, 5, 45558, ''),    -- Cord of the White Dawn
(5, 'Holy', '', 0, 0, 6, 49891, ''),    -- Leggings of Woven Death
(5, 'Holy', '', 0, 0, 7, 49893, ''),    -- Sandals of Consecration
(5, 'Holy', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(5, 'Holy', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(5, 'Holy', '', 1101, 0, 9, 47983, ''), -- Velen's Gloves of Triumph
(5, 'Holy', '', 690, 0, 9, 48066, ''),  -- Zabra's Gloves of Triumph
(5, 'Holy', '', 0, 0, 10, 45946, ''),   -- Fire Orchid Signet
(5, 'Holy', '', 0, 0, 11, 47732, ''),   -- Band of the Invoker
(5, 'Holy', '', 0, 0, 12, 46051, ''),   -- Meteorite Crystal
(5, 'Holy', '', 0, 0, 13, 47271, ''),   -- Solace of the Fallen
(5, 'Holy', '', 1101, 0, 14, 46976, ''),-- Shawl of the Refreshing Winds
(5, 'Holy', '', 690, 0, 14, 47256, ''), -- Drape of the Refreshing Winds
(5, 'Holy', '', 0, 0, 15, 45886, ''),   -- Icecore Staff
(5, 'Holy', '', 0, 0, 17, 45294, ''),   -- Petrified Ivy Sprig

-- Mage Arcane
(8, 'Arcane', '', 0, 0, 0, 50276, ''),    -- Bloodmage Hood
(8, 'Arcane', '', 0, 0, 1, 45243, ''),    -- Sapphire Amulet of Renewal
(8, 'Arcane', '', 0, 0, 2, 50279, ''),    -- Bloodmage Shoulderpads
(8, 'Arcane', '', 0, 0, 4, 50278, ''),    -- Bloodmage Robe
(8, 'Arcane', '', 0, 0, 5, 47258, ''),    -- Belt of the Tenebrous Mist
(8, 'Arcane', '', 0, 0, 6, 50277, ''),    -- Bloodmage Leggings
(8, 'Arcane', '', 0, 0, 7, 49890, ''),    -- Deathfrost Boots
(8, 'Arcane', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(8, 'Arcane', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(8, 'Arcane', '', 1101, 0, 9, 47752, ''), -- Khadgar's Gauntlets of Conquest
(8, 'Arcane', '', 690, 0, 9, 47773, ''),  -- Sunstrider's Gauntlets of Conquest
(8, 'Arcane', '', 0, 0, 10, 45495, ''),   -- Conductive Seal
(8, 'Arcane', '', 0, 0, 11, 51557, ''),   -- Runed Signet of the Kirin Tor
(8, 'Arcane', '', 0, 0, 12, 45518, ''),   -- Flare of the Heavens
(8, 'Arcane', '', 0, 0, 13, 48724, ''),   -- Talisman of Resurgence
(8, 'Arcane', '', 0, 0, 14, 47256, ''),   -- Drape of the Refreshing Winds
(8, 'Arcane', '', 0, 0, 15, 50047, ''),   -- Quel'Delar, Lens of the Mind
(8, 'Arcane', '', 0, 0, 16, 47276, ''),   -- Talisman of Heedless Sins
(8, 'Arcane', '', 0, 0, 17, 45294, ''),   -- Petrified Ivy Sprig

-- Mage Fire
(8, 'Fire', '', 0, 0, 0, 50276, ''),    -- Bloodmage Hood
(8, 'Fire', '', 0, 0, 1, 45933, ''),    -- Pendant of the Shallow Grave
(8, 'Fire', '', 0, 0, 2, 50279, ''),    -- Bloodmage Shoulderpads
(8, 'Fire', '', 0, 0, 4, 47603, ''),    -- Merlin's Robe
(8, 'Fire', '', 0, 0, 5, 47286, ''),    -- Belt of Biting Cold
(8, 'Fire', '', 0, 0, 6, 50277, ''),    -- Bloodmage Leggings
(8, 'Fire', '', 0, 0, 7, 49890, ''),    -- Deathfrost Boots
(8, 'Fire', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(8, 'Fire', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(8, 'Fire', '', 0, 0, 9, 50275, ''),    -- Bloodmage Gloves
(8, 'Fire', '', 0, 0, 10, 51557, ''),   -- Runed Signet of the Kirin Tor
(8, 'Fire', '', 0, 0, 11, 46046, ''),   -- Nebula Band
(8, 'Fire', '', 0, 0, 12, 45518, ''),   -- Flare of the Heavens
(8, 'Fire', '', 0, 0, 13, 47316, ''),   -- Reign of the Dead
(8, 'Fire', '', 0, 0, 14, 46042, ''),   -- Drape of the Messenger
(8, 'Fire', '', 0, 0, 15, 50047, ''),   -- Quel'Delar, Lens of the Mind
(8, 'Fire', '', 0, 0, 16, 47276, ''),   -- Talisman of Heedless Sins
(8, 'Fire', '', 0, 0, 17, 45294, ''),   -- Petrified Ivy Sprig

-- Mage Frost
(8, 'Frost', '', 0, 0, 0, 50276, ''),    -- Bloodmage Hood
(8, 'Frost', '', 0, 0, 1, 45933, ''),    -- Pendant of the Shallow Grave
(8, 'Frost', '', 0, 0, 2, 50279, ''),    -- Bloodmage Shoulderpads
(8, 'Frost', '', 0, 0, 4, 47603, ''),    -- Merlin's Robe
(8, 'Frost', '', 0, 0, 5, 47286, ''),    -- Belt of Biting Cold
(8, 'Frost', '', 0, 0, 6, 50277, ''),    -- Bloodmage Leggings
(8, 'Frost', '', 0, 0, 7, 49890, ''),    -- Deathfrost Boots
(8, 'Frost', '', 1101, 0, 8, 47585, ''), -- Bejeweled Wizard's Bracers
(8, 'Frost', '', 690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(8, 'Frost', '', 0, 0, 9, 50275, ''),    -- Bloodmage Gloves
(8, 'Frost', '', 0, 0, 10, 51557, ''),   -- Runed Signet of the Kirin Tor
(8, 'Frost', '', 0, 0, 11, 46046, ''),   -- Nebula Band
(8, 'Frost', '', 0, 0, 12, 45518, ''),   -- Flare of the Heavens
(8, 'Frost', '', 0, 0, 13, 39229, ''),   -- Embrace of the Spider
(8, 'Frost', '', 0, 0, 14, 46042, ''),   -- Drape of the Messenger
(8, 'Frost', '', 0, 0, 15, 50047, ''),   -- Quel'Delar, Lens of the Mind
(8, 'Frost', '', 0, 0, 16, 50309, ''),   -- Shriveled Heart
(8, 'Frost', '', 0, 0, 17, 50472, '');   -- Nightmare Ender
