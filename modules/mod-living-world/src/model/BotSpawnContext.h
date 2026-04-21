#pragma once

#include "EncounterTypes.h"

#include <cstdint>
#include <vector>

namespace living_world
{
namespace model
{
struct BotSpawnContext
{
    std::uint32_t playerZoneId = 0;
    std::vector<std::uint32_t> nearbyZoneIds;
    std::uint32_t localSpawnBudget = 0;
    std::uint32_t visibleBotLimit = 0;
    bool allowCitySpawns = true;
    bool allowOutdoorSpawns = true;
    ProgressionPhase phase = ProgressionPhase::Classic;
};
} // namespace model
} // namespace living_world

