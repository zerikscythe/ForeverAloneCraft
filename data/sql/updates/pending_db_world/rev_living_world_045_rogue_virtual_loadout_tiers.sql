-- rev_living_world_045_rogue_virtual_loadout_tiers (world DB)
--
-- Adds Rogue spec-specific tier-2 and tier-3 virtual loadouts so fresh 80
-- world bots land in a believable pre-raid band instead of inheriting only the
-- generic rogue class fallback package.
--
-- Design intent:
--   * tier 2 = heroic / dungeon gearing
--   * tier 3 = fresh-80 pre-raid
--   * Combat gets a slightly sturdier, higher-AP package
--   * Assassination leans harder into agility and AP
--   * Subtlety stays agility-heavy, slightly lighter than Combat

INSERT INTO `living_world_bot_virtual_loadout`
    (`class_id`, `spec_key`, `loadout_key`, `gear_tier`, `display_name`, `description`,
     `bonus_strength`, `bonus_agility`, `bonus_stamina`, `bonus_intellect`, `bonus_spirit`,
     `bonus_health`, `bonus_mana`, `bonus_armor`, `bonus_attack_power`, `bonus_ranged_attack_power`)
VALUES
    -- Rogue tier 2: dungeon / heroic gearing
    (4, 'Assassination', '', 2, 'Rogue Assassination Tier 2',
        'Heroic-gearing assassination rogue loadout for endgame world bots.',
        25, 130,  95, 0, 0, 1150, 0, 290, 285, 0),
    (4, 'Combat', '', 2, 'Rogue Combat Tier 2',
        'Heroic-gearing combat rogue loadout for endgame world bots.',
        40, 120, 105, 0, 0, 1250, 0, 340, 320, 0),
    (4, 'Subtlety', '', 2, 'Rogue Subtlety Tier 2',
        'Heroic-gearing subtlety rogue loadout for endgame world bots.',
        25, 125,  95, 0, 0, 1150, 0, 285, 270, 0),

    -- Rogue tier 3: fresh-80 pre-raid
    (4, 'Assassination', '', 3, 'Rogue Assassination Tier 3',
        'Fresh-80 pre-raid assassination rogue loadout for world bots.',
        30, 160, 115, 0, 0, 1450, 0, 360, 380, 0),
    (4, 'Combat', '', 3, 'Rogue Combat Tier 3',
        'Fresh-80 pre-raid combat rogue loadout for world bots.',
        50, 150, 125, 0, 0, 1550, 0, 420, 430, 0),
    (4, 'Subtlety', '', 3, 'Rogue Subtlety Tier 3',
        'Fresh-80 pre-raid subtlety rogue loadout for world bots.',
        30, 155, 110, 0, 0, 1400, 0, 350, 360, 0)
ON DUPLICATE KEY UPDATE
    `display_name`               = VALUES(`display_name`),
    `description`                = VALUES(`description`),
    `bonus_strength`             = VALUES(`bonus_strength`),
    `bonus_agility`              = VALUES(`bonus_agility`),
    `bonus_stamina`              = VALUES(`bonus_stamina`),
    `bonus_intellect`            = VALUES(`bonus_intellect`),
    `bonus_spirit`               = VALUES(`bonus_spirit`),
    `bonus_health`               = VALUES(`bonus_health`),
    `bonus_mana`                 = VALUES(`bonus_mana`),
    `bonus_armor`                = VALUES(`bonus_armor`),
    `bonus_attack_power`         = VALUES(`bonus_attack_power`),
    `bonus_ranged_attack_power`  = VALUES(`bonus_ranged_attack_power`);
