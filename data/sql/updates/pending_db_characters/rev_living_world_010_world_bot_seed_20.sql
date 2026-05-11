-- rev_living_world_010_world_bot_seed_20 (characters DB)
--
-- Seeds an initial deterministic set of 20 autonomous creature-based world-bot
-- identities (10 Alliance / 10 Horde). These are intended as a stable starter
-- pool for hub-based autonomous spawn testing.

INSERT IGNORE INTO living_world_bot_identity
    (name, race_id, class_id, spec_key, faction, display_id, gender,
     level, gear_tier, has_herbalism, has_mining, has_fishing)
VALUES
    -- Alliance (10)
    ('Marcus',       1,  1, 'Arms',            1,    49, 0, 12, 1, 0, 0, 0),
    ('Claire',       1,  5, 'Holy',            1,    53, 1, 18, 1, 0, 0, 1),
    ('Bronk',        3,  2, 'Retribution',     1,   131, 0, 27, 1, 0, 1, 0),
    ('Tyrenna',      4,  3, 'Marksmanship',    1,    59, 1, 35, 1, 1, 0, 0),
    ('Cogsworth',    7,  8, 'Frost',           1,   111, 0, 44, 1, 0, 0, 0),
    ('Elodra',      11,  7, 'Restoration',     1, 16128, 1, 58, 2, 1, 0, 1),
    ('Roland',       1,  9, 'Destruction',     1,    50, 0, 63, 2, 0, 0, 0),
    ('Leafsong',     4, 11, 'Balance',         1,    60, 1, 69, 2, 1, 0, 0),
    ('Hegir',        3,  6, 'Frost',           1,   132, 0, 75, 3, 0, 1, 0),
    ('Azuremist',   11,  2, 'Holy',            1, 16127, 1, 80, 3, 0, 0, 1),

    -- Horde (10)
    ('Grak',         2,  1, 'Fury',            2,    27, 0, 11, 1, 0, 0, 0),
    ('Sallow',       5,  8, 'Arcane',          2,    61, 1, 19, 1, 0, 0, 0),
    ('Stonehoof',    6, 11, 'Feral',           2,    61, 0, 28, 1, 1, 0, 0),
    ('Hexveil',      8,  4, 'Subtlety',        2,    74, 1, 37, 1, 0, 0, 1),
    ('Sunwhisper',  10,  2, 'Retribution',     2, 15479, 1, 46, 1, 0, 1, 0),
    ('Mors',         5,  5, 'Shadow',          2,    57, 0, 59, 2, 0, 0, 0),
    ('Krom',         2,  7, 'Enhancement',     2,    28, 0, 64, 2, 1, 0, 1),
    ('Lunarglow',   10,  8, 'Fire',            2, 15476, 0, 69, 2, 0, 0, 0),
    ('Vol',          8,  3, 'Survival',        2,    73, 0, 76, 3, 0, 1, 0),
    ('Bleakhaven',   5,  9, 'Affliction',      2,    63, 1, 80, 3, 0, 0, 1);