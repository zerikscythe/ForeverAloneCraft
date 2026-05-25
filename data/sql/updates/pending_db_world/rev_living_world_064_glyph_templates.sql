-- Seed initial glyph recommendations for world-bot proof cases.
-- We store glyph spell aura ids directly; world bots materialize these as auras.

CREATE TABLE IF NOT EXISTS `living_world_bot_glyph_template` (
  `template_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `class_id` TINYINT UNSIGNED NOT NULL,
  `spec_key` VARCHAR(32) NOT NULL DEFAULT '',
  `loadout_key` VARCHAR(64) NOT NULL DEFAULT '',
  `slot_index` TINYINT UNSIGNED NOT NULL,
  `glyph_spell_id` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`template_id`),
  UNIQUE KEY `uk_lw_bot_glyph_template` (`class_id`,`spec_key`,`loadout_key`,`slot_index`),
  KEY `idx_lw_bot_glyph_template_lookup` (`class_id`,`spec_key`,`loadout_key`,`slot_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELETE FROM `living_world_bot_glyph_template`
WHERE (`class_id` = 8 AND LOWER(`spec_key`) = LOWER('Frost'))
   OR (`class_id` = 6 AND LOWER(`spec_key`) = LOWER('Unholy'));

INSERT INTO `living_world_bot_glyph_template`
    (`class_id`, `spec_key`, `loadout_key`, `slot_index`, `glyph_spell_id`)
VALUES
    -- Frost Mage
    (8, 'Frost', '', 0, 56370), -- Glyph of Frostbolt
    (8, 'Frost', '', 1, 70937), -- Glyph of Eternal Water
    (8, 'Frost', '', 2, 56382), -- Glyph of Molten Armor
    (8, 'Frost', '', 3, 57924), -- Glyph of Arcane Intellect
    (8, 'Frost', '', 4, 57925), -- Glyph of Slow Fall
    (8, 'Frost', '', 5, 57926), -- Glyph of Fire Ward

    -- Unholy Death Knight
    (6, 'Unholy', '', 0, 58686), -- Glyph of the Ghoul
    (6, 'Unholy', '', 1, 57214), -- Glyph of Death and Decay
    (6, 'Unholy', '', 2, 57219), -- Glyph of Icy Touch
    (6, 'Unholy', '', 3, 57228), -- Glyph of Raise Dead
    (6, 'Unholy', '', 4, 57230), -- Glyph of Pestilence
    (6, 'Unholy', '', 5, 57209); -- Glyph of Blood Tap
