#pragma once

#include <cstdint>
#include <string>

namespace living_world
{
namespace model
{

// One row from living_world_hazard_auras.
struct HazardAuraEntry
{
    uint32_t    spellId  = 0;
    float       severity = 1.0f;
    std::string notes;
};

// Hazard-sensor role assigned to a bot.
// Maps the bot's class (and aggro state) to a behaviour bucket.
// TANK         — Warriors/DKs holding aggro
// HEALER       — Pure healers (Priest)
// HYBRID_HEALER— Healer hybrids (Paladin/Shaman/Druid)
// MELEE_DPS    — Melee damage dealers
// RANGED_DPS   — Ranged/caster damage dealers
enum class HazardRole : uint8_t
{
    Tank        = 0,
    Healer      = 1,
    HybridHealer= 2,
    MeleeDps    = 3,
    RangedDps   = 4,
};

// One row from living_world_hazard_role_rules.
// Controls whether a role participates in hazard escape and under what conditions.
struct HazardRoleRule
{
    std::string roleKey;              // TANK / HEALER / HYBRID_HEALER / MELEE_DPS / RANGED_DPS

    // When true: this role skips hazard escape.
    bool        skipEscape          = false;

    // If > 0: suppress escape when owner HP% is below this threshold.
    float       ownerHpGatePct      = 0.0f;

    // If true (TANK only): skip only while actively holding aggro on the current
    // target. Allows a tank to escape fire when NOT tanking a boss.
    bool        requiresAggroToSkip = false;
};

// Tuning constants loaded from living_world_hazard_config key/value rows.
struct HazardTuning
{
    float   damageThresholdPct     = 2.0f;
    int     consecutiveDamageTicks = 2;
    float   maxMovementYards       = 2.0f;
    float   anchorSearchRadius     = 40.0f;
    float   escapeStepYards        = 5.0f;
    int     commitWindowMs         = 2000;
};

} // namespace model
} // namespace living_world
