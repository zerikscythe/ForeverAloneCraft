#pragma once

#include "model/BotProfile.h"
#include "model/BotSpawnContext.h"
#include "model/EncounterRecord.h"
#include "model/RivalGuildProfile.h"
#include "planner/PlannerTypes.h"

#include <vector>

namespace living_world
{
namespace planner
{
class EncounterPlanner
{
public:
    virtual ~EncounterPlanner() = default;

    virtual PlannedEncounter PlanEncounter(
        model::RivalGuildProfile const& guild,
        std::vector<model::BotProfile> const& profiles,
        std::vector<model::EncounterRecord> const& encounterHistory,
        model::BotSpawnContext const& context) const = 0;
};
} // namespace planner
} // namespace living_world

