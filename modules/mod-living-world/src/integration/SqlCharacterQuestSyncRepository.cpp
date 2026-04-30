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

    // Sync active (in-progress) quests. Use ON DUPLICATE KEY UPDATE so an
    // existing source row can still absorb clone progress instead of silently
    // being skipped. Progress counters only ever move upward.
    CharacterDatabase.DirectExecute(
        "INSERT INTO character_queststatus "
        "(guid, quest, status, explored, timer, "
        " mobcount1, mobcount2, mobcount3, mobcount4, "
        " itemcount1, itemcount2, itemcount3, itemcount4, itemcount5, itemcount6, "
        " playercount) "
        "SELECT {}, quest, status, explored, timer, "
        " mobcount1, mobcount2, mobcount3, mobcount4, "
        " itemcount1, itemcount2, itemcount3, itemcount4, itemcount5, itemcount6, "
        " playercount "
        "FROM character_queststatus WHERE guid = {} AND status != 0 "
        "ON DUPLICATE KEY UPDATE "
        " timer = CASE WHEN status = 0 THEN VALUES(timer) ELSE timer END, "
        " explored = GREATEST(explored, VALUES(explored)), "
        " mobcount1 = GREATEST(mobcount1, VALUES(mobcount1)), "
        " mobcount2 = GREATEST(mobcount2, VALUES(mobcount2)), "
        " mobcount3 = GREATEST(mobcount3, VALUES(mobcount3)), "
        " mobcount4 = GREATEST(mobcount4, VALUES(mobcount4)), "
        " itemcount1 = GREATEST(itemcount1, VALUES(itemcount1)), "
        " itemcount2 = GREATEST(itemcount2, VALUES(itemcount2)), "
        " itemcount3 = GREATEST(itemcount3, VALUES(itemcount3)), "
        " itemcount4 = GREATEST(itemcount4, VALUES(itemcount4)), "
        " itemcount5 = GREATEST(itemcount5, VALUES(itemcount5)), "
        " itemcount6 = GREATEST(itemcount6, VALUES(itemcount6)), "
        " playercount = GREATEST(playercount, VALUES(playercount)), "
        " status = GREATEST(status, VALUES(status))",
        sourceCharacterGuid,
        cloneCharacterGuid);

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
