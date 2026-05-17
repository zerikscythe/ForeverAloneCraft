-- ---------------------------------------------------------------------------
-- Holy Paladin role-based support targeting
-- ---------------------------------------------------------------------------
-- Shift Holy Paladin world-bot support away from raw owner-targeting:
--   * tank-maintenance buffs use the role-style ally_tank anchor
--   * direct heals use lowest_hp_party so injured allies can be saved while
--     still naturally preferring the tank when they are the one under pressure

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Beacon of Light (tank anchor)'
WHERE `entry_id` = 370;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Sacred Shield (tank anchor)'
WHERE `entry_id` = 372;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Shock (ally emergency)'
WHERE `entry_id` = 376;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Shock (ally while moving)'
WHERE `entry_id` = 378;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Flash of Light (ally moderate)'
WHERE `entry_id` = 380;

UPDATE `living_world_bot_combat_default_entry`
SET `label` = 'Holy Light (ally stable cast)'
WHERE `entry_id` = 382;

UPDATE `living_world_bot_combat_default_action`
SET `target_key` = 'ally_tank'
WHERE `action_id` IN (3700, 3720);

UPDATE `living_world_bot_combat_default_action`
SET `target_key` = 'lowest_hp_party'
WHERE `action_id` IN (3760, 3780, 3800, 3820);

UPDATE `living_world_bot_combat_default_condition`
SET `subject_key` = 'ally_tank'
WHERE `condition_id` IN (3700, 3701, 3720, 3721);

UPDATE `living_world_bot_combat_default_condition`
SET `subject_key` = 'lowest_hp_party'
WHERE `condition_id` IN (3760, 3761, 3780, 3782, 3800, 3801, 3820, 3821);
