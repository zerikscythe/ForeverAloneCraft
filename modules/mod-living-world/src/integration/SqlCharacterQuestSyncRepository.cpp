#include "integration/SqlCharacterQuestSyncRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
bool SqlCharacterQuestSyncRepository::SyncQuestsFromCloneToSource(
    std::uint64_t sourceCharacterGuid,
    std::uint64_t cloneCharacterGuid)
{
    // INSERT IGNORE: completed quests only accumulate; if the source already
    // has a quest marked rewarded, leave it alone.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_queststatus_rewarded (guid, quest, active) "
        "SELECT {}, quest, active "
        "FROM character_queststatus_rewarded WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);
    return true;
}
} // namespace integration
} // namespace living_world
