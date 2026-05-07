-- LivingWorld: class-specific healer default profiles.
--
-- This migration completes the healer profile split so each healer class
-- gets its own dedicated combat profile.  The doctrine resolver
-- (FindDefaultProfile) already prefers class-specific rows over shared NULL
-- class_key rows, so no C++ changes are required.
--
-- Final healer profile layout after this migration:
--   11  Holy        HEAL  Paladin   (updated: was shared, now pure Paladin rotation)
--   12  Restoration HEAL  Druid     (updated: was shared, now pure Druid rotation)
--   31  Holy        HEAL  Priest    (new)
--   33  Restoration HEAL  Shaman    (new)
--   24  Discipline  HEAL  Priest    (pre-existing, untouched)
--
-- Schema note:
--   The old unique key uk_living_world_bot_combat_default_profile_spec_role
--   (spec_key, role_key) was dropped to allow multiple class-specific profiles
--   per spec+role.  The active uniqueness constraint is
--   uq_class_spec_context (class_key, spec_key, context_key).
--
-- Schema enum encodings:
--   action.action_type:   0=Spell
--   action.rank_mode:     0=BestKnown  1=ExactSpellId  2=SpecificRank
--   action.slot:          0=primary    1=secondary
--   condition.comparison: 3=LessThanOrEqual  4=GreaterThan  5=GreaterThanOrEqual
--                         7=NotHasAura
--
-- All spell_base_id values are WotLK rank-1 base IDs.
-- rank_mode=0 (BestKnown) selects the highest known rank at runtime.

