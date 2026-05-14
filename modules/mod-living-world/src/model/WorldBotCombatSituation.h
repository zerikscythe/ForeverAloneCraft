#pragma once

#include "model/WorldBotCombatPosture.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct WorldBotHazardSnapshot
{
    bool active = false;
    float severity = 0.0f;
};

struct WorldBotCombatSituation
{
    float targetDistance = 0.0f;
    float healthPct = 100.0f;
    float manaPct = 100.0f;
    std::uint32_t nearbyHostiles = 0;
    bool hasVictim = false;
    bool canCastSafely = true;
    bool isRangedStyle = false;
    bool isHealerStyle = false;
    bool isTankStyle = false;
    WorldBotHazardSnapshot hazard;
};

struct WorldBotMovementDecision
{
    WorldBotMovementDecisionSource source = WorldBotMovementDecisionSource::None;
    WorldBotCombatPosture posture = WorldBotCombatPosture::Hold;
    bool allowHardCasts = true;

    [[nodiscard]] bool IsOverride() const
    {
        return source == WorldBotMovementDecisionSource::HazardOverride;
    }
};
} // namespace model
} // namespace living_world