#pragma once

#include "model/WorldBotVirtualLoadout.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
class BotVirtualLoadoutRepository
{
public:
    virtual ~BotVirtualLoadoutRepository() = default;

    virtual std::vector<model::WorldBotVirtualLoadout> ListLoadouts() const = 0;

    virtual std::optional<model::WorldBotVirtualLoadout> FindLoadout(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey,
        std::uint8_t gearTier) const = 0;
};
} // namespace integration
} // namespace living_world