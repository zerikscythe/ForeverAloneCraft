-- rev_living_world_029_arcane_mage_talent_template (world DB)
--
-- Repairs the missing Arcane Mage default talent template for class 8.
-- Existing runtime preparation canonicalizes `mage_arcane` -> `Arcane`, so
-- world-bot preparation expects a living_world_bot_talent_template row with
-- spec_key='Arcane' and class_id=8.

INSERT INTO `living_world_bot_talent_template`
    (`spec_key`, `class_id`, `display_name`)
VALUES
    ('Arcane', 8, 'Arcane Mage DPS')
ON DUPLICATE KEY UPDATE
    `display_name` = VALUES(`display_name`);

SET @arcane_mage_template_id := (
    SELECT `template_id`
    FROM `living_world_bot_talent_template`
    WHERE `spec_key` = 'Arcane' AND `class_id` = 8
    LIMIT 1
);

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` = @arcane_mage_template_id;

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    (@arcane_mage_template_id,   0,   74, 'Arcane Subtlety', 2),
    (@arcane_mage_template_id,   1,   76, 'Arcane Focus', 3),
    (@arcane_mage_template_id,  12,   75, 'Arcane Concentration', 5),
    (@arcane_mage_template_id,  20,   82, 'Magic Attunement', 2),
    (@arcane_mage_template_id,  21,   81, 'Spell Impact', 3),
    (@arcane_mage_template_id,  22, 1845, 'Student of the Mind', 1),
    (@arcane_mage_template_id,  23, 2211, 'Focus Magic', 1),
    (@arcane_mage_template_id,  32, 1142, 'Arcane Meditation', 3),
    (@arcane_mage_template_id,  33, 2222, 'Torment the Weak', 3),
    (@arcane_mage_template_id,  41,   86, 'Presence of Mind', 1),
    (@arcane_mage_template_id,  43,   77, 'Arcane Mind', 5),
    (@arcane_mage_template_id,  51,  421, 'Arcane Instability', 3),
    (@arcane_mage_template_id,  52, 1725, 'Arcane Potency', 2),
    (@arcane_mage_template_id,  60, 1727, 'Arcane Empowerment', 3),
    (@arcane_mage_template_id,  61,   87, 'Arcane Power', 1),
    (@arcane_mage_template_id,  71, 1843, 'Arcane Flows', 2),
    (@arcane_mage_template_id,  72, 1728, 'Mind Mastery', 5),
    (@arcane_mage_template_id,  82, 2209, 'Missile Barrage', 5),
    (@arcane_mage_template_id,  91, 1846, 'Netherwind Presence', 3),
    (@arcane_mage_template_id,  92, 1826, 'Spell Power', 2),
    (@arcane_mage_template_id, 101, 1847, 'Arcane Barrage', 1),
    (@arcane_mage_template_id, 201, 1141, 'Incineration', 3),
    (@arcane_mage_template_id, 301,   37, 'Improved Frostbolt', 2),
    (@arcane_mage_template_id, 302,   62, 'Ice Floes', 3),
    (@arcane_mage_template_id, 310,   73, 'Ice Shards', 3),
    (@arcane_mage_template_id, 312, 1649, 'Precision', 3),
    (@arcane_mage_template_id, 321,   69, 'Icy Veins', 1)
ON DUPLICATE KEY UPDATE
    `priority`     = VALUES(`priority`),
    `talent_name`  = VALUES(`talent_name`),
    `desired_rank` = VALUES(`desired_rank`);
