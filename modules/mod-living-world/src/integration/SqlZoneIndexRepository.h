#pragma once

#include "model/AmbientBotTypes.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace living_world
{
namespace integration
{

class SqlZoneIndexRepository
{
public:
    // Load all zone rows.
    std::vector<model::ZoneEntry> LoadAll() const;

    // Find one zone by ID. Returns nullopt if not found.
    std::optional<model::ZoneEntry> Find(std::uint32_t zoneId) const;
};

} // namespace integration
} // namespace living_world
