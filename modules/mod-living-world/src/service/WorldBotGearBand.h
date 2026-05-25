#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline std::uint8_t ComputeWorldBotEndgameProgressionStage(
    std::uint8_t level,
    std::uint64_t postMaxWorldOnlineMs)
{
    if (level < 80)
        return 0;

    constexpr std::uint64_t HourMs = 60ull * 60ull * 1000ull;

    // Level-80 bots retire after about 50 hours of post-cap world time. Fresh
    // 80s should already materialize with a pre-raid kit, then climb toward
    // perfection, with only the last few hours reserved for near-BiS status:
    //   0 = pre-raid fresh 80
    //   1 = early raid-geared
    //   2 = established raid-geared
    //   3 = late-raid / high-end
    //   4 = near-BiS / maxed before retirement
    if (postMaxWorldOnlineMs >= 46ull * HourMs)
        return 4;
    if (postMaxWorldOnlineMs >= 36ull * HourMs)
        return 3;
    if (postMaxWorldOnlineMs >= 24ull * HourMs)
        return 2;
    if (postMaxWorldOnlineMs >= 12ull * HourMs)
        return 1;

    return 0;
}

inline std::uint8_t ComputeWorldBotGearRefreshBand(
    std::uint8_t level,
    std::uint64_t postMaxWorldOnlineMs = 0)
{
    if (level == 0)
        return 0;

    std::uint8_t const baseBand =
        static_cast<std::uint8_t>(1u + ((std::min<std::uint8_t>(level, 80u) - 1u) / 5u));

    if (level < 80)
        return baseBand;

    return static_cast<std::uint8_t>(baseBand + ComputeWorldBotEndgameProgressionStage(level, postMaxWorldOnlineMs));
}
} // namespace service
} // namespace living_world
