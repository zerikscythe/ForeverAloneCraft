#pragma once

#include "model/AmbientBotTypes.h"
#include <cstdint>
#include <vector>

namespace living_world
{
namespace integration
{

class SqlActivityLibraryRepository
{
public:
    // Load all activities eligible for a bot with the given parameters.
    // faction: 0=both 1=alliance 2=horde (matches required_faction=0 or exact)
    std::vector<model::ActivityEntry> LoadEligible(
        std::uint8_t faction,
        std::uint8_t level,
        bool hasHerbalism,
        bool hasMining,
        bool hasFishing) const;
};

} // namespace integration
} // namespace living_world
