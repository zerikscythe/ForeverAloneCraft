#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace living_world
{
namespace service
{
inline float BuildWorldBotExpertiseDodgeOrParryReductionPct(float expertiseBonus)
{
    return std::max(expertiseBonus, 0.0f) / 4.0f;
}

inline std::int32_t AdjustWorldBotDodgeOrParryChanceForExpertise(
    std::int32_t existingChanceBasisPoints,
    float expertiseBonus)
{
    std::int32_t const adjustedChance = existingChanceBasisPoints
        - static_cast<std::int32_t>(std::lround(
            BuildWorldBotExpertiseDodgeOrParryReductionPct(expertiseBonus) * 100.0f));
    return std::max(adjustedChance, 0);
}
} // namespace service
} // namespace living_world
