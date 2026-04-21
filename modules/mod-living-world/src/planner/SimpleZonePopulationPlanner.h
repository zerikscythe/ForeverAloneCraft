#pragma once

#include "planner/ProgressionGateResolver.h"
#include "planner/SpawnSelector.h"
#include "planner/ZonePopulationPlanner.h"

namespace living_world
{
namespace planner
{
class SimpleZonePopulationPlanner : public ZonePopulationPlanner
{
public:
    SimpleZonePopulationPlanner(SpawnSelector const& spawnSelector, ProgressionGateResolver const& gateResolver);

    ZonePopulationPlan BuildPlan(
        std::vector<model::BotProfile> const& profiles,
        std::vector<model::BotAbstractState> const& states,
        model::BotSpawnContext const& context,
        model::ProgressionPhaseState const& phaseState) const override;

private:
    SpawnSelector const& _spawnSelector;
    ProgressionGateResolver const& _gateResolver;
};
} // namespace planner
} // namespace living_world

