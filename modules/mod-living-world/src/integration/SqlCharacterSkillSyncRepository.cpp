#include "integration/SqlCharacterSkillSyncRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
bool SqlCharacterSkillSyncRepository::SyncSkillsFromCloneToSource(
    std::uint64_t sourceCharacterGuid,
    std::uint64_t cloneCharacterGuid)
{
    // Insert missing skills first, then merge higher current/max values onto
    // the source row. Splitting the write avoids self-table INSERT ... ON
    // DUPLICATE KEY ambiguity on MySQL.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_skills (guid, skill, value, max) "
        "SELECT {}, skill, value, max "
        "FROM character_skills WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    CharacterDatabase.DirectExecute(
        "UPDATE character_skills AS source "
        "INNER JOIN character_skills AS clone "
        "  ON clone.skill = source.skill "
        " AND clone.guid = {} "
        "SET source.value = GREATEST(source.value, clone.value), "
        "    source.max = GREATEST(source.max, clone.max) "
        "WHERE source.guid = {}",
        cloneCharacterGuid,
        sourceCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
