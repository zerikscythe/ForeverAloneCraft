#pragma once

#include "model/WorldBotCombatSituation.h"
#include "model/WorldBotPreparedBuild.h"

#include "SharedDefines.h"

namespace living_world
{
namespace service
{
inline model::WorldBotMovementStyle ResolveWorldBotMovementStyle(
    model::WorldBotPreparedBuild const& build)
{
    if (build.resolvedRoleKey == "TANK")
        return model::WorldBotMovementStyle::FrontlineTank;

    if (build.resolvedRoleKey == "HEAL")
        return model::WorldBotMovementStyle::BacklineHealer;

    if (build.classId == CLASS_HUNTER)
        return model::WorldBotMovementStyle::MobileRanged;

    if (build.classId == CLASS_MAGE
        || build.classId == CLASS_WARLOCK
        || build.canonicalSpecKey == "Balance"
        || build.canonicalSpecKey == "Elemental"
        || build.canonicalSpecKey == "Shadow")
    {
        return model::WorldBotMovementStyle::TurretCaster;
    }

    if (build.classId == CLASS_PRIEST && build.canonicalSpecKey != "Shadow")
        return model::WorldBotMovementStyle::BacklineHealer;

    return model::WorldBotMovementStyle::StickyMelee;
}

inline bool ResolveWorldBotCastSafety(
    model::WorldBotMovementStyle movementStyle,
    float targetDistance,
    std::uint32_t nearbyHostiles)
{
    if (movementStyle == model::WorldBotMovementStyle::TurretCaster
        || movementStyle == model::WorldBotMovementStyle::BacklineHealer
        || movementStyle == model::WorldBotMovementStyle::MobileRanged)
    {
        return targetDistance >= 10.0f && targetDistance <= 30.0f && nearbyHostiles <= 1u;
    }

    return true;
}

inline model::WorldBotCombatSituation BuildWorldBotCombatSituation(
    model::WorldBotPreparedBuild const& build,
    bool hasVictim,
    float targetDistance,
    float healthPct,
    float manaPct,
    std::uint32_t nearbyHostiles,
    model::WorldBotHazardSnapshot const& hazard = {})
{
    model::WorldBotCombatSituation situation;
    situation.movementStyle = ResolveWorldBotMovementStyle(build);
    situation.hasVictim = hasVictim;
    situation.targetDistance = targetDistance;
    situation.healthPct = healthPct;
    situation.manaPct = manaPct;
    situation.nearbyHostiles = nearbyHostiles;
    situation.hazard = hazard;

    situation.isTankStyle = situation.movementStyle == model::WorldBotMovementStyle::FrontlineTank;
    situation.isHealerStyle = situation.movementStyle == model::WorldBotMovementStyle::BacklineHealer;
    situation.isRangedStyle = situation.movementStyle == model::WorldBotMovementStyle::TurretCaster
        || situation.movementStyle == model::WorldBotMovementStyle::MobileRanged
        || situation.movementStyle == model::WorldBotMovementStyle::BacklineHealer;
    situation.canCastSafely = ResolveWorldBotCastSafety(
        situation.movementStyle,
        targetDistance,
        nearbyHostiles);

    return situation;
}
} // namespace service
} // namespace living_world