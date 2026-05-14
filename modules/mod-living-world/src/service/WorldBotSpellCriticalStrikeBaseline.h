#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
struct WorldBotSpellCriticalStrikeBaseline
{
    float critChance = 0.0f;
};

inline WorldBotSpellCriticalStrikeBaseline BuildWorldBotSpellCriticalStrikeBaseline(
    std::int32_t intellect,
    float critBase,
    float critRatio)
{
    WorldBotSpellCriticalStrikeBaseline baseline;
    if (critRatio <= 0.0f)
        return baseline;

    baseline.critChance = (critBase + static_cast<float>(std::max<std::int32_t>(intellect, 0)) * critRatio) * 100.0f;
    baseline.critChance = std::max(baseline.critChance, 0.0f);
    return baseline;
}

inline float AdjustWorldBotSpellCriticalStrikeChance(
    float existingCreatureCritChancePct,
    float playerLikeCritChancePct,
    float creatureBaseCritChancePct = 5.0f)
{
    float const adjustedChance = existingCreatureCritChancePct + (playerLikeCritChancePct - creatureBaseCritChancePct);
    return std::max(adjustedChance, 0.0f);
}

inline float AddWorldBotSpellCriticalStrikeRatingBonus(
    float existingCritChancePct,
    float ratingBonusPct)
{
    return std::max(existingCritChancePct + ratingBonusPct, 0.0f);
}
} // namespace service
} // namespace living_world
