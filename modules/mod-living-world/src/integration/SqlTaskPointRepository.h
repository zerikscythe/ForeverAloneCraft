#pragma once

#include "model/AmbientBotTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{

class SqlTaskPointRepository
{
public:
    std::optional<model::TaskPointEntry> FindByKey(std::string const& pointKey) const;
    std::optional<model::TaskPointEntry> FindByZoneAndType(std::uint32_t zoneId, std::string const& pointType) const;
    std::optional<model::ZoneAnchorEntry> FindZoneAnchor(
        std::uint32_t zoneId,
        std::string const& anchorRole,
        std::uint8_t faction,
        std::uint8_t level) const;
    std::vector<model::ZoneContentEntry> LoadZoneContentByKind(
        std::string const& contentKind,
        std::uint8_t faction,
        std::uint8_t level) const;
    std::vector<model::ZoneContentEntry> LoadZoneContentByZoneAndKind(
        std::uint32_t zoneId,
        std::string const& contentKind,
        std::uint8_t faction,
        std::uint8_t level) const;
    std::vector<model::TaskTransitRouteEntry> FindTransitPathForZones(
        std::uint32_t sourceZoneId,
        std::uint32_t destZoneId,
        std::uint8_t faction,
        std::uint8_t level) const;
};

} // namespace integration
} // namespace living_world