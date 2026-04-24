#pragma once

#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace integration
{
class CharacterNameLeaseRepository
{
public:
    virtual ~CharacterNameLeaseRepository() = default;

    virtual bool RestoreSourceNameLease(
        model::AccountAltRuntimeRecord const& runtime) = 0;
};
} // namespace integration
} // namespace living_world
