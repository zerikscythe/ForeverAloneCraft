#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace living_world
{
namespace service
{
inline std::int32_t AdjustWorldBotMeleeMissChance(
    std::int32_t existingMissChanceBasisPoints,
    float hitChanceBonusPct)
{
    std::int32_t const adjustedChance = existingMissChanceBasisPoints
        - static_cast<std::int32_t>(std::lround(hitChanceBonusPct * 100.0f));
    return std::max(adjustedChance, 0);
}
} // namespace service
} // namespace living_world
