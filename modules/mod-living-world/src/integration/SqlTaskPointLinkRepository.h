#pragma once

#include "model/AmbientBotTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{

class SqlTaskPointLinkRepository
{
public:
    std::vector<model::TaskPointLinkEntry> LoadLocalNavigationLinks(
        std::uint16_t mapId,
        std::uint32_t zoneId,
        std::string const& linkKind = "local_nav") const;
};

} // namespace integration
} // namespace living_world
