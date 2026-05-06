#include "integration/SqlBotHazardConfigRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{

void SqlBotHazardConfigRepository::EnsureSchema() const
{
    // ---------------------------------------------------------------
    // Table 1 — hazard aura registry
    // ---------------------------------------------------------------
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_hazard_auras ("
        "  id       INT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "  spell_id INT UNSIGNED NOT NULL,"
        "  severity FLOAT        NOT NULL DEFAULT 1.0,"
        "  notes    VARCHAR(255)          DEFAULT NULL,"
        "  enabled  TINYINT(1)   NOT NULL DEFAULT 1,"
        "  PRIMARY KEY (id),"
        "  UNIQUE KEY uq_spell (spell_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");

    // Seed the auras that were previously hardcoded in BotHazardSensor.cpp.
    // INSERT IGNORE means re-running is harmless; operators can add or adjust
    // rows freely without being overwritten on restart.
    WorldDatabase.DirectExecute(
        "INSERT IGNORE INTO living_world_hazard_auras (spell_id, severity, notes) VALUES"
        "  (28524, 1.0, 'Naxxramas - Slime Pool (Grobbulus)'),"
        "  (26575, 1.0, 'Generic - Void Zone'),"
        "  (37591, 1.0, 'Serpentshrine Cavern - Toxic Spores'),"
        "  (40923, 1.0, 'Black Temple - Fel Eruption'),"
        "  (46228, 1.0, 'Sunwell - Dark Decay (Eredar Twins)'),"
        "  (63018, 1.5, 'Ulduar - Searing Flames (Ignis)'),"
        "  (64290, 1.5, 'Ulduar - Saronite Vapors (General Vezax)'),"
        "  (67480, 1.0, 'Trial of the Crusader - Firebomb (Jaraxxus)'),"
        "  (70952, 2.0, 'Icecrown Citadel - Defile (Lich King)'),"
        "  (72754, 1.5, 'Icecrown Citadel - Frozen Orb ground effect'),"
        "  (74527, 1.5, 'Ruby Sanctum - Combustion ground fire')"
    );

    // ---------------------------------------------------------------
    // Table 2 — role-level escape rules
    // ---------------------------------------------------------------
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_hazard_role_rules ("
        "  role_key               VARCHAR(20)  NOT NULL,"
        "  skip_escape            TINYINT(1)   NOT NULL DEFAULT 0,"
        "  owner_hp_gate_pct      FLOAT        NOT NULL DEFAULT 0.0,"
        "  requires_aggro_to_skip TINYINT(1)   NOT NULL DEFAULT 0,"
        "  notes                  VARCHAR(255)          DEFAULT NULL,"
        "  PRIMARY KEY (role_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");

    // role_key values: TANK, HEALER, HYBRID_HEALER, MELEE_DPS, RANGED_DPS
    WorldDatabase.DirectExecute(
        "INSERT IGNORE INTO living_world_hazard_role_rules"
        "  (role_key, skip_escape, owner_hp_gate_pct, requires_aggro_to_skip, notes) VALUES"
        "  ('TANK',         1, 0.0,  1, 'Skip escape only while actively holding aggro'),"
        "  ('HEALER',       0, 50.0, 0, 'Suppress escape when owner HP is critical'),"
        "  ('HYBRID_HEALER',0, 50.0, 0, 'Suppress escape when owner HP is critical'),"
        "  ('MELEE_DPS',    0, 0.0,  0, 'Always escape — no special handling'),"
        "  ('RANGED_DPS',   0, 0.0,  0, 'Always escape — no special handling')"
    );

    // ---------------------------------------------------------------
    // Table 3 — tuning constants (key/value)
    // ---------------------------------------------------------------
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_hazard_config ("
        "  config_key   VARCHAR(64) NOT NULL,"
        "  config_value FLOAT       NOT NULL,"
        "  notes        VARCHAR(255)         DEFAULT NULL,"
        "  PRIMARY KEY (config_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");

    WorldDatabase.DirectExecute(
        "INSERT IGNORE INTO living_world_hazard_config (config_key, config_value, notes) VALUES"
        "  ('damage_threshold_pct',      2.0,    'HP%% drop per 500ms tick to count as taking damage'),"
        "  ('consecutive_damage_ticks',  2.0,    'Consecutive damage ticks before declaring layer-2 danger'),"
        "  ('max_movement_yards',        2.0,    'Max movement between ticks before ignoring HP drop'),"
        "  ('anchor_search_radius',     40.0,    'Yards to search for a clean party anchor'),"
        "  ('escape_step_yards',         5.0,    'Yards to step toward anchor per escape tick'),"
        "  ('commit_window_ms',       2000.0,    'Ms to keep the same anchor before re-evaluating')"
    );
}

std::vector<model::HazardAuraEntry>
SqlBotHazardConfigRepository::LoadHazardAuras() const
{
    std::vector<model::HazardAuraEntry> result;

    QueryResult qr = WorldDatabase.Query(
        "SELECT spell_id, severity, COALESCE(notes, '') "
        "FROM living_world_hazard_auras "
        "WHERE enabled = 1");
    if (!qr)
        return result;

    do
    {
        Field const* f = qr->Fetch();
        model::HazardAuraEntry entry;
        entry.spellId  = f[0].Get<uint32_t>();
        entry.severity = f[1].Get<float>();
        entry.notes    = f[2].Get<std::string>();
        result.push_back(std::move(entry));
    } while (qr->NextRow());

    return result;
}

std::vector<model::HazardRoleRule>
SqlBotHazardConfigRepository::LoadRoleRules() const
{
    std::vector<model::HazardRoleRule> result;

    QueryResult qr = WorldDatabase.Query(
        "SELECT role_key, skip_escape, owner_hp_gate_pct, requires_aggro_to_skip "
        "FROM living_world_hazard_role_rules");
    if (!qr)
        return result;

    do
    {
        Field const* f = qr->Fetch();
        model::HazardRoleRule rule;
        rule.roleKey             = f[0].Get<std::string>();
        rule.skipEscape          = f[1].Get<uint8_t>() != 0;
        rule.ownerHpGatePct      = f[2].Get<float>();
        rule.requiresAggroToSkip = f[3].Get<uint8_t>() != 0;
        result.push_back(std::move(rule));
    } while (qr->NextRow());

    return result;
}

model::HazardTuning
SqlBotHazardConfigRepository::LoadTuning() const
{
    model::HazardTuning tuning; // defaults from struct

    QueryResult qr = WorldDatabase.Query(
        "SELECT config_key, config_value FROM living_world_hazard_config");
    if (!qr)
        return tuning;

    do
    {
        Field const* f = qr->Fetch();
        std::string const key   = f[0].Get<std::string>();
        float       const value = f[1].Get<float>();

        if      (key == "damage_threshold_pct")
            tuning.damageThresholdPct = value;
        else if (key == "consecutive_damage_ticks")
            tuning.consecutiveDamageTicks = static_cast<int>(value);
        else if (key == "max_movement_yards")
            tuning.maxMovementYards = value;
        else if (key == "anchor_search_radius")
            tuning.anchorSearchRadius = value;
        else if (key == "escape_step_yards")
            tuning.escapeStepYards = value;
        else if (key == "commit_window_ms")
            tuning.commitWindowMs = static_cast<int>(value);
    } while (qr->NextRow());

    return tuning;
}

} // namespace integration
} // namespace living_world
