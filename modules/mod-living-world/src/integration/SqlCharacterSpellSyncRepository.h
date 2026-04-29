#pragma once

#include "integration/CharacterSpellSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterSpellSyncRepository final
    : public CharacterSpellSyncRepository
{
public:
    bool SyncSpellsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) override;
};
} // namespace integration
} // namespace living_world
