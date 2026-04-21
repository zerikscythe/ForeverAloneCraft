#pragma once

#include "EncounterTypes.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct BotAbstractState
{
    std::uint64_t botId = 0;
    std::uint32_t currentRegionId = 0;
    BotActivity activity = BotActivity::Abstract;
    bool isSpawned = false;
    bool isAlive = true;
    std::uint32_t respawnCooldownSecondsRemaining = 0;
    std::uint32_t relocationTimerSecondsRemaining = 0;
    std::uint32_t encounterCooldownSecondsRemaining = 0;
    float cityPreference = 0.0f;
    float outdoorPreference = 0.0f;
};
} // namespace model
} // namespace living_world

