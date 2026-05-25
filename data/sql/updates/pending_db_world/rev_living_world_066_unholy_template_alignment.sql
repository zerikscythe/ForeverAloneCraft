-- rev_living_world_066_unholy_template_alignment (world DB)
--
-- Align the modern Unholy Death Knight talent template to the local
-- Icy Veins build mirror. The first-pass reset drifted into an older
-- anti-magic / Scourge Strike shape and missed the ghoul package that
-- current world-bot glyph and pet validation depends on.

DELETE FROM `living_world_bot_talent_template_entry`
WHERE `template_id` = 12;

INSERT INTO `living_world_bot_talent_template_entry`
    (`template_id`, `priority`, `talent_id`, `talent_name`, `desired_rank`)
VALUES
    (12,   0, 2082, 'Vicious Strikes', 2),
    (12,   1, 1932, 'Virulence', 3),
    (12,  11, 1933, 'Morbidity', 3),
    (12,  13, 1934, 'Ravenous Dead', 3),
    (12,  21, 2047, 'Necrosis', 5),
    (12,  32, 2004, 'Blood-Caked Blade', 3),
    (12,  33, 2225, 'Night of the Dead', 2),
    (12,  40, 1996, 'Unholy Blight', 1),
    (12,  41, 2005, 'Impurity', 5),
    (12,  42, 2011, 'Dirge', 2),
    (12,  53, 1984, 'Master of Ghouls', 1),
    (12,  60, 2285, 'Desolation', 5),
    (12,  63, 2085, 'Ghoul Frenzy', 1),
    (12,  71, 1962, 'Crypt Fever', 3),
    (12,  72, 2007, 'Bone Shield', 1),
    (12,  80, 2003, 'Wandering Plague', 3),
    (12,  81, 2043, 'Ebon Plaguebringer', 3),
    (12,  91, 2036, 'Rage of Rivendare', 5),
    (12, 101, 2000, 'Summon Gargoyle', 1),
    (12, 200, 2031, 'Improved Icy Touch', 3),
    (12, 201, 2020, 'Runic Power Mastery', 2),
    (12, 212, 1973, 'Black Ice', 4),
    (12, 213, 2022, 'Nerves of Cold Steel', 3),
    (12, 220, 2042, 'Icy Talons', 5),
    (12, 233, 1971, 'Endless Winter', 2);
