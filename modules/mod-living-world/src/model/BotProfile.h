#pragma once

#include "EncounterTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace model
{
struct BotProfile
{
    std::uint64_t botId = 0;
    std::string name;
    std::uint8_t raceId = 0;
    std::uint8_t classId = 0;
    BotFaction faction = BotFaction::Neutral;
    std::uint8_t level = 1;
    std::string guildName;
    BotPersonality personality = BotPersonality::Indifferent;
    BotRole preferredRole = BotRole::Unknown;
    std::vector<std::uint32_t> preferredZoneIds;
};
} // namespace model
} // namespace living_world

