#pragma once

#include "model/CharacterItemSnapshot.h"

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterInventorySyncRepository
{
public:
    virtual ~CharacterInventorySyncRepository() = default;

    virtual bool SyncInventoryToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) = 0;
};
} // namespace integration
} // namespace living_world
