-- rev_living_world_061_paladin_warrior_preraid_gear_templates (world DB)
--
-- Seeds stage-0 curated level-80 assigned gear for Paladin and Warrior using
-- the WotLK Classic Icy Veins pre-raid gear pages. This migration uses the
-- race-aware staged gear template support so faction-paired catch-up items can
-- be assigned cleanly while neutral slots continue to use shared rows.
--
-- Race masks in Wrath:
--   Alliance = 1101 (Human, Dwarf, Night Elf, Gnome, Draenei)
--   Horde    =  690 (Orc, Undead, Tauren, Troll, Blood Elf)

DELETE FROM `living_world_bot_assigned_gear_template`
WHERE `loadout_key` = ''
  AND `endgame_stage` = 0
  AND (
        (`class_id` = 2 AND `spec_key` IN ('Holy', 'Protection', 'Retribution'))
     OR (`class_id` = 1 AND `spec_key` IN ('Arms', 'Fury', 'Protection'))
  );

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `race_mask`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
VALUES
-- Paladin Holy
(2, 'Holy', '', 1101, 0, 0, 47687, ''),  -- Headguard of Inner Warmth
(2, 'Holy', '',  690, 0, 0, 47686, ''),  -- Helm of Inner Warmth
(2, 'Holy', '', 1101, 0, 1, 47139, ''),  -- Wail of the Val'kyr
(2, 'Holy', '',  690, 0, 1, 47307, ''),  -- Cry of the Val'kyr
(2, 'Holy', '',    0, 0, 2, 46044, ''),  -- Observer's Mantle
(2, 'Holy', '', 1101, 0, 4, 47603, ''),  -- Merlin's Robe
(2, 'Holy', '',  690, 0, 4, 47604, ''),  -- Merlin's Robe
(2, 'Holy', '',    0, 0, 5, 50314, ''),  -- Strip of Remorse
(2, 'Holy', '',    0, 0, 6, 49891, ''),  -- Leggings of Woven Death
(2, 'Holy', '',    0, 0, 7, 49896, ''),  -- Earthsoul Boots
(2, 'Holy', '', 1101, 0, 8, 47585, ''),  -- Bejeweled Wizard's Bracers
(2, 'Holy', '',  690, 0, 8, 47586, ''),  -- Bejeweled Wizard's Bracers
(2, 'Holy', '', 1101, 0, 9, 48576, ''),  -- Turalyon's Gloves of Triumph
(2, 'Holy', '',  690, 0, 9, 48593, ''),  -- Liadrin's Gloves of Triumph
(2, 'Holy', '', 1101, 0, 10, 47223, ''), -- Ring of the Darkmender
(2, 'Holy', '',  690, 0, 10, 47278, ''), -- Circle of the Darkmender
(2, 'Holy', '',    0, 0, 11, 46046, ''), -- Nebula Band
(2, 'Holy', '',    0, 0, 12, 44255, ''), -- Darkmoon Card: Greatness (Intellect)
(2, 'Holy', '',    0, 0, 13, 46051, ''), -- Meteorite Crystal
(2, 'Holy', '', 1101, 0, 14, 47089, ''), -- Cloak of Displacement
(2, 'Holy', '',  690, 0, 14, 47291, ''), -- Shroud of Displacement
(2, 'Holy', '', 1101, 0, 15, 47193, ''), -- Misery's End
(2, 'Holy', '',  690, 0, 15, 47322, ''), -- Suffering's End
(2, 'Holy', '', 1101, 0, 16, 47079, ''), -- Bastion of Purity
(2, 'Holy', '',  690, 0, 16, 47287, ''), -- Bastion of Resolve
(2, 'Holy', '',    0, 0, 17, 40705, ''), -- Libram of Renewal

-- Paladin Protection
(2, 'Protection', '', 1101, 0, 0, 48639, ''), -- Turalyon's Faceguard of Triumph
(2, 'Protection', '',  690, 0, 0, 48659, ''), -- Liadrin's Faceguard of Triumph
(2, 'Protection', '',    0, 0, 1, 45485, ''), -- Bronze Pendant of the Vanir
(2, 'Protection', '', 1101, 0, 2, 48637, ''), -- Turalyon's Shoulderguards of Triumph
(2, 'Protection', '',  690, 0, 2, 48661, ''), -- Liadrin's Shoulderguards of Triumph
(2, 'Protection', '', 1101, 0, 4, 48641, ''), -- Turalyon's Breastplate of Triumph
(2, 'Protection', '',  690, 0, 4, 48657, ''), -- Liadrin's Breastplate of Triumph
(2, 'Protection', '',    0, 0, 5, 50991, ''), -- Verdigris Chain Belt
(2, 'Protection', '',    0, 0, 6, 49904, ''), -- Pillars of Might
(2, 'Protection', '',    0, 0, 7, 49907, ''), -- Boots of Kingly Upheaval
(2, 'Protection', '', 1101, 0, 8, 47570, ''), -- Saronite Swordbreakers
(2, 'Protection', '',  690, 0, 8, 47571, ''), -- Saronite Swordbreakers
(2, 'Protection', '', 1101, 0, 9, 48640, ''), -- Turalyon's Handguards of Triumph
(2, 'Protection', '',  690, 0, 9, 48658, ''), -- Liadrin's Handguards of Triumph
(2, 'Protection', '',    0, 0, 10, 45471, ''), -- Fate's Clutch
(2, 'Protection', '',    0, 0, 11, 47731, ''), -- Clutch of Fortification
(2, 'Protection', '',    0, 0, 12, 47080, ''), -- Satrina's Impeding Scarab
(2, 'Protection', '',    0, 0, 13, 50356, ''), -- Corroded Skeleton Key
(2, 'Protection', '',    0, 0, 14, 45496, ''), -- Titanskin Cloak
(2, 'Protection', '', 1101, 0, 15, 47148, ''), -- Stormpike Cleaver
(2, 'Protection', '',  690, 0, 15, 47314, ''), -- Hellscream Slicer
(2, 'Protection', '',    0, 0, 16, 46963, ''), -- Crystal Plated Vanguard
(2, 'Protection', '',    0, 0, 17, 47661, ''), -- Libram of Valiance

