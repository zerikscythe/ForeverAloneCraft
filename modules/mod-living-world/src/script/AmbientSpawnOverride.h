#pragma once

#include <cstdint>

namespace living_world
{
namespace script
{

struct AmbientSpawnOverrideConfig
{
    std::uint32_t forcedSpawnCount = 0;
    std::uint32_t forcedMapId = 0;
};

inline bool HasForcedSpawnOverride(AmbientSpawnOverrideConfig const& config)
{
    return config.forcedSpawnCount > 0;
}

inline bool ShouldUseForcedSpawn(
    AmbientSpawnOverrideConfig const& config,
    std::uint32_t onlineCount,
    std::uint32_t forcedSpawnedThisTick)
{
    return HasForcedSpawnOverride(config)
        && (onlineCount + forcedSpawnedThisTick) < config.forcedSpawnCount;
}

} // namespace script
} // namespace living_world
