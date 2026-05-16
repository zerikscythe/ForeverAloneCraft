#pragma once

#include <cstdint>
#include <vector>

namespace living_world
{
namespace integration
{

class SqlBotExploredZoneRepository
{
public:
    void EnsureSchema() const;

    void MarkExplored(
        std::uint32_t identityId,
        std::uint32_t zoneId) const;

    void ClearExploredZones(
        std::uint32_t identityId) const;

    void ReplaceExploredZones(
        std::uint32_t identityId,
        std::vector<std::uint32_t> const& zoneIds) const;

    std::vector<std::uint32_t> LoadExploredZones(
        std::uint32_t identityId) const;
};

} // namespace integration
} // namespace living_world
