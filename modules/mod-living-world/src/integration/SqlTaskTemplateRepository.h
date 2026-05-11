#pragma once

#include "model/AmbientBotTypes.h"

#include <cstdint>
#include <vector>

namespace living_world
{
namespace integration
{

class SqlTaskTemplateRepository
{
public:
    std::vector<model::TaskTemplateEntry> LoadEligible(
        std::uint8_t faction,
        std::uint8_t level,
        bool hasHerbalism,
        bool hasMining,
        bool hasFishing) const;

    std::vector<model::PlaylistEntrySet> LoadEligiblePlaylists(
        std::uint8_t faction,
        std::uint8_t level,
        bool hasHerbalism,
        bool hasMining,
        bool hasFishing) const;
};

} // namespace integration
} // namespace living_world