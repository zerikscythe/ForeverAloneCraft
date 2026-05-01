#include "integration/SqlCharacterQuestSyncRepository.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
namespace
{
std::uint32_t CountActiveQuestRows(std::uint64_t characterGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM character_queststatus WHERE guid = {} AND status != 0",
        characterGuid);
    if (!result)
    {
        return 0;
    }

    return result->Fetch()[0].Get<std::uint32_t>();
}

std::uint32_t CountRewardedQuestRows(std::uint64_t characterGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM character_queststatus_rewarded WHERE guid = {}",
        characterGuid);
    if (!result)
    {
        return 0;
    }

    return result->Fetch()[0].Get<std::uint32_t>();
}
} // namespace

bool SqlCharacterQuestSyncRepository::SyncQuestsFromCloneToSource(
    std::uint64_t sourceCharacterGuid,
    std::uint64_t cloneCharacterGuid)
{
    std::uint32_t const cloneActiveBefore = CountActiveQuestRows(cloneCharacterGuid);
    std::uint32_t const sourceActiveBefore = CountActiveQuestRows(sourceCharacterGuid);
    std::uint32_t const sourceRewardedBefore = CountRewardedQuestRows(sourceCharacterGuid);

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] QuestSync repo start sourceGuid={} cloneGuid={} "
        "cloneActiveBefore={} sourceActiveBefore={} sourceRewardedBefore={}",
        sourceCharacterGuid,
        cloneCharacterGuid,
        cloneActiveBefore,
        sourceActiveBefore,
        sourceRewardedBefore);

    // Sync rewarded (completed) quests — additive, INSERT IGNORE is safe.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_queststatus_rewarded (guid, quest, active) "
        "SELECT {}, quest, active "
        "FROM character_queststatus_rewarded WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    // Rewarded quests must not leave an old active quest row behind on the
    // parked/original source. AzerothCore loads active quest rows before it
    // checks rewarded state, so stale source rows can resurrect a quest that
    // the clone already turned in.
    CharacterDatabase.DirectExecute(
        "DELETE source "
        "FROM character_queststatus AS source "
        "INNER JOIN character_queststatus_rewarded AS clone_rewarded "
        "  ON clone_rewarded.quest = source.quest "
        " AND clone_rewarded.guid = {} "
        "WHERE source.guid = {}",
        cloneCharacterGuid,
        sourceCharacterGuid);

    // AzerothCore can leave behind zero-status rows for quests that are no
    // longer active. Those stale rows block INSERT IGNORE on the real clone
    // progress because (guid, quest) is the primary key. Clear only the stale
    // source rows for quests the clone currently has active before merging.
    CharacterDatabase.DirectExecute(
        "DELETE source "
        "FROM character_queststatus AS source "
        "INNER JOIN character_queststatus AS clone "
        "  ON clone.quest = source.quest "
        " AND clone.guid = {} "
        " AND clone.status != 0 "
        "WHERE source.guid = {} "
        "  AND source.status = 0",
        cloneCharacterGuid,
        sourceCharacterGuid);

    // Sync active (in-progress) quests in two steps. MySQL gets unhappy when
    // we self-select from and self-update character_queststatus inside one
    // INSERT ... ON DUPLICATE KEY UPDATE statement, so we first insert any
    // missing source rows, then mirror the clone row onto existing ones. The
    // clone is the authoritative live state for the runtime session, so we
    // copy its timer, counters, and quest status exactly instead of taking a
    // numeric max. QuestStatus values are not ordered by progress in AC.
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

    CharacterDatabase.DirectExecute(
        "UPDATE character_queststatus AS source "
        "INNER JOIN character_queststatus AS clone "
        "  ON clone.quest = source.quest "
        " AND clone.guid = {} "
        " AND clone.status != 0 "
        "SET source.status = clone.status, "
        "    source.explored = clone.explored, "
        "    source.timer = clone.timer, "
        "    source.mobcount1 = clone.mobcount1, "
        "    source.mobcount2 = clone.mobcount2, "
        "    source.mobcount3 = clone.mobcount3, "
        "    source.mobcount4 = clone.mobcount4, "
        "    source.itemcount1 = clone.itemcount1, "
        "    source.itemcount2 = clone.itemcount2, "
        "    source.itemcount3 = clone.itemcount3, "
        "    source.itemcount4 = clone.itemcount4, "
        "    source.itemcount5 = clone.itemcount5, "
        "    source.itemcount6 = clone.itemcount6, "
        "    source.playercount = clone.playercount "
        "WHERE source.guid = {}",
        cloneCharacterGuid,
        sourceCharacterGuid);

    std::uint32_t const sourceActiveAfter = CountActiveQuestRows(sourceCharacterGuid);
    std::uint32_t const sourceRewardedAfter = CountRewardedQuestRows(sourceCharacterGuid);

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] QuestSync repo end sourceGuid={} cloneGuid={} "
        "sourceActiveAfter={} sourceRewardedAfter={}",
        sourceCharacterGuid,
        cloneCharacterGuid,
        sourceActiveAfter,
        sourceRewardedAfter);

    return true;
}
} // namespace integration
} // namespace living_world
