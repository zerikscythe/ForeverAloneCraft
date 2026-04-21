#include "planner/SimpleZonePopulationPlanner.h"

#include <algorithm>

namespace living_world
{
namespace planner
{
SimpleZonePopulationPlanner::SimpleZonePopulationPlanner(SpawnSelector const& spawnSelector, ProgressionGateResolver const& gateResolver)
    : _spawnSelector(spawnSelector), _gateResolver(gateResolver)
{
}

ZonePopulationPlan SimpleZonePopulationPlanner::BuildPlan(
    std::vector<model::BotProfile> const& profiles,
    std::vector<model::BotAbstractState> const& states,
    model::BotSpawnContext const& context,
    model::ProgressionPhaseState const& phaseState) const
{
    ZonePopulationPlan plan;

    std::uint32_t const budget = std::min(context.localSpawnBudget, context.visibleBotLimit);
    if (budget == 0)
    {
        return plan;
    }

    std::vector<model::BotProfile> eligibleProfiles;
    eligibleProfiles.reserve(profiles.size());

    for (model::BotProfile const& profile : profiles)
    {
        if (_gateResolver.IsBotAllowedInContext(profile, context, phaseState))
        {
            eligibleProfiles.push_back(profile);
        }
    }

    std::vector<PlannedSpawn> selected = _spawnSelector.SelectSpawns(eligibleProfiles, states, context, phaseState);
    if (selected.size() > budget)
    {
        selected.resize(budget);
    }

    plan.reservedBudget = static_cast<std::uint32_t>(selected.size());
    plan.spawns = std::move(selected);
    return plan;
}
} // namespace planner
} // namespace living_world

