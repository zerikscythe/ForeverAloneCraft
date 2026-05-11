#pragma once

#include <cstdint>

namespace living_world
{
namespace ai
{

struct TravelWatchdogState
{
    bool          initialized = false;
    float         anchorX = 0.0f;
    float         anchorY = 0.0f;
    float         anchorZ = 0.0f;
    std::uint32_t elapsedMs = 0;
    std::uint32_t stagnantMs = 0;
};

struct TravelWatchdogConfig
{
    std::uint32_t stagnantLimitMs = 10000;
    std::uint32_t timeoutMs = 120000;
    float         progressThreshold = 1.5f;
};

enum class TravelWatchdogSignal : std::uint8_t
{
    None = 0,
    Stuck = 1,
    Timeout = 2,
};

inline constexpr TravelWatchdogConfig kDefaultTravelWatchdogConfig {};

inline void ResetTravelWatchdog(TravelWatchdogState& state)
{
    state = TravelWatchdogState{};
}

inline TravelWatchdogSignal UpdateTravelWatchdog(
    TravelWatchdogState& state,
    float currentX,
    float currentY,
    float currentZ,
    std::uint32_t diffMs,
    TravelWatchdogConfig const& config = kDefaultTravelWatchdogConfig)
{
    state.elapsedMs += diffMs;

    if (!state.initialized)
    {
        state.initialized = true;
        state.anchorX = currentX;
        state.anchorY = currentY;
        state.anchorZ = currentZ;
        return state.elapsedMs >= config.timeoutMs
            ? TravelWatchdogSignal::Timeout
            : TravelWatchdogSignal::None;
    }

    float const dx = currentX - state.anchorX;
    float const dy = currentY - state.anchorY;
    float const dz = currentZ - state.anchorZ;
    float const distSq = (dx * dx) + (dy * dy) + (dz * dz);
    float const thresholdSq = config.progressThreshold * config.progressThreshold;

    if (distSq >= thresholdSq)
    {
        state.anchorX = currentX;
        state.anchorY = currentY;
        state.anchorZ = currentZ;
        state.stagnantMs = 0;
    }
    else
    {
        state.stagnantMs += diffMs;
    }

    if (state.elapsedMs >= config.timeoutMs)
        return TravelWatchdogSignal::Timeout;

    if (state.stagnantMs >= config.stagnantLimitMs)
        return TravelWatchdogSignal::Stuck;

    return TravelWatchdogSignal::None;
}

} // namespace ai
} // namespace living_world