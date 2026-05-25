-- rev_living_world_032_city_poi_task_expansion (world DB)
--
-- Expands city task-point and task-template coverage so reserve/city bots can
-- visit a wider mix of believable service and hangout locations using the same
-- DB tables surfaced by the lw-editor task UI.

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('stormwind_banker_john_burnside_n',      1519, 0, 'bank',                    'Stormwind Banker John Burnside',             -8939.28,   620.538,   99.606),
    ('stormwind_banker_olivia_burnside_n',    1519, 0, 'bank',                    'Stormwind Banker Olivia Burnside',           -8931.23,   605.801,   99.606),
    ('stormwind_auctioneer_chilton_ne',       1519, 0, 'auction_house',           'Stormwind Auctioneer Chilton',               -8824.28,   665.711,   97.4674),
    ('stormwind_auctioneer_jaxon_ne',         1519, 0, 'auction_house',           'Stormwind Auctioneer Jaxon',                 -8814.55,   660.315,   96.6818),
    ('stormwind_mailbox_trade_w',             1519, 0, 'mailbox',                 'Stormwind Trade Mailbox West',               -8876.98,   652.007,   95.9927),
    ('stormwind_mailbox_auction_e',           1519, 0, 'mailbox',                 'Stormwind Auction Mailbox East',             -8815.17,   652.927,   94.8966),
    ('stormwind_anvil_dwarven_district',      1519, 0, 'anvil',                   'Stormwind Dwarven District Anvil',           -8433.50,   610.800,   95.7000),
    ('stormwind_kitchen_trade_district',      1519, 0, 'kitchen',                 'Stormwind Trade District Kitchen',           -8861.20,   670.900,   97.9000),
    ('stormwind_prof_alchemy_mage_quarter',   1519, 0, 'profession_alchemy',      'Stormwind Mage Quarter Alchemy Corner',      -8988.50,   759.400,  105.2000),
    ('stormwind_prof_leatherworking_old_town',1519, 0, 'profession_leatherworking','Stormwind Old Town Leatherworking Corner',  -8718.00,   465.600,  103.4000),
    ('stormwind_old_town_walk',               1519, 0, 'poi',                     'Stormwind Old Town Walk',                    -8664.00,   491.000,  103.0000),
    ('stormwind_cathedral_square',            1519, 0, 'poi',                     'Stormwind Cathedral Square',                 -8578.00,   812.000,  106.5000),
    ('stormwind_mage_quarter_plaza',          1519, 0, 'poi',                     'Stormwind Mage Quarter Plaza',               -9003.50,   860.550,  105.8770),

    ('orgrimmar_bank_01',           1637, 1, 'bank',                    'Orgrimmar Bank Counter 01',                1627.32, -4376.07,   12.0576),
    ('orgrimmar_bank_02',           1637, 1, 'bank',                    'Orgrimmar Bank Counter 02',                1622.90, -4369.06,   12.0536),
    ('orgrimmar_ah_01',             1637, 1, 'auction_house',           'Orgrimmar Auction House Desk 01',          1695.92, -4455.55,   20.3911),
    ('orgrimmar_ah_02',             1637, 1, 'auction_house',           'Orgrimmar Auction House Desk 02',          1667.62, -4463.76,   20.3911),
    ('orgrimmar_mailbox_01',        1637, 1, 'mailbox',                 'Orgrimmar Mailbox 01',                     1615.58, -4391.60,   10.3350),
    ('orgrimmar_mailbox_02',        1637, 1, 'mailbox',                 'Orgrimmar Mailbox 02',                     1657.87, -4433.03,   17.4818),
    ('orgrimmar_anvil_01',          1637, 1, 'anvil',                   'Orgrimmar Forge and Anvil',                1781.50, -4293.00,    8.8500),
    ('orgrimmar_kitchen_01',        1637, 1, 'kitchen',                 'Orgrimmar Kitchen',                        1635.00, -4440.00,   15.7000),
    ('orgrimmar_prof_alchemy_01',   1637, 1, 'profession_alchemy',      'Orgrimmar Alchemy Corner',                 1958.00, -4478.00,   73.0000),
    ('orgrimmar_prof_lw_01',        1637, 1, 'profession_leatherworking','Orgrimmar Leatherworking Corner',         1998.00, -4257.00,   29.0000),
    ('orgrimmar_poi_valley_strength_01',1637,1,'poi',                   'Orgrimmar Valley of Strength',             1670.00, -4350.00,   26.0000),
    ('orgrimmar_poi_valley_honor_01',1637,1,'poi',                      'Orgrimmar Valley of Honor',                1805.00, -4218.00,   10.2000),
    ('orgrimmar_poi_cleft_01',      1637, 1, 'poi',                     'Orgrimmar Cleft of Shadow',                1810.00, -4372.00,   -9.5000)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);

