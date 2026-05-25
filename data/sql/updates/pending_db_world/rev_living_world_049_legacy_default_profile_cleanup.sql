-- rev_living_world_049_legacy_default_profile_cleanup (world DB)
--
-- Paladin (`033`) marked the start of the fresh doctrine generation.
-- This cleanup removes the older starter/default profile rows and their
-- dependent entry/action/condition scaffolding, leaving only the modernized
-- profile families created from `033` onward.
--
-- Fresh profile keep-set:
--   1  Warrior Arms
--   2  Paladin Retribution
--   4  Rogue Combat
--   6  Death Knight Unholy
--   7  Shaman Elemental
--   11 Paladin Holy
--   13 Warrior Protection
--   14 Death Knight Blood
--   19 Warrior Fury
--   22 Rogue Assassination
--   23 Rogue Subtlety
--   25 Shaman Enhancement
--   33 Shaman Restoration
--   34 Death Knight Frost
--   35 Paladin Protection
--
-- Intentionally removed here:
--   early starter/default scaffolding from `003/004/005/007/027`
--   partial/legacy families that will be rebuilt cleanly later

-- -----------------------------------------------------------------------
-- REMOVE DEPENDENT ENTRY/ACTION/CONDITION ROWS FOR LEGACY PROFILES
-- -----------------------------------------------------------------------
DELETE `c`
FROM `living_world_bot_combat_default_condition` `c`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `c`.`entry_id`
WHERE `e`.`default_profile_id` NOT IN (1, 2, 4, 6, 7, 11, 13, 14, 19, 22, 23, 25, 33, 34, 35);

DELETE `a`
FROM `living_world_bot_combat_default_action` `a`
INNER JOIN `living_world_bot_combat_default_entry` `e`
        ON `e`.`entry_id` = `a`.`entry_id`
WHERE `e`.`default_profile_id` NOT IN (1, 2, 4, 6, 7, 11, 13, 14, 19, 22, 23, 25, 33, 34, 35);

DELETE FROM `living_world_bot_combat_default_entry`
WHERE `default_profile_id` NOT IN (1, 2, 4, 6, 7, 11, 13, 14, 19, 22, 23, 25, 33, 34, 35);

-- -----------------------------------------------------------------------
-- REMOVE LEGACY PROFILE ROWS
-- -----------------------------------------------------------------------
DELETE FROM `living_world_bot_combat_default_profile`
WHERE `default_profile_id` NOT IN (1, 2, 4, 6, 7, 11, 13, 14, 19, 22, 23, 25, 33, 34, 35);

-- -----------------------------------------------------------------------
-- ORPHAN SAFETY SWEEP
-- -----------------------------------------------------------------------
DELETE `c`
FROM `living_world_bot_combat_default_condition` `c`
LEFT JOIN `living_world_bot_combat_default_entry` `e`
       ON `e`.`entry_id` = `c`.`entry_id`
WHERE `e`.`entry_id` IS NULL;

DELETE `a`
FROM `living_world_bot_combat_default_action` `a`
LEFT JOIN `living_world_bot_combat_default_entry` `e`
       ON `e`.`entry_id` = `a`.`entry_id`
WHERE `e`.`entry_id` IS NULL;
