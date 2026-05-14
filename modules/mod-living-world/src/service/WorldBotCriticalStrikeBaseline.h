#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
struct WorldBotCriticalStrikeBaseline
{
    float critChance = 0.0f;
};

inline WorldBotCriticalStrikeBaseline BuildWorldBotCriticalStrikeBaseline(
    std::int32_t agility,
    float critBase,
    float critRatio)
{
    WorldBotCriticalStrikeBaseline baseline;
    if (critRatio <= 0.0f)
        return baseline;

    baseline.critChance = (critBase + static_cast<float>(std::max<std::int32_t>(agility, 0)) * critRatio) * 100.0f;
    baseline.critChance = std::max(baseline.critChance, 0.0f);
    return baseline;
}

inline std::int32_t AdjustWorldBotCriticalStrikeChance(
    std::int32_t existingChanceBasisPoints,
    float playerLikeCritChancePct,
    float creatureBaseCritChancePct = 5.0f)
{
    std::int32_t const adjustedChance = existingChanceBasisPoints
        + static_cast<std::int32_t>((playerLikeCritChancePct - creatureBaseCritChancePct) * 100.0f);
    return std::max(adjustedChance, 0);
}

inline std::int32_t AddWorldBotCriticalStrikeRatingBonus(
    std::int32_t existingChanceBasisPoints,
    float ratingBonusPct)
{
    return std::max(
        existingChanceBasisPoints + static_cast<std::int32_t>(ratingBonusPct * 100.0f),
        0);
}
} // namespace service
} // namespace living_world
