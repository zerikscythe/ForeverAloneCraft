#pragma once

#include "EncounterTypes.h"
#include "RivalGuildMember.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace model
{
struct RivalGuildProfile
{
    std::uint32_t guildId = 0;
    std::string guildName;
    BotFaction faction = BotFaction::Neutral;
    float aggressionBias = 0.0f;
    std::vector<RivalGuildMember> members;
};
} // namespace model
} // namespace living_world

