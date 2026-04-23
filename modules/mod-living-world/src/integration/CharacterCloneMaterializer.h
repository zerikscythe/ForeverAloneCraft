#pragma once

#include "model/AccountAltRuntime.h"

#include <string>

namespace living_world
{
namespace integration
{
struct CharacterCloneMaterializationResult
{
    bool succeeded = false;
    std::uint64_t cloneCharacterGuid = 0;
    std::string cloneCharacterName;
    model::CharacterProgressSnapshot cloneSnapshot;
    std::string reason;
};

class CharacterCloneMaterializer
{
public:
    virtual ~CharacterCloneMaterializer() = default;

    virtual CharacterCloneMaterializationResult MaterializeClone(
        model::AccountAltRuntimeRecord const& runtime) = 0;
};
} // namespace integration
} // namespace living_world
