-- rev_living_world_059_rogue_preraid_gear_templates (world DB)
--
-- Seeds stage-0 curated level-80 assigned gear for Rogue using the WotLK
-- Classic Icy Veins pre-raid gear pages. Combat and Subtlety share the same
-- first-pass stage-0 set; Assassination gets its own dagger-focused weapon and
-- trinket configuration.

DELETE FROM `living_world_bot_assigned_gear_template`
WHERE `class_id` = 4
  AND `loadout_key` = ''
  AND `endgame_stage` = 0
  AND `spec_key` IN ('Combat', 'Assassination', 'Subtlety');

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
SELECT 4, spec_key, '', 0, slot_id, item_id, ''
FROM
(
    SELECT 'Combat' AS spec_key
    UNION ALL SELECT 'Subtlety'
) AS specs
CROSS JOIN
(
    SELECT 0 AS slot_id, 48225 AS item_id  -- VanCleef's Helmet of Triumph
    UNION ALL SELECT 1, 45517              -- Pendulum of Infinity
    UNION ALL SELECT 2, 47708              -- Duskstalker Shoulderpads
    UNION ALL SELECT 4, 48223              -- VanCleef's Breastplate of Triumph
    UNION ALL SELECT 5, 46095              -- Soul-Devouring Cinch
    UNION ALL SELECT 6, 48226              -- VanCleef's Legplates of Triumph
    UNION ALL SELECT 7, 47071              -- Treads of the Icewalker
    UNION ALL SELECT 8, 47151              -- Bracers of Dark Determination
    UNION ALL SELECT 9, 48224              -- VanCleef's Gauntlets of Triumph
    UNION ALL SELECT 10, 46048             -- Band of Lights
    UNION ALL SELECT 11, 47070             -- Ring of Callous Aggression
    UNION ALL SELECT 12, 47303             -- Death's Choice
    UNION ALL SELECT 13, 45931             -- Mjolnir Runestone
    UNION ALL SELECT 14, 45461             -- Drape of Icy Intent
    UNION ALL SELECT 15, 47148             -- Stormpike Cleaver
    UNION ALL SELECT 16, 46036             -- Void Sabre
    UNION ALL SELECT 17, 45296             -- Twirling Blades
) AS slots;

INSERT INTO `living_world_bot_assigned_gear_template`
    (`class_id`, `spec_key`, `loadout_key`, `endgame_stage`, `slot_id`, `item_id`, `enchantments`)
SELECT 4, 'Assassination', '', 0, slot_id, item_id, ''
FROM
(
    SELECT 0 AS slot_id, 48225 AS item_id  -- VanCleef's Helmet of Triumph
    UNION ALL SELECT 1, 45517              -- Pendulum of Infinity
    UNION ALL SELECT 2, 47708              -- Duskstalker Shoulderpads
    UNION ALL SELECT 4, 48223              -- VanCleef's Breastplate of Triumph
    UNION ALL SELECT 5, 46095              -- Soul-Devouring Cinch
    UNION ALL SELECT 6, 48226              -- VanCleef's Legplates of Triumph
    UNION ALL SELECT 7, 47071              -- Treads of the Icewalker
    UNION ALL SELECT 8, 47151              -- Bracers of Dark Determination
    UNION ALL SELECT 9, 48224              -- VanCleef's Gauntlets of Triumph
    UNION ALL SELECT 10, 46048             -- Band of Lights
    UNION ALL SELECT 11, 47070             -- Ring of Callous Aggression
    UNION ALL SELECT 12, 47303             -- Death's Choice
    UNION ALL SELECT 13, 45609             -- Comet's Trail
    UNION ALL SELECT 14, 45461             -- Drape of Icy Intent
    UNION ALL SELECT 15, 45930             -- Combatant's Bootblade
    UNION ALL SELECT 16, 45930             -- Combatant's Bootblade
    UNION ALL SELECT 17, 45296             -- Twirling Blades
) AS slots;
