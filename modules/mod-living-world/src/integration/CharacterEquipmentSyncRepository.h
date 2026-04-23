#pragma once

#include "model/CharacterItemSnapshot.h"

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterEquipmentSyncRepository
{
public:
    virtual ~CharacterEquipmentSyncRepository() = default;

    virtual bool SyncEquipmentToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) = 0;
};
} // namespace integration
} // namespace living_world
