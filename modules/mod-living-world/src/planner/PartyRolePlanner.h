#pragma once

#include "model/PartyBotProfile.h"

#include <vector>

namespace living_world
{
namespace planner
{
class PartyRolePlanner
{
public:
    virtual ~PartyRolePlanner() = default;

    virtual std::vector<std::uint64_t> SelectPartyBots(
        std::vector<model::PartyBotProfile> const& profiles,
        std::vector<model::BotRole> const& requiredRoles,
        std::uint8_t playerLevel) const = 0;
};
} // namespace planner
} // namespace living_world

