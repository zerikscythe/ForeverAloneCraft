#pragma once

#include "model/EncounterTypes.h"

namespace living_world
{
namespace planner
{
class GroupStateResolver
{
public:
    virtual ~GroupStateResolver() = default;

    virtual model::GroupAlertState ResolveNextState(
        model::GroupAlertState currentState,
        bool memberInitiatedAttack,
        bool playerFoughtBack) const = 0;
};
} // namespace planner
} // namespace living_world