-- =========================================================================
-- Drop blocking unique key (safe to re-run; IF NOT EXISTS not needed as
-- this is the canonical migration script)
-- =========================================================================
SET @exist := (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name   = 'living_world_bot_combat_default_profile'
      AND index_name   = 'uk_living_world_bot_combat_default_profile_spec_role'
);
SET @sql := IF(@exist > 0,
    'ALTER TABLE living_world_bot_combat_default_profile DROP INDEX uk_living_world_bot_combat_default_profile_spec_role',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- =========================================================================
-- NEW PROFILES 31 + 33
-- =========================================================================
INSERT INTO living_world_bot_combat_default_profile
    (default_profile_id, spec_key, role_key, class_key, display_name,
     conservation_mode, resource_low_water, resource_high_water,
     enable_down_rank, down_rank_floor,
     default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius)
VALUES
    (31, 'Holy',        'HEAL', 'Priest', 'Priest Holy',        1, 40, 65, 1, 2, 0, 3, 12.0),
    (33, 'Restoration', 'HEAL', 'Shaman', 'Shaman Restoration', 1, 35, 60, 1, 2, 0, 3, 12.0)
ON DUPLICATE KEY UPDATE
    display_name            = VALUES(display_name),
    class_key               = VALUES(class_key),
    conservation_mode       = VALUES(conservation_mode),
    resource_low_water          = VALUES(resource_low_water),
    resource_high_water         = VALUES(resource_high_water),
    enable_down_rank        = VALUES(enable_down_rank),
    down_rank_floor         = VALUES(down_rank_floor),
    default_aoe_mode        = VALUES(default_aoe_mode),
    default_aoe_min_targets = VALUES(default_aoe_min_targets),
    default_aoe_scan_radius = VALUES(default_aoe_scan_radius);

-- =========================================================================
-- PROFILE 11 — Paladin Holy  (replace old mixed-class entries)
-- =========================================================================
DELETE FROM living_world_bot_combat_default_condition
    WHERE entry_id IN (SELECT entry_id FROM living_world_bot_combat_default_entry WHERE default_profile_id = 11);
DELETE FROM living_world_bot_combat_default_action
    WHERE entry_id IN (SELECT entry_id FROM living_world_bot_combat_default_entry WHERE default_profile_id = 11);
DELETE FROM living_world_bot_combat_default_entry WHERE default_profile_id = 11;

INSERT INTO living_world_bot_combat_default_profile
    (default_profile_id, spec_key, role_key, class_key, display_name,
     conservation_mode, resource_low_water, resource_high_water,
     enable_down_rank, down_rank_floor,
     default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius)
VALUES
    (11, 'Holy', 'HEAL', 'Paladin', 'Paladin Holy', 1, 40, 60, 1, 2, 0, 2, 10.0)
ON DUPLICATE KEY UPDATE
    display_name            = VALUES(display_name),
    class_key               = VALUES(class_key),
    conservation_mode       = VALUES(conservation_mode),
    resource_low_water          = VALUES(resource_low_water),
    resource_high_water         = VALUES(resource_high_water);

-- Priority order:
--   5   Beacon of Light on owner (keep active)
--   10  Flash of Light emergency when party < 40%
--   20  Holy Light moderate when party < 75%
--   30  Holy Shock instant heal when party < 85%
--   40  Judgement of Light on target
--   50  Seal of Light self-maintenance
--   60  Consecration ground AoE
--   70  Exorcism primary / Crusader Strike secondary
INSERT INTO living_world_bot_combat_default_entry
    (entry_id, default_profile_id, priority, label,
     is_interrupt, breaks_current_cast, enabled, condition_logic)
VALUES
    (83, 11,  5, 'Beacon of Light',            0, 1, 1, 0),
    (84, 11, 10, 'Flash of Light Emergency',   0, 0, 1, 0),
    (85, 11, 20, 'Holy Light Moderate',        0, 0, 1, 0),
    (86, 11, 30, 'Holy Shock',                 0, 1, 1, 0),
    (87, 11, 40, 'Judgement of Light',         0, 0, 1, 0),
    (88, 11, 50, 'Seal of Light',              0, 0, 1, 0),
    (89, 11, 60, 'Consecration',               0, 0, 1, 0),
    (90, 11, 70, 'Exorcism / Crusader Strike', 0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    default_profile_id = VALUES(default_profile_id), priority = VALUES(priority), label = VALUES(label);

INSERT INTO living_world_bot_combat_default_action
    (action_id, entry_id, slot, action_type, spell_base_id, rank_mode, rank_value, target_key)
VALUES
    (743, 83, 0, 0, 53563, 0, 0, 'owner'),              -- Beacon of Light
    (744, 84, 0, 0, 19750, 0, 0, 'lowest_hp_party'),    -- Flash of Light
    (745, 84, 1, 0, 635,   0, 0, 'lowest_hp_party'),    -- Holy Light
    (746, 85, 0, 0, 635,   0, 0, 'lowest_hp_party'),    -- Holy Light
    (747, 85, 1, 0, 19750, 0, 0, 'lowest_hp_party'),    -- Flash of Light
    (748, 86, 0, 0, 20473, 0, 0, 'lowest_hp_party'),    -- Holy Shock
    (749, 87, 0, 0, 20271, 0, 0, 'enemy_primary'),      -- Judgement of Light
    (750, 88, 0, 0, 20165, 0, 0, 'self'),               -- Seal of Light
    (751, 89, 0, 0, 26573, 0, 0, 'self'),               -- Consecration
    (752, 90, 0, 0, 879,   0, 0, 'enemy_primary'),      -- Exorcism
    (753, 90, 1, 0, 35395, 0, 0, 'enemy_primary')       -- Crusader Strike
ON DUPLICATE KEY UPDATE
    spell_base_id = VALUES(spell_base_id), target_key = VALUES(target_key);

INSERT INTO living_world_bot_combat_default_condition
    (condition_id, entry_id, sequence, subject_key, stat_key, comparison, numeric_value, string_value)
VALUES
    (711, 83, 0, 'owner',           'aura',   7, 0.0,  '53563'), -- NOT Beacon of Light
    (712, 84, 0, 'lowest_hp_party', 'hp_pct', 3, 40.0, ''),
    (713, 85, 0, 'lowest_hp_party', 'hp_pct', 3, 75.0, ''),
    (714, 86, 0, 'lowest_hp_party', 'hp_pct', 3, 85.0, ''),
    (715, 88, 0, 'self',            'aura',   7, 0.0,  '20165') -- NOT Seal of Light
ON DUPLICATE KEY UPDATE
    subject_key = VALUES(subject_key), stat_key = VALUES(stat_key),
    comparison  = VALUES(comparison),  numeric_value = VALUES(numeric_value),
    string_value = VALUES(string_value);

-- =========================================================================
-- PROFILE 12 — Druid Restoration  (replace old mixed-class entries)
-- =========================================================================
DELETE FROM living_world_bot_combat_default_condition
    WHERE entry_id IN (SELECT entry_id FROM living_world_bot_combat_default_entry WHERE default_profile_id = 12);
DELETE FROM living_world_bot_combat_default_action
    WHERE entry_id IN (SELECT entry_id FROM living_world_bot_combat_default_entry WHERE default_profile_id = 12);
DELETE FROM living_world_bot_combat_default_entry WHERE default_profile_id = 12;

INSERT INTO living_world_bot_combat_default_profile
    (default_profile_id, spec_key, role_key, class_key, display_name,
     conservation_mode, resource_low_water, resource_high_water,
     enable_down_rank, down_rank_floor,
     default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius)
VALUES
    (12, 'Restoration', 'HEAL', 'Druid', 'Druid Restoration', 1, 35, 60, 1, 2, 0, 3, 12.0)
ON DUPLICATE KEY UPDATE
    display_name            = VALUES(display_name),
    class_key               = VALUES(class_key),
    conservation_mode       = VALUES(conservation_mode),
    resource_low_water          = VALUES(resource_low_water),
    resource_high_water         = VALUES(resource_high_water);

-- Priority order:
--   5   Lifebloom stack on owner (< 3 stacks)
--   10  Regrowth emergency when party < 40%
--   20  Rejuvenation HoT when party < 80% and no Rejuv active
--   30  Wild Growth AoE when 3+ members below 70%
--   40  Healing Touch moderate when party < 70%
--   50  Moonfire DoT (offense, only if not applied)
--   60  Wrath filler
INSERT INTO living_world_bot_combat_default_entry
    (entry_id, default_profile_id, priority, label,
     is_interrupt, breaks_current_cast, enabled, condition_logic)
VALUES
    (98,  12,  5, 'Lifebloom',              0, 0, 1, 0),
    (99,  12, 10, 'Regrowth Emergency',     0, 0, 1, 0),
    (100, 12, 20, 'Rejuvenation HoT',       0, 0, 1, 0),
    (101, 12, 30, 'Wild Growth AoE',        0, 0, 1, 0),
    (102, 12, 40, 'Healing Touch Moderate', 0, 0, 1, 0),
    (103, 12, 50, 'Moonfire',               0, 0, 1, 0),
    (104, 12, 60, 'Wrath',                  0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    default_profile_id = VALUES(default_profile_id), priority = VALUES(priority), label = VALUES(label);

INSERT INTO living_world_bot_combat_default_action
    (action_id, entry_id, slot, action_type, spell_base_id, rank_mode, rank_value, target_key)
VALUES
    (765, 98,  0, 0, 33763, 0, 0, 'owner'),             -- Lifebloom
    (766, 98,  1, 0, 774,   0, 0, 'owner'),             -- Rejuvenation fallback
    (767, 99,  0, 0, 8936,  0, 0, 'lowest_hp_party'),   -- Regrowth
    (768, 99,  1, 0, 5185,  0, 0, 'lowest_hp_party'),   -- Healing Touch
    (769, 100, 0, 0, 774,   0, 0, 'lowest_hp_party'),   -- Rejuvenation
    (770, 101, 0, 0, 48438, 0, 0, 'lowest_hp_party'),   -- Wild Growth
    (771, 102, 0, 0, 5185,  0, 0, 'lowest_hp_party'),   -- Healing Touch
    (772, 102, 1, 0, 8936,  0, 0, 'lowest_hp_party'),   -- Regrowth
    (773, 103, 0, 0, 8921,  0, 0, 'enemy_primary'),     -- Moonfire
    (774, 104, 0, 0, 5176,  0, 0, 'enemy_primary')      -- Wrath
ON DUPLICATE KEY UPDATE
    spell_base_id = VALUES(spell_base_id), target_key = VALUES(target_key);

INSERT INTO living_world_bot_combat_default_condition
    (condition_id, entry_id, sequence, subject_key, stat_key, comparison, numeric_value, string_value)
VALUES
    (722, 98,  0, 'owner',           'aura_stacks', 3, 3.0,  '33763'), -- Lifebloom stacks < 3
    (723, 99,  0, 'lowest_hp_party', 'hp_pct',      3, 40.0, ''),
    (724, 100, 0, 'lowest_hp_party', 'hp_pct',      3, 80.0, ''),
    (725, 100, 1, 'lowest_hp_party', 'aura',        7, 0.0,  '774'),   -- NOT Rejuvenation
    (726, 101, 0, 'lowest_hp_party', 'hp_pct',      3, 70.0, ''),
    (727, 102, 0, 'lowest_hp_party', 'hp_pct',      3, 70.0, ''),
    (728, 103, 0, 'enemy_primary',   'aura',        7, 0.0,  '8921')   -- NOT Moonfire
ON DUPLICATE KEY UPDATE
    subject_key = VALUES(subject_key), stat_key = VALUES(stat_key),
    comparison  = VALUES(comparison),  numeric_value = VALUES(numeric_value),
    string_value = VALUES(string_value);

-- =========================================================================
-- PROFILE 31 — Priest Holy
-- Priority order:
--   5   PW:Shield self when bot hp < 50%
--   10  Flash Heal emergency when party < 40%
--   20  Renew HoT when party < 80% and no Renew active
--   30  Prayer of Healing when 3+ party below 70%
--   40  PW:Shield party member < 60% (no shield)
--   50  Shadow Word: Pain DoT
--   60  Holy Fire primary / Smite secondary
-- =========================================================================
INSERT INTO living_world_bot_combat_default_entry
    (entry_id, default_profile_id, priority, label,
     is_interrupt, breaks_current_cast, enabled, condition_logic)
VALUES
    (76, 31,  5, 'PW:Shield Self',        0, 1, 1, 0),
    (77, 31, 10, 'Emergency Flash Heal',  0, 0, 1, 0),
    (78, 31, 20, 'Renew HoT',             0, 0, 1, 0),
    (79, 31, 30, 'Prayer of Healing AoE', 0, 0, 1, 0),
    (80, 31, 40, 'PW:Shield Party',       0, 1, 1, 0),
    (81, 31, 50, 'Shadow Word: Pain',     0, 0, 1, 0),
    (82, 31, 60, 'Holy Fire / Smite',     0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    default_profile_id = VALUES(default_profile_id), priority = VALUES(priority), label = VALUES(label);

INSERT INTO living_world_bot_combat_default_action
    (action_id, entry_id, slot, action_type, spell_base_id, rank_mode, rank_value, target_key)
VALUES
    (734, 76, 0, 0, 17,    0, 0, 'self'),               -- PW:Shield self
    (735, 77, 0, 0, 2061,  0, 0, 'lowest_hp_party'),    -- Flash Heal
    (736, 77, 1, 0, 2060,  0, 0, 'lowest_hp_party'),    -- Greater Heal
    (737, 78, 0, 0, 139,   0, 0, 'lowest_hp_party'),    -- Renew
    (738, 79, 0, 0, 596,   0, 0, 'lowest_hp_party'),    -- Prayer of Healing
    (739, 80, 0, 0, 17,    0, 0, 'lowest_hp_party'),    -- PW:Shield party
    (740, 81, 0, 0, 589,   0, 0, 'enemy_primary'),      -- Shadow Word: Pain
    (741, 82, 0, 0, 14914, 0, 0, 'enemy_primary'),      -- Holy Fire
    (742, 82, 1, 0, 585,   0, 0, 'enemy_primary')       -- Smite
ON DUPLICATE KEY UPDATE
    spell_base_id = VALUES(spell_base_id), target_key = VALUES(target_key);

INSERT INTO living_world_bot_combat_default_condition
    (condition_id, entry_id, sequence, subject_key, stat_key, comparison, numeric_value, string_value)
VALUES
    (703, 76, 0, 'self',            'hp_pct', 3, 50.0, ''),
    (704, 76, 1, 'self',            'aura',   7, 0.0,  '17'),   -- NOT PW:Shield
    (705, 77, 0, 'lowest_hp_party', 'hp_pct', 3, 40.0, ''),
    (706, 78, 0, 'lowest_hp_party', 'hp_pct', 3, 80.0, ''),
    (707, 78, 1, 'lowest_hp_party', 'aura',   7, 0.0,  '139'), -- NOT Renew
    (708, 79, 0, 'lowest_hp_party', 'hp_pct', 3, 70.0, ''),
    (709, 80, 0, 'lowest_hp_party', 'hp_pct', 3, 60.0, ''),
    (710, 80, 1, 'lowest_hp_party', 'aura',   7, 0.0,  '17')   -- NOT PW:Shield
ON DUPLICATE KEY UPDATE
    subject_key = VALUES(subject_key), stat_key = VALUES(stat_key),
    comparison  = VALUES(comparison),  numeric_value = VALUES(numeric_value),
    string_value = VALUES(string_value);

-- =========================================================================
-- PROFILE 33 — Shaman Restoration
-- Priority order:
--   5   Earth Shield on owner (keep active)
--   10  Riptide + LHW emergency when party < 40%
--   20  LHW urgent when party < 55%
--   30  Chain Heal when 3+ party below 70%
--   40  Healing Wave moderate when party < 80%
--   50  Flame Shock DoT (only if not applied)
--   60  Earth Shock primary / Lightning Bolt secondary
-- =========================================================================
INSERT INTO living_world_bot_combat_default_entry
    (entry_id, default_profile_id, priority, label,
     is_interrupt, breaks_current_cast, enabled, condition_logic)
VALUES
    (91, 33,  5, 'Earth Shield',          0, 1, 1, 0),
    (92, 33, 10, 'Riptide Emergency',     0, 1, 1, 0),
    (93, 33, 20, 'LHW Urgent',            0, 0, 1, 0),
    (94, 33, 30, 'Chain Heal AoE',        0, 0, 1, 0),
    (95, 33, 40, 'Healing Wave Moderate', 0, 0, 1, 0),
    (96, 33, 50, 'Flame Shock',           0, 0, 1, 0),
    (97, 33, 60, 'Earth Shock / LB',      0, 0, 1, 0)
ON DUPLICATE KEY UPDATE
    default_profile_id = VALUES(default_profile_id), priority = VALUES(priority), label = VALUES(label);

INSERT INTO living_world_bot_combat_default_action
    (action_id, entry_id, slot, action_type, spell_base_id, rank_mode, rank_value, target_key)
VALUES
    (754, 91, 0, 0, 974,   0, 0, 'owner'),              -- Earth Shield
    (755, 92, 0, 0, 61295, 0, 0, 'lowest_hp_party'),    -- Riptide
    (756, 92, 1, 0, 8004,  0, 0, 'lowest_hp_party'),    -- Lesser Healing Wave
    (757, 93, 0, 0, 8004,  0, 0, 'lowest_hp_party'),    -- Lesser Healing Wave
    (758, 93, 1, 0, 331,   0, 0, 'lowest_hp_party'),    -- Healing Wave
    (759, 94, 0, 0, 1064,  0, 0, 'lowest_hp_party'),    -- Chain Heal
    (760, 95, 0, 0, 331,   0, 0, 'lowest_hp_party'),    -- Healing Wave
    (761, 95, 1, 0, 8004,  0, 0, 'lowest_hp_party'),    -- Lesser Healing Wave
    (762, 96, 0, 0, 8050,  0, 0, 'enemy_primary'),      -- Flame Shock
    (763, 97, 0, 0, 8042,  0, 0, 'enemy_primary'),      -- Earth Shock
    (764, 97, 1, 0, 403,   0, 0, 'enemy_primary')       -- Lightning Bolt
ON DUPLICATE KEY UPDATE
    spell_base_id = VALUES(spell_base_id), target_key = VALUES(target_key);

INSERT INTO living_world_bot_combat_default_condition
    (condition_id, entry_id, sequence, subject_key, stat_key, comparison, numeric_value, string_value)
VALUES
    (716, 91, 0, 'owner',           'aura',   7, 0.0,  '974'),  -- NOT Earth Shield
    (717, 92, 0, 'lowest_hp_party', 'hp_pct', 3, 40.0, ''),
    (718, 93, 0, 'lowest_hp_party', 'hp_pct', 3, 55.0, ''),
    (719, 94, 0, 'lowest_hp_party', 'hp_pct', 3, 70.0, ''),
    (720, 95, 0, 'lowest_hp_party', 'hp_pct', 3, 80.0, ''),
    (721, 96, 0, 'enemy_primary',   'aura',   7, 0.0,  '8050') -- NOT Flame Shock
ON DUPLICATE KEY UPDATE
    subject_key = VALUES(subject_key), stat_key = VALUES(stat_key),
    comparison  = VALUES(comparison),  numeric_value = VALUES(numeric_value),
    string_value = VALUES(string_value);
