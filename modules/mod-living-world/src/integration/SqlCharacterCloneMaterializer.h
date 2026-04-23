#pragma once

#include "integration/CharacterCloneMaterializer.h"

namespace living_world
{
namespace integration
{
class SqlCharacterCloneMaterializer final : public CharacterCloneMaterializer
{
public:
    CharacterCloneMaterializationResult MaterializeClone(
        model::AccountAltRuntimeRecord const& runtime) override;
};
} // namespace integration
} // namespace living_world