INSERT INTO living_world_task_template
    (template_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('stormwind_use_auctioneer_chilton',       'Use Auctioneer Chilton in Stormwind',     'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_use_auctioneer_jaxon',         'Use Auctioneer Jaxon in Stormwind',       'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_use_banker_john_burnside',     'Use Banker John Burnside in Stormwind',   'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_use_banker_olivia_burnside',   'Use Banker Olivia Burnside in Stormwind', 'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_use_mailbox_trade_w',          'Use West Trade Mailbox in Stormwind',     'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_use_mailbox_auction_e',        'Use East Auction Mailbox in Stormwind',   'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_use_anvil_dwarven_district',   'Use Dwarven District Anvil',              'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),
    ('stormwind_use_kitchen_trade_district',   'Use Trade District Kitchen',              'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),
    ('stormwind_visit_alchemy_mage_quarter',   'Visit Mage Quarter Alchemy',              'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),
    ('stormwind_visit_leatherworking_old_town','Visit Old Town Leatherworking',           'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),
    ('stormwind_visit_old_town_walk',          'Visit Old Town Walk',                      'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),
    ('stormwind_idle_cathedral_square',        'Idle by Cathedral Square',                 'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),
    ('stormwind_visit_mage_quarter_plaza',     'Visit Mage Quarter Plaza',                 'city_errand', 1, 1, 80, 0, 0, 0, 1, 1),

    ('orgrimmar_use_ah_01',            'Use AH 01 in Orgrimmar',               'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_use_ah_02',            'Use AH 02 in Orgrimmar',               'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_use_bank_01',          'Use Bank 01 in Orgrimmar',             'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_use_bank_02',          'Use Bank 02 in Orgrimmar',             'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_use_mailbox_01',       'Use Mailbox 01 in Orgrimmar',          'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_use_mailbox_02',       'Use Mailbox 02 in Orgrimmar',          'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_use_anvil_01',         'Use Anvil in Orgrimmar',               'city_errand', 2, 1, 80, 0, 0, 0, 1, 1),
    ('orgrimmar_use_kitchen_01',       'Use Kitchen in Orgrimmar',             'city_errand', 2, 1, 80, 0, 0, 0, 1, 1),
    ('orgrimmar_visit_prof_alch_01',   'Visit Alchemy in Orgrimmar',           'city_errand', 2, 1, 80, 0, 0, 0, 1, 1),
    ('orgrimmar_visit_prof_lw_01',     'Visit Leatherworking in Orgrimmar',    'city_errand', 2, 1, 80, 0, 0, 0, 1, 1),
    ('orgrimmar_visit_poi_strength_01','Visit Valley of Strength',             'city_errand', 2, 1, 80, 0, 0, 0, 1, 1),
    ('orgrimmar_idle_poi_honor_01',    'Idle by Valley of Honor',              'city_errand', 2, 1, 80, 0, 0, 0, 1, 1),
    ('orgrimmar_visit_poi_cleft_01',   'Visit Cleft of Shadow',                'city_errand', 2, 1, 80, 0, 0, 0, 1, 1)
ON DUPLICATE KEY UPDATE
    display_name       = VALUES(display_name),
    task_family        = VALUES(task_family),
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    requires_herbalism = VALUES(requires_herbalism),
    requires_mining    = VALUES(requires_mining),
    requires_fishing   = VALUES(requires_fishing),
    weight             = VALUES(weight),
    is_enabled         = VALUES(is_enabled);

DELETE s
FROM living_world_task_template_step s
JOIN living_world_task_template t ON t.template_id = s.template_id
WHERE t.template_key IN (
    'stormwind_use_auctioneer_chilton', 'stormwind_use_auctioneer_jaxon',
    'stormwind_use_banker_john_burnside', 'stormwind_use_banker_olivia_burnside',
    'stormwind_use_mailbox_trade_w', 'stormwind_use_mailbox_auction_e',
    'stormwind_use_anvil_dwarven_district', 'stormwind_use_kitchen_trade_district',
    'stormwind_visit_alchemy_mage_quarter', 'stormwind_visit_leatherworking_old_town',
    'stormwind_visit_old_town_walk', 'stormwind_idle_cathedral_square',
    'stormwind_visit_mage_quarter_plaza',
    'orgrimmar_use_ah_01', 'orgrimmar_use_ah_02', 'orgrimmar_use_bank_01', 'orgrimmar_use_bank_02',
    'orgrimmar_use_mailbox_01', 'orgrimmar_use_mailbox_02', 'orgrimmar_use_anvil_01', 'orgrimmar_use_kitchen_01',
    'orgrimmar_visit_prof_alch_01', 'orgrimmar_visit_prof_lw_01', 'orgrimmar_visit_poi_strength_01',
    'orgrimmar_idle_poi_honor_01', 'orgrimmar_visit_poi_cleft_01');

INSERT INTO living_world_task_template_step
    (template_id, step_order, step_type, target_zone_id, target_point_key, duration_min_sec, duration_max_sec, label)
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_auctioneer_chilton_ne', 45, 120, 'Browse Auctioneer Chilton''s lane'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_auctioneer_chilton'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_auctioneer_jaxon_ne', 45, 120, 'Browse Auctioneer Jaxon''s lane'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_auctioneer_jaxon'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_banker_john_burnside_n', 40, 120, 'Check in with John Burnside'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_banker_john_burnside'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_banker_olivia_burnside_n', 40, 120, 'Check in with Olivia Burnside'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_banker_olivia_burnside'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_mailbox_trade_w', 20, 75, 'Sort mail near the west trade lane'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_mailbox_trade_w'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_mailbox_auction_e', 20, 75, 'Sort mail near the east auction lane'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_mailbox_auction_e'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_anvil_dwarven_district', 60, 180, 'Hammer away in the Dwarven District'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_anvil_dwarven_district'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_kitchen_trade_district', 45, 150, 'Linger in the Trade District kitchen'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_use_kitchen_trade_district'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_prof_alchemy_mage_quarter', 60, 180, 'Visit the Mage Quarter alchemy corner'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_visit_alchemy_mage_quarter'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_prof_leatherworking_old_town', 60, 180, 'Visit the Old Town leatherworking corner'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_visit_leatherworking_old_town'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_old_town_walk', 75, 210, 'Wander through Old Town'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_visit_old_town_walk'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_cathedral_square', 90, 240, 'Idle by Cathedral Square'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_idle_cathedral_square'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1519, 'stormwind_mage_quarter_plaza', 75, 210, 'Visit the Mage Quarter plaza'
FROM living_world_task_template t WHERE t.template_key = 'stormwind_visit_mage_quarter_plaza'

UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_ah_01', 45, 120, 'Browse the first auction lane'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_ah_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_ah_02', 45, 120, 'Browse the second auction lane'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_ah_02'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_bank_01', 40, 120, 'Check the first bank counter'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_bank_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_bank_02', 40, 120, 'Check the second bank counter'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_bank_02'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_mailbox_01', 20, 75, 'Sort mail at the first mailbox'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_mailbox_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_mailbox_02', 20, 75, 'Sort mail at the second mailbox'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_mailbox_02'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_anvil_01', 60, 180, 'Hammer away at the forge'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_anvil_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_kitchen_01', 45, 150, 'Linger in the kitchen'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_use_kitchen_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_prof_alchemy_01', 60, 180, 'Visit the alchemy corner'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_visit_prof_alch_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_prof_lw_01', 60, 180, 'Visit the leatherworking corner'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_visit_prof_lw_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_poi_valley_strength_01', 75, 210, 'Visit the Valley of Strength'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_visit_poi_strength_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_poi_valley_honor_01', 90, 240, 'Idle by the Valley of Honor'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_idle_poi_honor_01'
UNION ALL
SELECT t.template_id, 1, 'idle_city', 1637, 'orgrimmar_poi_cleft_01', 75, 210, 'Visit the Cleft of Shadow'
FROM living_world_task_template t WHERE t.template_key = 'orgrimmar_visit_poi_cleft_01';

INSERT INTO living_world_playlist
    (playlist_key, display_name, task_family, required_faction,
     min_level, max_level, requires_herbalism, requires_mining, requires_fishing,
     weight, is_enabled)
VALUES
    ('stormwind_trade_shuffle',      'Stormwind Trade Shuffle',         'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_crafter_afternoon',  'Stormwind Crafter Afternoon',     'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('stormwind_district_wander',    'Stormwind District Wander',       'city_errand', 1, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_trade_shuffle',      'Orgrimmar Trade Shuffle',         'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_crafter_afternoon',  'Orgrimmar Crafter Afternoon',     'city_errand', 2, 1, 80, 0, 0, 0, 2, 1),
    ('orgrimmar_district_wander',    'Orgrimmar District Wander',       'city_errand', 2, 1, 80, 0, 0, 0, 2, 1)
ON DUPLICATE KEY UPDATE
    display_name       = VALUES(display_name),
    task_family        = VALUES(task_family),
    required_faction   = VALUES(required_faction),
    min_level          = VALUES(min_level),
    max_level          = VALUES(max_level),
    requires_herbalism = VALUES(requires_herbalism),
    requires_mining    = VALUES(requires_mining),
    requires_fishing   = VALUES(requires_fishing),
    weight             = VALUES(weight),
    is_enabled         = VALUES(is_enabled);

DELETE e
FROM living_world_playlist_entry e
JOIN living_world_playlist p ON p.playlist_id = e.playlist_id
WHERE p.playlist_key IN (
    'stormwind_trade_shuffle', 'stormwind_crafter_afternoon', 'stormwind_district_wander',
    'orgrimmar_trade_shuffle', 'orgrimmar_crafter_afternoon', 'orgrimmar_district_wander');

INSERT INTO living_world_playlist_entry
    (playlist_id, entry_order, task_template_id, repeat_count, note)
SELECT p.playlist_id, 1, t.template_id, 1, 'Start at the west Trade District mailbox'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_use_mailbox_trade_w'
WHERE p.playlist_key = 'stormwind_trade_shuffle'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Work Auctioneer Chilton''s lane'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_use_auctioneer_chilton'
WHERE p.playlist_key = 'stormwind_trade_shuffle'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Visit Olivia Burnside at the bank'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_use_banker_olivia_burnside'
WHERE p.playlist_key = 'stormwind_trade_shuffle'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'Loop back through the east auction mailbox'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_use_mailbox_auction_e'
WHERE p.playlist_key = 'stormwind_trade_shuffle'

UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Start at the forge'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_use_anvil_dwarven_district'
WHERE p.playlist_key = 'stormwind_crafter_afternoon'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Check Mage Quarter alchemy supplies'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_visit_alchemy_mage_quarter'
WHERE p.playlist_key = 'stormwind_crafter_afternoon'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Stop in the Trade District kitchen'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_use_kitchen_trade_district'
WHERE p.playlist_key = 'stormwind_crafter_afternoon'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'End in the Old Town leatherworking corner'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_visit_leatherworking_old_town'
WHERE p.playlist_key = 'stormwind_crafter_afternoon'

UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Visit the Mage Quarter plaza'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_visit_mage_quarter_plaza'
WHERE p.playlist_key = 'stormwind_district_wander'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Cross into Cathedral Square'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_idle_cathedral_square'
WHERE p.playlist_key = 'stormwind_district_wander'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Drift through Old Town'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'stormwind_visit_old_town_walk'
WHERE p.playlist_key = 'stormwind_district_wander'

UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Mail first'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_use_mailbox_01'
WHERE p.playlist_key = 'orgrimmar_trade_shuffle'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Work the first auction lane'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_use_ah_01'
WHERE p.playlist_key = 'orgrimmar_trade_shuffle'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Visit the bank'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_use_bank_02'
WHERE p.playlist_key = 'orgrimmar_trade_shuffle'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'Loop back through a second mailbox'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_use_mailbox_02'
WHERE p.playlist_key = 'orgrimmar_trade_shuffle'

UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Start at the forge'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_use_anvil_01'
WHERE p.playlist_key = 'orgrimmar_crafter_afternoon'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Check alchemy supplies'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_visit_prof_alch_01'
WHERE p.playlist_key = 'orgrimmar_crafter_afternoon'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Stop in the kitchen'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_use_kitchen_01'
WHERE p.playlist_key = 'orgrimmar_crafter_afternoon'
UNION ALL
SELECT p.playlist_id, 4, t.template_id, 1, 'End with leatherworking'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_visit_prof_lw_01'
WHERE p.playlist_key = 'orgrimmar_crafter_afternoon'

UNION ALL
SELECT p.playlist_id, 1, t.template_id, 1, 'Visit the Valley of Strength'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_visit_poi_strength_01'
WHERE p.playlist_key = 'orgrimmar_district_wander'
UNION ALL
SELECT p.playlist_id, 2, t.template_id, 1, 'Pause in the Valley of Honor'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_idle_poi_honor_01'
WHERE p.playlist_key = 'orgrimmar_district_wander'
UNION ALL
SELECT p.playlist_id, 3, t.template_id, 1, 'Drift into the Cleft of Shadow'
FROM living_world_playlist p
JOIN living_world_task_template t ON t.template_key = 'orgrimmar_visit_poi_cleft_01'
WHERE p.playlist_key = 'orgrimmar_district_wander';
