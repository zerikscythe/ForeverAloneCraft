#pragma once

#include "integration/CharacterNameLeaseRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterNameLeaseRepository final
    : public CharacterNameLeaseRepository
{
public:
    bool RestoreSourceNameLease(
        model::AccountAltRuntimeRecord const& runtime) override;
};
} // namespace integration
} // namespace living_world
