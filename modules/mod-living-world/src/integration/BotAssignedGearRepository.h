#pragma once

#include "model/WorldBotAssignedGear.h"

#include <cstdint>
#include <vector>

namespace living_world
{
namespace integration
{
class BotAssignedGearRepository
{
public:
    virtual ~BotAssignedGearRepository() = default;

    virtual std::vector<model::WorldBotAssignedGearEntry> LoadAssignments(
        std::uint32_t identityId) const = 0;

    virtual void ReplaceAssignments(
        std::uint32_t identityId,
        std::uint8_t refreshBand,
        std::vector<model::WorldBotAssignedGearEntry> const& entries) const = 0;
};
} // namespace integration
} // namespace living_world