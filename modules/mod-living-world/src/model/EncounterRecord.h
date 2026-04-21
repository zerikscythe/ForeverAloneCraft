#pragma once

#include "EncounterTypes.h"

#include <cstdint>
#include <vector>

namespace living_world
{
namespace model
{
struct EncounterRecord
{
    std::uint64_t encounterId = 0;
    std::vector<std::uint64_t> participantBotIds;
    std::uint32_t zoneId = 0;
    EncounterDisposition disposition = EncounterDisposition::PassBy;
    GroupAlertState resolvedGroupState = GroupAlertState::Idle;
    std::uint32_t cooldownSecondsApplied = 0;
};
} // namespace model
} // namespace living_world

