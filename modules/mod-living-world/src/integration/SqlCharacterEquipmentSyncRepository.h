#pragma once

#include "integration/CharacterEquipmentSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterEquipmentSyncRepository final
    : public CharacterEquipmentSyncRepository
{
public:
    bool SyncEquipmentToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) override;
};
} // namespace integration
} // namespace living_world
