#pragma once

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterAchievementSyncRepository
{
public:
    virtual ~CharacterAchievementSyncRepository() = default;

    // Copies all completed achievements and criteria progress from the clone to
    // the source. Completed achievements use INSERT IGNORE (achievements only
    // accumulate). Criteria counters are merged by taking the higher value so
    // partial progress earned during the session is preserved.
    // Returns true if all writes completed without error.
    virtual bool SyncAchievementsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) = 0;
};
} // namespace integration
} // namespace living_world
