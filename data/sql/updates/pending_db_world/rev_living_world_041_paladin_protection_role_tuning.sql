-- rev_living_world_041_paladin_protection_role_tuning (world DB)
--
-- Focused audit pass for Paladin Protection after the Ret/Holy cleanup.
-- The goal is not to turn tanks into PvP duelists; it is to make the doctrine
-- read more like a tank role:
--   * peel and protect allies more aggressively
--   * bias toward hostile DPS over tunnel-visioning the wrong target
--   * stop wasting taunt-style globals in open-world / PvP skirmish contexts
--   * keep the dungeon/raid regain-threat tool intact where it still matters

-- -----------------------------------------------------------------------
-- PROFILE-LEVEL TARGETING POLICY
-- -----------------------------------------------------------------------
-- Tanks should assist and peel hard, focus targets already being pressured by
-- allies, and generally prefer enemy DPS over tanks. We keep the profile in
-- Assist mode, but tune the biases so the role reads as "frontline control"
-- instead of "generic Paladin with tank stats".
UPDATE `living_world_bot_combat_default_profile`
SET
    `targeting_mode`      = 1,
    `current_target_bias` = 95,
    `assist_target_bias`  = 185,
    `focus_fire_bias`     = 85,
    `protect_ally_bias`   = 280,
    `prefer_healer_bias`  = 90,
    `prefer_dps_bias`     = 260,
    `avoid_tank_bias`     = 220
WHERE `default_profile_id` = 35;

-- -----------------------------------------------------------------------
-- WORLD VS DUNGEON THREAT TOOLS
-- -----------------------------------------------------------------------
-- Hand of Reckoning is a real regain-threat tool in PvE, but in open-world
-- player-like skirmishes it is usually just a bad global. Gate it to dungeon /
-- raid contexts so tanks stop spending time on a taunt-shaped spell in world
-- fights.
DELETE FROM `living_world_bot_combat_default_condition`
WHERE `condition_id` = 3961;

INSERT INTO `living_world_bot_combat_default_condition`
    (`condition_id`, `entry_id`, `sequence`, `subject_key`, `stat_key`,
     `comparison`, `numeric_value`, `string_value`)
VALUES
    (3961, 396, 1, 'self', 'combat_environment', 0, 0.0, 'dungeon_or_raid')
ON DUPLICATE KEY UPDATE
    `entry_id` = VALUES(`entry_id`),
    `sequence` = VALUES(`sequence`),
    `subject_key` = VALUES(`subject_key`),
    `stat_key` = VALUES(`stat_key`),
    `comparison` = VALUES(`comparison`),
    `numeric_value` = VALUES(`numeric_value`),
    `string_value` = VALUES(`string_value`);

-- -----------------------------------------------------------------------
-- ROTATION CADENCE
-- -----------------------------------------------------------------------
-- Keep Avenger's Shield a true opener / range-bridge button, keep Hammer of
-- the Righteous and Shield of Righteousness as the main in-pack pressure, and
-- let Consecration come online a bit sooner once we know we are handling a
-- small pile instead of a pure single target.
UPDATE `living_world_bot_combat_default_entry`
SET `priority` = 45
WHERE `entry_id` = 400;
