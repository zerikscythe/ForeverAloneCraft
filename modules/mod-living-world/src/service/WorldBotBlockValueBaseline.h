#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline std::uint32_t BuildWorldBotShieldBlockValue(
    bool hasShield,
    float strength,
    float flatBlockValue,
    float blockValueMultiplier)
{
    if (!hasShield || blockValueMultiplier <= 0.0f)
        return 0;

    float const value = (
        std::max(strength, 0.0f) * 0.5f
            + flatBlockValue
            - 10.0f) * blockValueMultiplier;

    return value > 0.0f
        ? static_cast<std::uint32_t>(value)
        : 0;
}
} // namespace service
} // namespace living_world
