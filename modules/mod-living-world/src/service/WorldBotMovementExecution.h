#pragma once

#include "model/WorldBotCombatSituation.h"

#include <algorithm>
#include <cmath>

namespace living_world
{
namespace service
{
enum class WorldBotMovementPlanKind : std::uint8_t
{
    None = 0,
    Chase = 1,
    MovePoint = 2,
};

struct WorldBotMovementExecutionPlan
{
    WorldBotMovementPlanKind kind = WorldBotMovementPlanKind::None;
    float chaseDistance = 0.0f;
    float pointX = 0.0f;
    float pointY = 0.0f;
    float pointZ = 0.0f;
};

inline float ResolveWorldBotDesiredRange(
    model::WorldBotMovementStyle style,
    model::WorldBotCombatPosture posture)
{
    switch (posture)
    {
        case model::WorldBotCombatPosture::Hold:
            switch (style)
            {
                case model::WorldBotMovementStyle::FrontlineTank:
                case model::WorldBotMovementStyle::StickyMelee:
                    return 2.0f;
                case model::WorldBotMovementStyle::BacklineHealer:
                    return 24.0f;
                case model::WorldBotMovementStyle::TurretCaster:
                    return 26.0f;
                case model::WorldBotMovementStyle::MobileRanged:
                    return 24.0f;
                default:
                    return 0.0f;
            }

        case model::WorldBotCombatPosture::Close:
            switch (style)
            {
                case model::WorldBotMovementStyle::FrontlineTank:
                case model::WorldBotMovementStyle::StickyMelee:
                    return 1.5f;
                case model::WorldBotMovementStyle::BacklineHealer:
                    return 24.0f;
                case model::WorldBotMovementStyle::TurretCaster:
                    return 26.0f;
                case model::WorldBotMovementStyle::MobileRanged:
                    return 24.0f;
                default:
                    return 2.0f;
            }

        case model::WorldBotCombatPosture::Reposition:
            return 16.0f;
        case model::WorldBotCombatPosture::Kite:
            return 20.0f;
        case model::WorldBotCombatPosture::Retreat:
            return 28.0f;
        case model::WorldBotCombatPosture::HazardEscape:
            return 30.0f;
    }

    return 0.0f;
}

inline WorldBotMovementExecutionPlan BuildWorldBotMovementExecutionPlan(
    model::WorldBotCombatSituation const& situation,
    model::WorldBotMovementDecision const& decision,
    float botX,
    float botY,
    float botZ,
    float targetX,
    float targetY,
    float targetZ)
{
    WorldBotMovementExecutionPlan plan;
    if (!situation.hasVictim)
        return plan;

    float const desiredRange = ResolveWorldBotDesiredRange(situation.movementStyle, decision.posture);

    if (decision.posture == model::WorldBotCombatPosture::Hold)
        return plan;

    if (decision.posture == model::WorldBotCombatPosture::Close)
    {
        if (desiredRange > 0.0f && situation.targetDistance <= desiredRange + 0.5f)
            return plan;

        plan.kind = WorldBotMovementPlanKind::Chase;
        plan.chaseDistance = std::max(desiredRange, 0.0f);
        return plan;
    }

    if (desiredRange > 0.0f && situation.targetDistance >= desiredRange - 0.5f)
        return plan;

    float dx = botX - targetX;
    float dy = botY - targetY;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.001f)
    {
        dx = 1.0f;
        dy = 0.0f;
        dist = 1.0f;
    }

    float const scale = desiredRange / dist;
    plan.kind = WorldBotMovementPlanKind::MovePoint;
    plan.pointX = targetX + dx * scale;
    plan.pointY = targetY + dy * scale;
    plan.pointZ = targetZ + (botZ - targetZ);
    return plan;
}
} // namespace service
} // namespace living_world