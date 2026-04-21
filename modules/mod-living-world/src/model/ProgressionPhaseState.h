#pragma once

#include "EncounterTypes.h"

#include <cstdint>
#include <unordered_set>

namespace living_world
{
namespace model
{
struct ProgressionPhaseState
{
    ProgressionPhase currentPhase = ProgressionPhase::Classic;
    std::uint8_t maxPlayerLevel = 60;
    bool outlandUnlocked = false;
    bool northrendUnlocked = false;
    std::unordered_set<std::uint32_t> unlockedZoneIds;
};
} // namespace model
} // namespace living_world

