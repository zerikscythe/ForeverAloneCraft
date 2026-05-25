-- rev_living_world_058_hunter_warlock_preraid_gear_templates (world DB)
--
-- Seeds stage-0 curated level-80 assigned gear for Hunter and Warlock from the
-- spec-specific Icy Veins WotLK Classic pre-raid gear pages. These are the
-- "fresh 80" endgame templates that should be assigned immediately when a bot
-- first reaches level 80 before later stage markers upgrade the loadout.

DELETE FROM `living_world_bot_assigned_gear_template`
WHERE `loadout_key` = ''
  AND `endgame_stage` = 0
  AND (
        (`class_id` = 3 AND `spec_key` IN ('BeastMastery', 'Marksmanship', 'Survival'))
     OR (`class_id` = 9 AND `spec_key` IN ('Affliction', 'Demonology', 'Destruction'))
  );

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
SELECT 3, spec_key, '', 0, slot_id, item_id, ''
FROM
(
    SELECT 'BeastMastery' AS spec_key
    UNION ALL SELECT 'Marksmanship'
    UNION ALL SELECT 'Survival'
) AS specs
CROSS JOIN
(
    SELECT 0 AS slot_id, 45610 AS item_id  -- Boundless Gaze
    UNION ALL SELECT 1, 45517              -- Pendulum of Infinity
    UNION ALL SELECT 2, 45300              -- Mantle of Fiery Vengeance
    UNION ALL SELECT 4, 45473              -- Embrace of the Gladiator
    UNION ALL SELECT 6, 45536              -- Legguards of Cunning Deception
    UNION ALL SELECT 7, 45244              -- Greaves of Swift Vengeance
    UNION ALL SELECT 8, 47281              -- Bracers of the Silent Massacre
    UNION ALL SELECT 9, 45444              -- Gloves of the Steady Hand
    UNION ALL SELECT 10, 45608             -- Brann's Signet Ring
    UNION ALL SELECT 11, 47070             -- Ring of Callous Aggression
    UNION ALL SELECT 12, 47115             -- Death's Verdict
    UNION ALL SELECT 13, 46038             -- Dark Matter
    UNION ALL SELECT 14, 46032             -- Drape of the Faceless General
    UNION ALL SELECT 15, 45613             -- Dreambinder
    UNION ALL SELECT 17, 45570             -- Skyforge Crossbow
    UNION ALL SELECT 5, 47311              -- Waistguard of Deathly Dominion
) AS slots;

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
SELECT 9, spec_key, '', 0, slot_id, item_id, ''
FROM
(
    SELECT 'Affliction' AS spec_key
    UNION ALL SELECT 'Demonology'
    UNION ALL SELECT 'Destruction'
) AS specs
CROSS JOIN
(
    SELECT 0 AS slot_id, 51765 AS item_id  -- Dark Coven Hood
    UNION ALL SELECT 1, 45133              -- Pendant of Fiery Havoc
    UNION ALL SELECT 2, 50244              -- Dark Coven Shoulderpads
    UNION ALL SELECT 4, 50243              -- Dark Coven Robe
    UNION ALL SELECT 5, 47258              -- Belt of the Tenebrous Mist
    UNION ALL SELECT 6, 49891              -- Leggings of Woven Death
    UNION ALL SELECT 7, 49893              -- Sandals of Consecration
    UNION ALL SELECT 8, 47585              -- Bejeweled Wizard's Bracers
    UNION ALL SELECT 9, 51766              -- Dark Coven Gloves
    UNION ALL SELECT 10, 51557             -- Runed Signet of the Kirin Tor
    UNION ALL SELECT 11, 45495             -- Conductive Seal
    UNION ALL SELECT 12, 45518             -- Flare of the Heavens
    UNION ALL SELECT 13, 47316             -- Reign of the Dead
    UNION ALL SELECT 14, 45242             -- Drape of Mortal Downfall
    UNION ALL SELECT 15, 47261             -- Barb of Tarasque
    UNION ALL SELECT 17, 45294             -- Petrified Ivy Sprig
) AS slots;
