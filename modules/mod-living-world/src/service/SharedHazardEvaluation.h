#pragma once

#include "Position.h"
#include "model/BotHazardConfig.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace living_world
{
namespace service
{

struct SharedHazardEvaluationState
{
    bool initialized = false;
    float lastHealthPct = 100.0f;
    Position lastPosition;
    int consecutiveDamageTicks = 0;
    std::chrono::steady_clock::time_point lastDamageTick = {};
    std::chrono::steady_clock::time_point commitWindowExpiresAt = {};
    float committedSeverity = 0.0f;
    std::uint32_t committedHazardSpellId = 0;
};

struct SharedHazardEvaluationResult
{
    bool explicitAuraTriggered = false;
    bool repeatedDamageTriggered = false;
    bool dangerDetectedNow = false;
    bool commitWindowActive = false;
    std::uint32_t hazardSpellId = 0;
    int consecutiveDamageTicks = 0;
    float severity = 0.0f;
};

inline SharedHazardEvaluationResult EvaluateSharedHazard(
    SharedHazardEvaluationState& state,
    std::chrono::steady_clock::time_point now,
    float currentHealthPct,
    Position const& currentPosition,
    bool hasKnownAura,
    std::uint32_t hazardSpellId,
    float explicitAuraSeverity,
    model::HazardTuning const& tuning)
{
    SharedHazardEvaluationResult result;
    result.explicitAuraTriggered = hasKnownAura;

    float hpDrop = 0.0f;
    float moved = tuning.maxMovementYards + 1.0f;
    if (state.initialized)
    {
        hpDrop = state.lastHealthPct - currentHealthPct;
        moved = state.lastPosition.GetExactDist2d(currentPosition);
    }

    if (hpDrop > tuning.damageThresholdPct && moved < tuning.maxMovementYards)
    {
        auto const msSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - state.lastDamageTick).count();

        if (msSinceLast < 750 && state.consecutiveDamageTicks > 0)
            ++state.consecutiveDamageTicks;
        else
            state.consecutiveDamageTicks = 1;

        state.lastDamageTick = now;

        if (state.consecutiveDamageTicks >= tuning.consecutiveDamageTicks)
            result.repeatedDamageTriggered = true;
    }
    else if (hpDrop <= 0.0f)
    {
        state.consecutiveDamageTicks = 0;
    }

    result.consecutiveDamageTicks = state.consecutiveDamageTicks;
    result.dangerDetectedNow = hasKnownAura || result.repeatedDamageTriggered;

    if (result.dangerDetectedNow)
    {
        constexpr float RepeatedDamageSeverity = 0.75f;

        state.commitWindowExpiresAt =
            now + std::chrono::milliseconds(tuning.commitWindowMs);
        state.committedSeverity = std::max(
            hasKnownAura ? explicitAuraSeverity : 0.0f,
            result.repeatedDamageTriggered ? RepeatedDamageSeverity : 0.0f);
        state.committedHazardSpellId = hasKnownAura ? hazardSpellId : 0;
    }

    result.commitWindowActive = now < state.commitWindowExpiresAt;
    if (!result.dangerDetectedNow && !result.commitWindowActive)
    {
        state.committedSeverity = 0.0f;
        state.committedHazardSpellId = 0;
    }

    if (result.dangerDetectedNow || result.commitWindowActive)
    {
        result.severity = state.committedSeverity;
        result.hazardSpellId = state.committedHazardSpellId;
    }

    state.lastHealthPct = currentHealthPct;
    state.lastPosition = currentPosition;
    state.initialized = true;

    return result;
}

inline void ResetSharedHazardEvaluationState(SharedHazardEvaluationState& state)
{
    state = {};
}

} // namespace service
} // namespace living_world