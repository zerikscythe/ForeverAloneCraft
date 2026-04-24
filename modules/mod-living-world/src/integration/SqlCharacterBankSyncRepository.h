#pragma once

#include "integration/CharacterBankSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterBankSyncRepository final
    : public CharacterBankSyncRepository
{
public:
    bool SyncBankToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) override;
};
} // namespace integration
} // namespace living_world
