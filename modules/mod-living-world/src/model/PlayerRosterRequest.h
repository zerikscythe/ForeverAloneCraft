#pragma once

#include "model/EncounterTypes.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct PlayerRosterRequest
{
    std::uint64_t requesterCharacterGuid = 0;
    std::uint32_t requesterAccountId = 0;
    std::uint64_t requestedRosterEntryId = 0;
    PossessionMode possessionMode = PossessionMode::Assisted;
    bool spawnAtPlayerLocation = true;
    bool addToPlayerParty = true;
};
} // namespace model
} // namespace living_world

