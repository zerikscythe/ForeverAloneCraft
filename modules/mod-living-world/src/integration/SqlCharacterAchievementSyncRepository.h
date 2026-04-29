#pragma once

#include "integration/CharacterAchievementSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterAchievementSyncRepository final
    : public CharacterAchievementSyncRepository
{
public:
    bool SyncAchievementsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) override;
};
} // namespace integration
} // namespace living_world
