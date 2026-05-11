-- rev_living_world_020_healer_aoe_conditions (world DB)
--
-- Align healer AoE doctrine rows with the runtime by requiring at least
-- three party members below 70% health before the AoE heal entries fire.
--
-- Entry mapping:
--   79  = Priest Holy        Prayer of Healing
--   94  = Shaman Restoration Chain Heal
--   101 = Druid Restoration  Wild Growth

UPDATE living_world_bot_combat_default_condition
SET subject_key   = 'self',
    stat_key      = 'party_members_below_hp_pct',
    comparison    = 5,
    numeric_value = 3,
    string_value  = '70'
WHERE entry_id IN (79, 94, 101)
  AND sequence = 0;