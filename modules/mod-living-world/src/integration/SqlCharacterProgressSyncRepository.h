#pragma once

#include "integration/CharacterProgressSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterProgressSyncRepository final
    : public CharacterProgressSyncRepository
{
public:
    bool SyncProgressToCharacter(
        std::uint64_t characterGuid,
        model::CharacterProgressSnapshot const& snapshot) override;
};
} // namespace integration
} // namespace living_world
