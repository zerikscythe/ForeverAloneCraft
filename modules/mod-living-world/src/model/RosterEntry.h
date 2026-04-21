#pragma once

#include "model/ControllableBotProfile.h"
#include "model/EncounterTypes.h"

#include <cstdint>

namespace living_world
{
namespace model
{
struct RosterEntry
{
    std::uint64_t rosterEntryId = 0;
    RosterEntrySource source = RosterEntrySource::GenericBot;
    std::uint32_t ownerAccountId = 0;
    std::uint64_t characterGuid = 0;
    ControllableBotProfile controllableProfile;
    bool isEnabled = true;
    bool isAlreadySummoned = false;
};
} // namespace model
} // namespace living_world

