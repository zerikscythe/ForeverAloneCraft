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
    // Sync rewarded (completed) quests — additive, INSERT IGNORE is safe.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_queststatus_rewarded (guid, quest, active) "
        "SELECT {}, quest, active "
        "FROM character_queststatus_rewarded WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    // Sync active (in-progress) quests. INSERT IGNORE so we never overwrite a
    // quest the source already has in a further-along state. Only copy rows
    // with a real status (status != 0 excludes stale zero-rows left by
    // AzerothCore after quest removal).
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_queststatus "
        "(guid, quest, status, explored, timer, "
        " mobcount1, mobcount2, mobcount3, mobcount4, "
        " itemcount1, itemcount2, itemcount3, itemcount4, itemcount5, itemcount6, "
        " playercount) "
        "SELECT {}, quest, status, explored, timer, "
        " mobcount1, mobcount2, mobcount3, mobcount4, "
        " itemcount1, itemcount2, itemcount3, itemcount4, itemcount5, itemcount6, "
        " playercount "
        "FROM character_queststatus WHERE guid = {} AND status != 0",
        sourceCharacterGuid,
        cloneCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
