#pragma once

#include "EncounterTypes.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct PartyBotProfile
{
    std::uint64_t botId = 0;
    BotRole preferredRole = BotRole::Unknown;
    std::uint8_t minSupportedPlayerLevel = 1;
    std::uint8_t maxSupportedPlayerLevel = 80;
    bool isSummonable = true;
};
} // namespace model
} // namespace living_world

