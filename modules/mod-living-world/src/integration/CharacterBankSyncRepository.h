#pragma once

#include "model/CharacterItemSnapshot.h"

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterBankSyncRepository
{
public:
    virtual ~CharacterBankSyncRepository() = default;

    virtual bool SyncBankToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) = 0;
};
} // namespace integration
} // namespace living_world
