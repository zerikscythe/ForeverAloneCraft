#include "integration/SqlCharacterAchievementSyncRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
bool SqlCharacterAchievementSyncRepository::SyncAchievementsFromCloneToSource(
    std::uint64_t sourceCharacterGuid,
    std::uint64_t cloneCharacterGuid)
{
    // Completed achievements: INSERT IGNORE keeps the earliest completion date
    // on the source if already present.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_achievement (guid, achievement, date) "
        "SELECT {}, achievement, date "
        "FROM character_achievement WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    // Criteria progress: merge by taking the higher counter value, and update
    // the date only when the counter is actually advanced.
    CharacterDatabase.DirectExecute(
        "INSERT INTO character_achievement_progress (guid, criteria, counter, date) "
        "SELECT {}, criteria, counter, date "
        "FROM character_achievement_progress WHERE guid = {} "
        "ON DUPLICATE KEY UPDATE "
        "counter = IF(VALUES(counter) > counter, VALUES(counter), counter), "
        "date = IF(VALUES(counter) > counter, VALUES(date), date)",
        sourceCharacterGuid,
        cloneCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
