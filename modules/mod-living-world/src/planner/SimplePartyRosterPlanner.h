#pragma once

#include "planner/PartyRosterPlanner.h"

namespace living_world
{
namespace planner
{
class SimplePartyRosterPlanner : public PartyRosterPlanner
{
public:
    PartyRosterPlan BuildPlan(
        std::vector<model::RosterEntry> const& rosterEntries,
        model::PlayerRosterRequest const& request,
        model::PlayerRosterContext const& context) const override;
};
} // namespace planner
} // namespace living_world

