-- Seed placeholder characters for BotHouse001-010 accounts.
-- Copied from character guid=1 (Tester, account=1, Blood Elf Mage level 1).
-- These are placeholders; clone materialization will overwrite them.
--
-- Char guid assignment: 3-12 (BotHouse001-010, accounts 2-11)
-- Item guid assignment: 23-82 (6 items x 10 chars, sequential)

-- -------------------------------------------------------------------------
-- characters
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `characters`
    (guid, account, name, race, class, gender, level, xp, money,
     skin, face, hairStyle, hairColor, facialStyle,
     bankSlots, restState, playerFlags,
     position_x, position_y, position_z, map, instance_id, instance_mode_mask, orientation,
     taximask, online, cinematic, totaltime, leveltime, logout_time,
     is_logout_resting, rest_bonus, resettalents_cost, resettalents_time,
     trans_x, trans_y, trans_z, trans_o, transguid,
     extra_flags, stable_slots, at_login, zone, death_expire_time, taxi_path,
     arenaPoints, totalHonorPoints, todayHonorPoints, yesterdayHonorPoints,
     totalKills, todayKills, yesterdayKills, chosenTitle, knownCurrencies,
     watchedFaction, drunk, health, power1, power2, power3, power4, power5, power6, power7,
     latency, talentGroupsCount, activeTalentGroup,
     exploredZones, equipmentCache, ammoId, knownTitles, actionBars, grantableLevels,
     creation_date, innTriggerId)
SELECT
     3 + bot.n, 2 + bot.n, bot.name,
     race, class, gender, level, xp, money,
     skin, face, hairStyle, hairColor, facialStyle,
     bankSlots, restState, playerFlags,
     position_x, position_y, position_z, map, 0, 0, orientation,
     taximask, 0, cinematic, 0, 0, 0,
     0, 0, 0, 0,
     0, 0, 0, 0, 0,
     extra_flags, stable_slots, at_login, zone, 0, '',
     0, 0, 0, 0, 0, 0, 0, 0, 0,
     watchedFaction, 0, health, power1, power2, power3, power4, power5, power6, power7,
     0, talentGroupsCount, activeTalentGroup,
     exploredZones, equipmentCache, ammoId, knownTitles, actionBars, 0,
     NOW(), 0
FROM `characters`
JOIN (
    SELECT 0 AS n, 'Botone'   AS name UNION ALL
    SELECT 1,      'Bottwo'          UNION ALL
    SELECT 2,      'Botthree'        UNION ALL
    SELECT 3,      'Botfour'         UNION ALL
    SELECT 4,      'Botfive'         UNION ALL
    SELECT 5,      'Botsix'          UNION ALL
    SELECT 6,      'Botseven'        UNION ALL
    SELECT 7,      'Boteight'        UNION ALL
    SELECT 8,      'Botnine'         UNION ALL
    SELECT 9,      'Botten'
) AS bot ON 1 = 1
WHERE `characters`.guid = 1;

-- -------------------------------------------------------------------------
-- character_skills
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `character_skills` (guid, skill, value, max)
SELECT 3 + bot.n, skill, value, max
FROM `character_skills`
JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
      UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) bot ON 1 = 1
WHERE `character_skills`.guid = 1;

-- -------------------------------------------------------------------------
-- character_action
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `character_action` (guid, spec, button, action, type)
SELECT 3 + bot.n, spec, button, action, type
FROM `character_action`
JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
      UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) bot ON 1 = 1
WHERE `character_action`.guid = 1;

-- -------------------------------------------------------------------------
-- character_homebind
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `character_homebind` (guid, mapId, zoneId, posX, posY, posZ)
SELECT 3 + bot.n, mapId, zoneId, posX, posY, posZ
FROM `character_homebind`
JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
      UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) bot ON 1 = 1
WHERE `character_homebind`.guid = 1;

-- -------------------------------------------------------------------------
-- character_reputation
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `character_reputation` (guid, faction, standing, flags)
SELECT 3 + bot.n, faction, standing, flags
FROM `character_reputation`
JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
      UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) bot ON 1 = 1
WHERE `character_reputation`.guid = 1;

-- -------------------------------------------------------------------------
-- item_instance  (6 items per bot, guids 23-82)
-- Rank 0-5 computed in subquery (6 source rows only), then broadcast via bot join.
-- Formula: new_guid = 23 + (bot.n * 6) + rank
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `item_instance`
    (guid, itemEntry, owner_guid, creatorGuid, giftCreatorGuid,
     count, duration, charges, flags, enchantments,
     randomPropertyId, durability, playedTime, text)
SELECT
    23 + (bot.n * 6) + src.item_rank,
    src.itemEntry,
    3 + bot.n,
    0, 0,
    src.count, src.duration, src.charges, src.flags, src.enchantments,
    src.randomPropertyId, src.durability, 0, src.text
FROM (
    SELECT
        ii.itemEntry, ii.count, ii.duration, ii.charges, ii.flags,
        ii.enchantments, ii.randomPropertyId, ii.durability, ii.text,
        ROW_NUMBER() OVER (ORDER BY ci.slot) - 1 AS item_rank
    FROM item_instance ii
    JOIN character_inventory ci ON ci.item = ii.guid
    WHERE ii.owner_guid = 1
) AS src
JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
      UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) bot ON 1 = 1;

-- -------------------------------------------------------------------------
-- character_inventory  (6 slots per bot, item guids mapped to 23-82)
-- slot_rank computed in subquery so the bot JOIN doesn't affect it.
-- -------------------------------------------------------------------------
INSERT IGNORE INTO `character_inventory` (guid, bag, slot, item)
SELECT
    3 + bot.n,
    src.bag,
    src.slot,
    23 + (bot.n * 6) + src.item_rank
FROM (
    SELECT
        ci.bag, ci.slot,
        ROW_NUMBER() OVER (ORDER BY ci.slot) - 1 AS item_rank
    FROM character_inventory ci
    WHERE ci.guid = 1
) AS src
JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
      UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) bot ON 1 = 1;
