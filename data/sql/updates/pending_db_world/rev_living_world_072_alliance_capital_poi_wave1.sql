-- rev_living_world_072_alliance_capital_poi_wave1 (world DB)
--
-- Seeds a first wave of clearly named Alliance capital task points using
-- marker-cache service NPC/object positions. These are intended primarily for
-- PoI selection, city assist-route authoring, and future city-errand expansion.

INSERT INTO living_world_task_point
    (point_key, zone_id, map_id, point_type, point_name, x, y, z)
VALUES
    ('ironforge_auctioneer_buckler_ne',        1537, 0,   'auction_house', 'Ironforge Auctioneer Buckler',          -4948.01,   -901.528,  505.172),
    ('ironforge_auctioneer_lympkin_ne',        1537, 0,   'auction_house', 'Ironforge Auctioneer Lympkin',         -4967.86,   -917.619,  505.169),
    ('ironforge_banker_bailey_stonemantle_n',  1537, 0,   'bank',          'Ironforge Banker Bailey Stonemantle',  -4886.55,   -997.594,  504.024),
    ('ironforge_banker_soleil_stonemantle_n',  1537, 0,   'bank',          'Ironforge Banker Soleil Stonemantle',  -4895.64,  -1004.660,  504.024),
    ('ironforge_mailbox_bank_n',               1537, 0,   'mailbox',       'Ironforge Bank Mailbox',               -4910.38,   -976.210,  501.410),
    ('ironforge_mailbox_great_forge_n',        1537, 0,   'mailbox',       'Ironforge Great Forge Mailbox',        -4845.78,   -879.300,  501.610),
    ('ironforge_innkeeper_firebrew',           1537, 0,   'inn',           'Ironforge Innkeeper Firebrew',         -4840.67,   -857.090,  502.000),

    ('darnassus_auctioneer_cazarez_nw',        1657, 1,   'auction_house', 'Darnassus Auctioneer Cazarez',          9868.17,   2350.060, 1331.980),
    ('darnassus_auctioneer_silvalas_nw',       1657, 1,   'auction_house', 'Darnassus Auctioneer Silva''las',       9860.31,   2331.850, 1331.980),
    ('darnassus_banker_garryeth_n',            1657, 1,   'bank',          'Darnassus Banker Garryeth',             9942.03,   2519.240, 1317.660),
    ('darnassus_banker_idriana_n',             1657, 1,   'bank',          'Darnassus Banker Idriana',              9938.84,   2521.530, 1317.660),
    ('darnassus_mailbox_bank_n',               1657, 1,   'mailbox',       'Darnassus Bank Mailbox',                9943.00,   2497.740, 1317.690),
    ('darnassus_mailbox_inn_ne',               1657, 1,   'mailbox',       'Darnassus Inn Mailbox',                10122.10,   2227.400, 1328.190),
    ('darnassus_innkeeper_saelienne',          1657, 1,   'inn',           'Darnassus Innkeeper Saelienne',        10127.90,   2224.790, 1328.810),

    ('exodar_auctioneer_eoch_sw',              3557, 530, 'auction_house', 'Exodar Auctioneer Eoch',               -4023.29, -11739.700, -151.799),
    ('exodar_auctioneer_iressa_sw',            3557, 530, 'auction_house', 'Exodar Auctioneer Iressa',             -4025.49, -11736.000, -151.811),
    ('exodar_banker_jaela_n',                  3557, 530, 'bank',          'Exodar Banker Jaela',                  -3918.95, -11544.700, -150.039),
    ('exodar_banker_ossco_n',                  3557, 530, 'bank',          'Exodar Banker Ossco',                  -3923.77, -11544.500, -150.193),
    ('exodar_mailbox_bank_n',                  3557, 530, 'mailbox',       'Exodar Bank Mailbox',                  -3913.08, -11606.000, -138.350),
    ('exodar_mailbox_city_center_s',           3557, 530, 'mailbox',       'Exodar City Center Mailbox',           -3975.04, -11700.000, -139.260),
    ('exodar_innkeeper_breel',                 3557, 530, 'inn',           'Exodar Caregiver Breel',               -3746.37, -11696.100, -105.770)
ON DUPLICATE KEY UPDATE
    zone_id    = VALUES(zone_id),
    map_id     = VALUES(map_id),
    point_type = VALUES(point_type),
    point_name = VALUES(point_name),
    x          = VALUES(x),
    y          = VALUES(y),
    z          = VALUES(z);
