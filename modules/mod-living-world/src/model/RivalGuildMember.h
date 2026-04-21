#pragma once

#include "EncounterTypes.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct RivalGuildMember
{
    std::uint64_t botId = 0;
    BotRole role = BotRole::Unknown;
    BotPersonality personality = BotPersonality::Indifferent;
    std::uint8_t rankWeight = 0;
};
} // namespace model
} // namespace living_world