-- Paladin Retribution
(2, 'Retribution', '', 1101, 0, 0, 48609, ''), -- Turalyon's Helm of Triumph
(2, 'Retribution', '',  690, 0, 0, 48624, ''), -- Liadrin's Helm of Triumph
(2, 'Retribution', '',    0, 0, 1, 46040, ''), -- Strength of the Heavens
(2, 'Retribution', '', 1101, 0, 2, 47697, ''), -- Pauldrons of Trembling Rage
(2, 'Retribution', '',  690, 0, 2, 47696, ''), -- Shoulderplates of Trembling Rage
(2, 'Retribution', '', 1101, 0, 4, 47589, ''), -- Titanium Razorplate
(2, 'Retribution', '',  690, 0, 4, 47590, ''), -- Titanium Razorplate
(2, 'Retribution', '',    0, 0, 5, 46095, ''), -- Soul-Devouring Cinch
(2, 'Retribution', '',    0, 0, 6, 49903, ''), -- Legplates of Painful Death
(2, 'Retribution', '',    0, 0, 7, 49895, ''), -- Footpads of Impending Death
(2, 'Retribution', '', 1101, 0, 8, 47572, ''), -- Titanium Spikeguards
(2, 'Retribution', '',  690, 0, 8, 47573, ''), -- Titanium Spikeguards
(2, 'Retribution', '', 1101, 0, 9, 48608, ''), -- Turalyon's Gauntlets of Triumph
(2, 'Retribution', '',  690, 0, 9, 48625, ''), -- Liadrin's Gauntlets of Triumph
(2, 'Retribution', '',    0, 0, 10, 45534, ''), -- Seal of the Betrayed King
(2, 'Retribution', '',    0, 0, 11, 47729, ''), -- Bloodshed Band
(2, 'Retribution', '', 1101, 0, 12, 47115, ''), -- Death's Verdict
(2, 'Retribution', '',  690, 0, 12, 47303, ''), -- Death's Choice
(2, 'Retribution', '',    0, 0, 13, 46038, ''), -- Dark Matter
(2, 'Retribution', '',    0, 0, 14, 47320, ''), -- Might of the Nerub
(2, 'Retribution', '', 1101, 0, 15, 47069, ''), -- Justicebringer
(2, 'Retribution', '',  690, 0, 15, 47285, ''), -- Dual-blade Butcher
(2, 'Retribution', '',    0, 0, 17, 47661, ''), -- Libram of Valiance

