#pragma once

#include "model/PlayerRosterContext.h"
#include "model/PlayerRosterRequest.h"
#include "model/RosterEntry.h"
#include "planner/PlannerTypes.h"

#include <vector>

namespace living_world
{
namespace planner
{
class PartyRosterPlanner
{
public:
    virtual ~PartyRosterPlanner() = default;

    virtual PartyRosterPlan BuildPlan(
        std::vector<model::RosterEntry> const& rosterEntries,
        model::PlayerRosterRequest const& request,
        model::PlayerRosterContext const& context) const = 0;
};
} // namespace planner
} // namespace living_world

