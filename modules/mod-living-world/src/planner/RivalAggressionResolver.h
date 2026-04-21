#pragma once

#include "model/BotAbstractState.h"
#include "model/BotProfile.h"
#include "model/BotSpawnContext.h"
#include "model/EncounterTypes.h"

namespace living_world
{
namespace planner
{
class RivalAggressionResolver
{
public:
    virtual ~RivalAggressionResolver() = default;

    virtual model::EncounterDisposition ResolveDisposition(
        model::BotProfile const& profile,
        model::BotAbstractState const& state,
        model::BotSpawnContext const& context) const = 0;
};
} // namespace planner
} // namespace living_world