-- Warrior Arms / Fury shared
(1, 'Arms', '', 1101, 0, 0, 48378, ''), -- Wrynn's Helmet of Triumph
(1, 'Arms', '',  690, 0, 0, 48393, ''), -- Hellscream's Helmet of Triumph
(1, 'Fury', '', 1101, 0, 0, 48378, ''), -- Wrynn's Helmet of Triumph
(1, 'Fury', '',  690, 0, 0, 48393, ''), -- Hellscream's Helmet of Triumph
(1, 'Arms', '',    0, 0, 1, 45945, ''), -- Seed of Budding Carnage
(1, 'Fury', '',    0, 0, 1, 45945, ''),
(1, 'Arms', '',    0, 0, 2, 46037, ''), -- Shoulderplates of the Celestial Watch
(1, 'Fury', '',    0, 0, 2, 46037, ''),
(1, 'Arms', '', 1101, 0, 4, 47589, ''), -- Titanium Razorplate
(1, 'Arms', '',  690, 0, 4, 47590, ''),
(1, 'Fury', '', 1101, 0, 4, 47589, ''),
(1, 'Fury', '',  690, 0, 4, 47590, ''),
(1, 'Arms', '',    0, 0, 5, 46095, ''), -- Soul-Devouring Cinch
(1, 'Fury', '',    0, 0, 5, 46095, ''),
(1, 'Arms', '',    0, 0, 6, 49899, ''), -- Bladeborn Leggings
(1, 'Fury', '',    0, 0, 6, 49899, ''),
(1, 'Arms', '',    0, 0, 7, 49906, ''), -- Hellfrozen Bonegrinders
(1, 'Fury', '',    0, 0, 7, 49906, ''),
(1, 'Arms', '', 1101, 0, 8, 47572, ''), -- Titanium Spikeguards
(1, 'Arms', '',  690, 0, 8, 47573, ''),
(1, 'Fury', '', 1101, 0, 8, 47572, ''),
(1, 'Fury', '',  690, 0, 8, 47573, ''),
(1, 'Arms', '', 1101, 0, 9, 48377, ''), -- Wrynn's Gauntlets of Triumph
(1, 'Arms', '',  690, 0, 9, 48392, ''), -- Hellscream's Gauntlets of Triumph
(1, 'Fury', '', 1101, 0, 9, 48377, ''),
(1, 'Fury', '',  690, 0, 9, 48392, ''),
(1, 'Arms', '',    0, 0, 10, 46048, ''), -- Band of Lights
(1, 'Fury', '',    0, 0, 10, 46048, ''),
(1, 'Arms', '',    0, 0, 11, 50271, ''), -- Band of Stained Souls
(1, 'Fury', '',    0, 0, 11, 50271, ''),
(1, 'Arms', '',    0, 0, 12, 45931, ''), -- Mjolnir Runestone
(1, 'Fury', '',    0, 0, 12, 45931, ''),
(1, 'Arms', '', 1101, 0, 13, 47115, ''), -- Death's Verdict
(1, 'Arms', '',  690, 0, 13, 47303, ''), -- Death's Choice
(1, 'Fury', '', 1101, 0, 13, 47115, ''),
(1, 'Fury', '',  690, 0, 13, 47303, ''),
(1, 'Arms', '',    0, 0, 14, 46032, ''), -- Drape of the Faceless General
(1, 'Fury', '',    0, 0, 14, 46032, ''),
(1, 'Arms', '',    0, 0, 15, 45868, ''), -- Aesir's Edge
(1, 'Fury', '',    0, 0, 15, 45868, ''),
(1, 'Fury', '',    0, 0, 16, 45868, ''), -- Aesir's Edge offhand
(1, 'Arms', '',    0, 0, 17, 45296, ''), -- Twirling Blades
(1, 'Fury', '',    0, 0, 17, 45296, ''),

-- Warrior Protection
(1, 'Protection', '', 1101, 0, 0, 48430, ''), -- Wrynn's Greathelm of Triumph
(1, 'Protection', '',  690, 0, 0, 48463, ''), -- Hellscream's Greathelm of Triumph
(1, 'Protection', '',    0, 0, 1, 45485, ''), -- Bronze Pendant of the Vanir
(1, 'Protection', '', 1101, 0, 2, 48454, ''), -- Wrynn's Pauldrons of Triumph
(1, 'Protection', '',  690, 0, 2, 48465, ''), -- Hellscream's Pauldrons of Triumph
(1, 'Protection', '', 1101, 0, 4, 48450, ''), -- Wrynn's Breastplate of Triumph
(1, 'Protection', '',  690, 0, 4, 48461, ''), -- Hellscream's Breastplate of Triumph
(1, 'Protection', '', 1101, 0, 5, 47072, ''), -- Girdle of Bloodied Scars
(1, 'Protection', '',  690, 0, 5, 47283, ''), -- Belt of Bloodied Scars
(1, 'Protection', '',    0, 0, 6, 49904, ''), -- Pillars of Might
(1, 'Protection', '',    0, 0, 7, 49907, ''), -- Boots of Kingly Upheaval
(1, 'Protection', '', 1101, 0, 8, 47570, ''), -- Saronite Swordbreakers
(1, 'Protection', '',  690, 0, 8, 47571, ''), -- Saronite Swordbreakers
(1, 'Protection', '', 1101, 0, 9, 48452, ''), -- Wrynn's Handguards of Triumph
(1, 'Protection', '',  690, 0, 9, 48462, ''), -- Hellscream's Handguards of Triumph
(1, 'Protection', '',    0, 0, 10, 45471, ''), -- Fate's Clutch
(1, 'Protection', '',    0, 0, 11, 50447, ''), -- Harbinger's Bone Band
(1, 'Protection', '',    0, 0, 12, 47216, ''), -- The Black Heart
(1, 'Protection', '',    0, 0, 13, 47080, ''), -- Satrina's Impeding Scarab
(1, 'Protection', '',    0, 0, 14, 45496, ''), -- Titanskin Cloak
(1, 'Protection', '',    0, 0, 15, 45876, ''), -- Shiver
(1, 'Protection', '',    0, 0, 16, 45877, ''), -- The Boreal Guard
(1, 'Protection', '',    0, 0, 17, 47660, ''); -- Blades of the Sable Cross
