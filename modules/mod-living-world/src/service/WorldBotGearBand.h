#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline std::uint8_t ComputeWorldBotGearRefreshBand(std::uint8_t level)
{
    if (level == 0)
        return 0;

    return static_cast<std::uint8_t>(1u + ((std::min<std::uint8_t>(level, 80u) - 1u) / 5u));
}
} // namespace service
} // namespace living_world