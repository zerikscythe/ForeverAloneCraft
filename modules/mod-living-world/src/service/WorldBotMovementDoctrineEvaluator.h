#pragma once

#include "model/WorldBotCombatSituation.h"

namespace living_world
{
namespace service
{
inline model::WorldBotMovementDecision EvaluateWorldBotMovementDoctrine(
    model::WorldBotCombatSituation const& situation)
{
    model::WorldBotMovementDecision decision;

    bool const strictHazardContext =
        situation.environment == model::WorldBotCombatEnvironment::DungeonOrRaid;
    bool const worldRepeatedDamageEscapeAllowed =
        situation.isHealerStyle || situation.isRangedStyle;
    bool const shouldHonorHazardOverride =
        situation.hazard.active
        && (strictHazardContext
            || situation.hazard.explicitAuraTriggered
            || (situation.hazard.repeatedDamageTriggered && worldRepeatedDamageEscapeAllowed));

    if (shouldHonorHazardOverride)
    {
        decision.source = model::WorldBotMovementDecisionSource::HazardOverride;
        decision.posture = model::WorldBotCombatPosture::HazardEscape;
        decision.allowHardCasts = false;
        return decision;
    }

    decision.source = model::WorldBotMovementDecisionSource::PostureDoctrine;

    if (!situation.hasVictim)
    {
        decision.posture = model::WorldBotCombatPosture::Hold;
        decision.allowHardCasts = true;
        return decision;
    }

    if (situation.isTankStyle)
    {
        decision.posture = (situation.inEffectiveMeleeRange || situation.targetDistance <= 4.0f)
            ? model::WorldBotCombatPosture::Hold
            : model::WorldBotCombatPosture::Close;
        decision.allowHardCasts = true;
        return decision;
    }

    if (!situation.isRangedStyle && !situation.isHealerStyle)
    {
        decision.posture = (situation.inEffectiveMeleeRange || situation.targetDistance <= 4.0f)
            ? model::WorldBotCombatPosture::Hold
            : model::WorldBotCombatPosture::Close;
        decision.allowHardCasts = true;
        return decision;
    }

    if (situation.isHealerStyle)
    {
        if (situation.healthPct < 35.0f)
        {
            decision.posture = model::WorldBotCombatPosture::Retreat;
            decision.allowHardCasts = false;
            return decision;
        }

        decision.posture = situation.targetDistance < 12.0f
            ? model::WorldBotCombatPosture::Reposition
            : model::WorldBotCombatPosture::Hold;
        decision.allowHardCasts = decision.posture == model::WorldBotCombatPosture::Hold;
        return decision;
    }

    if (situation.isRangedStyle)
    {
        if (situation.healthPct < 35.0f)
        {
            decision.posture = model::WorldBotCombatPosture::Retreat;
            decision.allowHardCasts = false;
            return decision;
        }

        if (situation.targetDistance < 10.0f)
        {
            decision.posture = model::WorldBotCombatPosture::Kite;
            decision.allowHardCasts = false;
            return decision;
        }

        if (situation.targetDistance > 30.0f)
        {
            decision.posture = model::WorldBotCombatPosture::Close;
            decision.allowHardCasts = false;
            return decision;
        }

        decision.posture = model::WorldBotCombatPosture::Hold;
        decision.allowHardCasts = situation.canCastSafely;
        return decision;
    }

    decision.posture = model::WorldBotCombatPosture::Hold;
    decision.allowHardCasts = true;
    return decision;
}
} // namespace service
} // namespace living_world
