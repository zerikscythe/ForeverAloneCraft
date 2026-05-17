#pragma once

#include "service/BotActivitySessionComposer.h"
#include "service/WorldBotRoutePlanning.h"
#include "service/WorldBotTaxiPlanning.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

namespace living_world
{
namespace ai
{

struct AbstractWorldBotProgressState
{
    std::size_t   currentStep    = 0;
    std::uint32_t stepElapsedMs  = 0;
    bool          stepStartKnown = false;
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
    service::WorldBotRoutePlanResolver routePlanResolver;
    std::function<std::optional<service::WorldBotResolvedTravelOption>(
        service::AmbientStep const&,
        std::uint16_t,
        float,
        float,
        float)> travelOptionResolver;
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

enum class AbstractWorldBotTravelPhaseKind : std::uint8_t
{
    None = 0,
    GroundOnly = 1,
    TaxiSourceGround = 2,
    TaxiFlight = 3,
    TaxiDestinationGround = 4,
};

struct AbstractWorldBotTravelPhase
{
    AbstractWorldBotTravelPhaseKind kind = AbstractWorldBotTravelPhaseKind::None;
    AbstractWorldBotInterpolatedPosition position;
    std::uint32_t zoneId = 0;
};

inline std::optional<service::WorldBotResolvedTravelOption> ResolveAbstractWorldBotTravelOption(
    service::AmbientStep const& step,
    AbstractWorldBotProgressState const& state,
    AbstractWorldBotProgressConfig const& config)
{
    if (step.type != service::AmbientStepType::Travel || !state.stepStartKnown || state.stepStartMapId != step.mapId)
        return std::nullopt;

    if (!config.travelOptionResolver)
        return std::nullopt;

    return config.travelOptionResolver(
        step,
        state.stepStartMapId,
        state.stepStartX,
        state.stepStartY,
        state.stepStartZ);
}

inline std::optional<AbstractWorldBotTravelPhase> ResolveAbstractWorldBotTravelOptionPhase(
    service::WorldBotResolvedTravelOption const& option,
    AbstractWorldBotProgressState const& state)
{
    if (option.usesTaxi() && option.taxiJourney.has_value() && !option.taxiJourney->empty())
    {
        service::WorldBotResolvedTaxiJourney const& journey = *option.taxiJourney;
        std::uint32_t const sourceMs = journey.sourceGroundPlan.etaMs;
        std::uint32_t const taxiMs = journey.taxiCandidate.route.totalEtaMs;
        std::uint32_t const destinationMs = journey.destinationGroundPlan.etaMs;

        if (sourceMs > 0 && state.stepElapsedMs < sourceMs)
        {
            auto const sample = service::SampleWorldBotTravelPlanPosition(
                journey.sourceGroundPlan,
                state.stepStartX,
                state.stepStartY,
                state.stepStartZ,
                journey.sourceGroundPlan.totalDistanceYards
                    * std::clamp(
                        static_cast<float>(state.stepElapsedMs) / static_cast<float>(sourceMs),
                        0.0f,
                        1.0f));
            return AbstractWorldBotTravelPhase{
                AbstractWorldBotTravelPhaseKind::TaxiSourceGround,
                {sample.mapId, sample.x, sample.y, sample.z},
                journey.taxiCandidate.sourceNode.zoneId};
        }

        if (taxiMs > 0 && state.stepElapsedMs < (sourceMs + taxiMs))
        {
            float const taxiProgress = std::clamp(
                static_cast<float>(state.stepElapsedMs - sourceMs) / static_cast<float>(taxiMs),
                0.0f,
                1.0f);
            service::WorldBotTaxiNode const& sourceNode = journey.taxiCandidate.sourceNode;
            service::WorldBotTaxiNode const& destinationNode = journey.taxiCandidate.destinationNode;
            return AbstractWorldBotTravelPhase{
                AbstractWorldBotTravelPhaseKind::TaxiFlight,
                {
                    sourceNode.mapId,
                    sourceNode.x + ((destinationNode.x - sourceNode.x) * taxiProgress),
                    sourceNode.y + ((destinationNode.y - sourceNode.y) * taxiProgress),
                    sourceNode.z + ((destinationNode.z - sourceNode.z) * taxiProgress)
                },
                destinationNode.zoneId};
        }

        service::WorldBotTaxiNode const& destinationNode = journey.taxiCandidate.destinationNode;
        if (destinationMs > 0)
        {
            std::uint32_t const destinationElapsedMs =
                state.stepElapsedMs > (sourceMs + taxiMs)
                    ? (state.stepElapsedMs - sourceMs - taxiMs)
                    : 0u;
            auto const sample = service::SampleWorldBotTravelPlanPosition(
                journey.destinationGroundPlan,
                destinationNode.x,
                destinationNode.y,
                destinationNode.z,
                journey.destinationGroundPlan.totalDistanceYards
                    * std::clamp(
                        static_cast<float>(destinationElapsedMs) / static_cast<float>(destinationMs),
                        0.0f,
                        1.0f));
            return AbstractWorldBotTravelPhase{
                AbstractWorldBotTravelPhaseKind::TaxiDestinationGround,
                {sample.mapId, sample.x, sample.y, sample.z},
                destinationNode.zoneId};
        }

        return AbstractWorldBotTravelPhase{
            AbstractWorldBotTravelPhaseKind::TaxiDestinationGround,
            {destinationNode.mapId, destinationNode.x, destinationNode.y, destinationNode.z},
            destinationNode.zoneId};
    }

    if (option.groundPlan.has_value() && !option.groundPlan->empty() && option.groundPlan->etaMs > 0)
    {
        auto const sample = service::SampleWorldBotTravelPlanPosition(
            *option.groundPlan,
            state.stepStartX,
            state.stepStartY,
            state.stepStartZ,
            option.groundPlan->totalDistanceYards
                * std::clamp(
                    static_cast<float>(state.stepElapsedMs) / static_cast<float>(option.groundPlan->etaMs),
                    0.0f,
                    1.0f));
        return AbstractWorldBotTravelPhase{
            AbstractWorldBotTravelPhaseKind::GroundOnly,
            {sample.mapId, sample.x, sample.y, sample.z},
            option.groundPlan->zoneId};
    }

    return std::nullopt;
}

inline std::optional<AbstractWorldBotInterpolatedPosition> ComputeAbstractWorldBotTravelOptionPosition(
    service::WorldBotResolvedTravelOption const& option,
    AbstractWorldBotProgressState const& state)
{
    if (auto const phase = ResolveAbstractWorldBotTravelOptionPhase(option, state))
        return phase->position;

    return std::nullopt;
}

inline std::uint32_t ComputeAbstractWorldBotStepDurationMs(
    service::AmbientStep const& step,
    AbstractWorldBotProgressState const& state,
    AbstractWorldBotProgressConfig const& config = {})
{
    if (step.type == service::AmbientStepType::Transit)
        return std::max(config.minStepDurationMs, step.durationSec * 1000u);

    if (step.type == service::AmbientStepType::Travel)
    {
        if (!state.stepStartKnown || state.stepStartMapId != step.mapId)
            return std::max(config.minStepDurationMs, config.crossMapTravelMs);

        if (auto const option = ResolveAbstractWorldBotTravelOption(step, state, config))
        {
            if (option->totalEtaMs > 0)
                return std::max(config.minStepDurationMs, option->totalEtaMs);
        }

        if (config.routePlanResolver)
        {
            if (auto const plan = config.routePlanResolver(
                step,
                state.stepStartMapId,
                state.stepStartX,
                state.stepStartY,
                state.stepStartZ))
            {
                if (plan->totalDistanceYards > 0.0f && plan->speedYardsPerSecond > 0.0f)
                {
                    std::uint32_t const ms = static_cast<std::uint32_t>(
                        (plan->totalDistanceYards / plan->speedYardsPerSecond) * 1000.0f);
                    return std::max(config.minStepDurationMs, ms);
                }
            }
        }

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

    if (step.type == service::AmbientStepType::Transit)
    {
        if (!state.stepStartKnown)
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

        if (state.stepStartMapId != step.mapId)
        {
            pos.mapId = progress >= 1.0f ? step.mapId : state.stepStartMapId;
            pos.x = progress >= 1.0f ? step.x : state.stepStartX;
            pos.y = progress >= 1.0f ? step.y : state.stepStartY;
            pos.z = progress >= 1.0f ? step.z : state.stepStartZ;
            return pos;
        }

        pos.mapId = state.stepStartMapId;
        pos.x = state.stepStartX + ((step.x - state.stepStartX) * progress);
        pos.y = state.stepStartY + ((step.y - state.stepStartY) * progress);
        pos.z = state.stepStartZ + ((step.z - state.stepStartZ) * progress);
        return pos;
    }

    if (step.type != service::AmbientStepType::Travel || !state.stepStartKnown || state.stepStartMapId != step.mapId)
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

    if (auto const option = ResolveAbstractWorldBotTravelOption(step, state, config))
    {
        if (auto const sample = ComputeAbstractWorldBotTravelOptionPosition(*option, state))
            return *sample;
    }

    if (config.routePlanResolver)
    {
        if (auto const plan = config.routePlanResolver(
            step,
            state.stepStartMapId,
            state.stepStartX,
            state.stepStartY,
            state.stepStartZ))
        {
            if (!plan->empty() && plan->totalDistanceYards > 0.0f)
            {
                auto const sample = service::SampleWorldBotTravelPlanPosition(
                    *plan,
                    state.stepStartX,
                    state.stepStartY,
                    state.stepStartZ,
                    plan->totalDistanceYards * progress);
                pos.mapId = sample.mapId;
                pos.x = sample.x;
                pos.y = sample.y;
                pos.z = sample.z;
                return pos;
            }
        }
    }

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
        state.stepStartKnown = true;
        state.stepElapsedMs = 0;
        ++state.currentStep;
    }

    outcome.sessionComplete = state.currentStep >= session.steps.size();
    return outcome;
}

} // namespace ai
} // namespace living_world
