#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline float BuildWorldBotHealthRegenPerTick(
    float healthRegenPerFiveSeconds,
    std::uint32_t intervalMs)
{
    return std::max(healthRegenPerFiveSeconds, 0.0f)
        * static_cast<float>(intervalMs)
        / 5000.0f;
}
} // namespace service
} // namespace living_world