#pragma once

#include "integration/CharacterInventorySyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterInventorySyncRepository final
    : public CharacterInventorySyncRepository
{
public:
    bool SyncInventoryToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) override;
};
} // namespace integration
} // namespace living_world
