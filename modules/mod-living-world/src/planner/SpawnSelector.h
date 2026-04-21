#pragma once

#include "model/BotAbstractState.h"
#include "model/BotProfile.h"
#include "model/BotSpawnContext.h"
#include "model/ProgressionPhaseState.h"
#include "planner/PlannerTypes.h"

#include <vector>

namespace living_world
{
namespace planner
{
class SpawnSelector
{
public:
    virtual ~SpawnSelector() = default;

    virtual std::vector<PlannedSpawn> SelectSpawns(
        std::vector<model::BotProfile> const& profiles,
        std::vector<model::BotAbstractState> const& states,
        model::BotSpawnContext const& context,
        model::ProgressionPhaseState const& phaseState) const = 0;
};
} // namespace planner
} // namespace living_world

