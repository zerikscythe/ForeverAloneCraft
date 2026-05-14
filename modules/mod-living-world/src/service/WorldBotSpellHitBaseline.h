#pragma once

#include <cmath>
#include <cstdint>

namespace living_world
{
namespace service
{
inline std::int32_t AdjustWorldBotMagicSpellHitChance(
    std::int32_t existingHitChanceBasisPoints,
    float spellHitBonusPct)
{
    return existingHitChanceBasisPoints
        + static_cast<std::int32_t>(std::lround(spellHitBonusPct * 100.0f));
}
} // namespace service
} // namespace living_world
