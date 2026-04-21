#pragma once

#include "model/BotProfile.h"
#include "model/BotSpawnContext.h"
#include "model/ProgressionPhaseState.h"

namespace living_world
{
namespace planner
{
class ProgressionGateResolver
{
public:
    virtual ~ProgressionGateResolver() = default;

    virtual bool IsBotAllowedInContext(
        model::BotProfile const& profile,
        model::BotSpawnContext const& context,
        model::ProgressionPhaseState const& phaseState) const = 0;
};
} // namespace planner
} // namespace living_world

