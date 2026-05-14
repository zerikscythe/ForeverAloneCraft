#pragma once

#include "model/WorldBotCombatSituation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

namespace living_world
{
namespace service
{
struct WorldBotNearbyHostileSnapshot
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool engaged = false;
    bool isCurrentTarget = false;
};

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
    float safetyScore = 0.0f;
    std::uint32_t freshHostilesNearDestination = 0;
    std::uint32_t engagedHostilesNearDestination = 0;
};

inline float ScoreWorldBotEscapeDestination(
    float candidateX,
    float candidateY,
    float botX,
    float botY,
    std::span<WorldBotNearbyHostileSnapshot const> nearbyHostiles,
    std::uint32_t& freshHostilesNearDestination,
    std::uint32_t& engagedHostilesNearDestination)
{
    constexpr float FreshDangerRadius = 12.0f;
    constexpr float FreshPressureRadius = 18.0f;
    constexpr float EngagedDangerRadius = 8.0f;

    float score = 0.0f;
    for (WorldBotNearbyHostileSnapshot const& hostile : nearbyHostiles)
    {
        float const dx = candidateX - hostile.x;
        float const dy = candidateY - hostile.y;
        float const dist = std::sqrt(dx * dx + dy * dy);

        if (hostile.isCurrentTarget)
            continue;

        if (!hostile.engaged)
        {
            if (dist <= FreshDangerRadius)
            {
                ++freshHostilesNearDestination;
                score += 1000.0f + ((FreshDangerRadius - dist) * 100.0f);
            }
            else if (dist <= FreshPressureRadius)
            {
                score += 100.0f + ((FreshPressureRadius - dist) * 10.0f);
            }
        }
        else if (dist <= EngagedDangerRadius)
        {
            ++engagedHostilesNearDestination;
            score += 25.0f + ((EngagedDangerRadius - dist) * 5.0f);
        }
    }

    float const moveDx = candidateX - botX;
    float const moveDy = candidateY - botY;
    score += std::sqrt(moveDx * moveDx + moveDy * moveDy) * 0.01f;
    return score;
}

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
    float targetZ,
    std::span<WorldBotNearbyHostileSnapshot const> nearbyHostiles = {})
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

    plan.kind = WorldBotMovementPlanKind::MovePoint;

    float const baseAngle = std::atan2(dy, dx);
    float bestScore = std::numeric_limits<float>::max();
    bool foundCandidate = false;

    constexpr float QuarterTurn = 0.78539816339f;
    constexpr float HalfTurn = 1.57079632679f;

    std::span<float const> angleOffsets;
    constexpr float retreatOffsets[] = { 0.0f, QuarterTurn, -QuarterTurn, HalfTurn, -HalfTurn };
    constexpr float kiteOffsets[] = { QuarterTurn, -QuarterTurn, 0.0f, HalfTurn, -HalfTurn };
    constexpr float repositionOffsets[] = { QuarterTurn, -QuarterTurn, 0.0f };

    switch (decision.posture)
    {
        case model::WorldBotCombatPosture::Retreat:
        case model::WorldBotCombatPosture::HazardEscape:
            angleOffsets = retreatOffsets;
            break;
        case model::WorldBotCombatPosture::Kite:
            angleOffsets = kiteOffsets;
            break;
        case model::WorldBotCombatPosture::Reposition:
            angleOffsets = repositionOffsets;
            break;
        default:
            angleOffsets = retreatOffsets;
            break;
    }

    for (float angleOffset : angleOffsets)
    {
        float const angle = baseAngle + angleOffset;
        float const candidateX = targetX + std::cos(angle) * desiredRange;
        float const candidateY = targetY + std::sin(angle) * desiredRange;
        std::uint32_t freshHostilesNearDestination = 0;
        std::uint32_t engagedHostilesNearDestination = 0;
        float const score = ScoreWorldBotEscapeDestination(
            candidateX,
            candidateY,
            botX,
            botY,
            nearbyHostiles,
            freshHostilesNearDestination,
            engagedHostilesNearDestination);

        if (!foundCandidate || score < bestScore)
        {
            foundCandidate = true;
            bestScore = score;
            plan.pointX = candidateX;
            plan.pointY = candidateY;
            plan.pointZ = targetZ + (botZ - targetZ);
            plan.safetyScore = score;
            plan.freshHostilesNearDestination = freshHostilesNearDestination;
            plan.engagedHostilesNearDestination = engagedHostilesNearDestination;
        }
    }

    return plan;
}
} // namespace service
} // namespace living_world