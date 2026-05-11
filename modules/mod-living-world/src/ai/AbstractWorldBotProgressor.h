#pragma once

#include "service/BotActivitySessionComposer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace living_world
{
namespace ai
{

struct AbstractWorldBotProgressState
{
    std::size_t   currentStep    = 0;
    std::uint32_t stepElapsedMs  = 0;
    std::uint16_t stepStartMapId = 0;
    float         stepStartX     = 0.0f;
    float         stepStartY     = 0.0f;
    float         stepStartZ     = 0.0f;
};

struct AbstractWorldBotProgressConfig
{
    float         travelYardsPerSecond   = 4.5f;
    std::uint32_t minStepDurationMs      = 1000;
    std::uint32_t crossMapTravelMs       = 30000;
};

struct AbstractWorldBotInterpolatedPosition
{
    std::uint16_t mapId = 0;
    float         x     = 0.0f;
    float         y     = 0.0f;
    float         z     = 0.0f;
};

struct AbstractWorldBotProgressOutcome
{
    bool          advancedStep   = false;
    bool          sessionComplete = false;
    std::uint32_t stepsCompleted = 0;
};

inline std::uint32_t ComputeAbstractWorldBotStepDurationMs(
    service::AmbientStep const& step,
    AbstractWorldBotProgressState const& state,
    AbstractWorldBotProgressConfig const& config = {})
{
    if (step.type == service::AmbientStepType::TaxiFlight)
        return std::max(config.minStepDurationMs, step.durationSec * 1000u);

    if (step.type == service::AmbientStepType::Travel)
    {
        if (state.stepStartMapId == 0 || state.stepStartMapId != step.mapId)
            return std::max(config.minStepDurationMs, config.crossMapTravelMs);

        float const dx = step.x - state.stepStartX;
        float const dy = step.y - state.stepStartY;
        float const dz = step.z - state.stepStartZ;
        float const dist = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        float const speed = std::max(0.1f, config.travelYardsPerSecond);
        std::uint32_t const ms = static_cast<std::uint32_t>((dist / speed) * 1000.0f);
        return std::max(config.minStepDurationMs, ms);
    }

    return std::max(config.minStepDurationMs, step.durationSec * 1000u);
}

inline AbstractWorldBotInterpolatedPosition ComputeAbstractWorldBotInterpolatedPosition(
    service::AmbientSession const& session,
    AbstractWorldBotProgressState const& state,
    AbstractWorldBotProgressConfig const& config = {})
{
    AbstractWorldBotInterpolatedPosition pos;
    if (state.currentStep >= session.steps.size())
    {
        pos.mapId = state.stepStartMapId;
        pos.x = state.stepStartX;
        pos.y = state.stepStartY;
        pos.z = state.stepStartZ;
        return pos;
    }

    service::AmbientStep const& step = session.steps[state.currentStep];
    pos.mapId = step.mapId;

    if (step.type != service::AmbientStepType::Travel || state.stepStartMapId != step.mapId)
    {
        pos.x = step.x;
        pos.y = step.y;
        pos.z = step.z;
        return pos;
    }

    std::uint32_t const durationMs = ComputeAbstractWorldBotStepDurationMs(step, state, config);
    float const progress = durationMs == 0
        ? 1.0f
        : std::clamp(static_cast<float>(state.stepElapsedMs) / static_cast<float>(durationMs), 0.0f, 1.0f);

    pos.x = state.stepStartX + ((step.x - state.stepStartX) * progress);
    pos.y = state.stepStartY + ((step.y - state.stepStartY) * progress);
    pos.z = state.stepStartZ + ((step.z - state.stepStartZ) * progress);
    return pos;
}

inline AbstractWorldBotProgressOutcome AdvanceAbstractWorldBotProgress(
    service::AmbientSession const& session,
    AbstractWorldBotProgressState& state,
    std::uint32_t diffMs,
    AbstractWorldBotProgressConfig const& config = {})
{
    AbstractWorldBotProgressOutcome outcome;

    while (diffMs > 0 && state.currentStep < session.steps.size())
    {
        service::AmbientStep const& step = session.steps[state.currentStep];
        std::uint32_t const durationMs = ComputeAbstractWorldBotStepDurationMs(step, state, config);
        std::uint32_t const remainingMs = durationMs > state.stepElapsedMs
            ? (durationMs - state.stepElapsedMs)
            : 0u;

        std::uint32_t const consumedMs = std::min(diffMs, remainingMs == 0 ? diffMs : remainingMs);
        state.stepElapsedMs += consumedMs;
        diffMs -= consumedMs;

        if (state.stepElapsedMs < durationMs)
            break;

        outcome.advancedStep = true;
        ++outcome.stepsCompleted;

        state.stepStartMapId = step.mapId;
        state.stepStartX = step.x;
        state.stepStartY = step.y;
        state.stepStartZ = step.z;
        state.stepElapsedMs = 0;
        ++state.currentStep;
    }

    outcome.sessionComplete = state.currentStep >= session.steps.size();
    return outcome;
}

} // namespace ai
} // namespace living_world