INSERT INTO `living_world_bot_combat_default_profile` (
    `spec_key`,
    `role_key`,
    `display_name`,
    `class_key`,
    `conservation_mode`,
    `resource_low_water`,
    `resource_high_water`,
    `enable_down_rank`,
    `down_rank_floor`,
    `default_aoe_mode`,
    `default_aoe_min_targets`,
    `default_aoe_scan_radius`,
    `context_key`,
    `variant_key`,
    `description`,
    `targeting_mode`,
    `current_target_bias`,
    `assist_target_bias`,
    `focus_fire_bias`,
    `protect_ally_bias`,
    `prefer_healer_bias`,
    `prefer_dps_bias`,
    `avoid_tank_bias`
)
SELECT
    src.`spec_key`,
    src.`role_key`,
    CONCAT(src.`display_name`, ' PvP'),
    src.`class_key`,
    src.`conservation_mode`,
    src.`resource_low_water`,
    src.`resource_high_water`,
    src.`enable_down_rank`,
    src.`down_rank_floor`,
    src.`default_aoe_mode`,
    src.`default_aoe_min_targets`,
    src.`default_aoe_scan_radius`,
    'PvP',
    src.`variant_key`,
    src.`description`,
    src.`targeting_mode`,
    src.`current_target_bias`,
    src.`assist_target_bias`,
    src.`focus_fire_bias`,
    src.`protect_ally_bias`,
    src.`prefer_healer_bias`,
    src.`prefer_dps_bias`,
    src.`avoid_tank_bias`
FROM `living_world_bot_combat_default_profile` AS src
WHERE src.`context_key` = 'PvE'
  AND NOT EXISTS (
      SELECT 1
      FROM `living_world_bot_combat_default_profile` AS dst
      WHERE dst.`class_key` <=> src.`class_key`
        AND dst.`spec_key` = src.`spec_key`
        AND dst.`role_key` = src.`role_key`
        AND dst.`context_key` = 'PvP'
        AND dst.`variant_key` = src.`variant_key`
  );
