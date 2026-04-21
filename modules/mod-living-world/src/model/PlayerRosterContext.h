#pragma once

#include "model/EncounterTypes.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct PlayerRosterContext
{
    std::uint32_t partyMemberCount = 0;
    std::uint32_t maxPartyMembers = 5;
    std::uint32_t playerZoneId = 0;
    bool playerIsInWorld = true;
    bool playerCanControlCompanions = true;
};
} // namespace model
} // namespace living_world

