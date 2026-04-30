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

    // Criteria progress: insert missing rows first, then merge upward. This
    // avoids self-table INSERT ... ON DUPLICATE KEY ambiguity on MySQL.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_achievement_progress (guid, criteria, counter, date) "
        "SELECT {}, criteria, counter, date "
        "FROM character_achievement_progress WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    CharacterDatabase.DirectExecute(
        "UPDATE character_achievement_progress AS source "
        "INNER JOIN character_achievement_progress AS clone "
        "  ON clone.criteria = source.criteria "
        " AND clone.guid = {} "
        "SET source.counter = IF(clone.counter > source.counter, clone.counter, source.counter), "
        "    source.date = IF(clone.counter > source.counter, clone.date, source.date) "
        "WHERE source.guid = {}",
        cloneCharacterGuid,
        sourceCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
