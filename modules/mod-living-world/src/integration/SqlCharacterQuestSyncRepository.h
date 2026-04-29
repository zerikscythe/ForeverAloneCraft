#pragma once

#include "integration/CharacterQuestSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterQuestSyncRepository final
    : public CharacterQuestSyncRepository
{
public:
    bool SyncQuestsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) override;
};
} // namespace integration
} // namespace living_world
