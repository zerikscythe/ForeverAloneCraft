#include "integration/SqlCharacterReputationSyncRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
bool SqlCharacterReputationSyncRepository::SyncReputationFromCloneToSource(
    std::uint64_t sourceCharacterGuid,
    std::uint64_t cloneCharacterGuid)
{
    // Insert missing faction rows first, then merge higher standings onto the
    // source row. Splitting this into two queries avoids MySQL ambiguity when
    // selecting from and updating the same table in one statement.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_reputation (guid, faction, standing, flags) "
        "SELECT {}, faction, standing, flags "
        "FROM character_reputation WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    CharacterDatabase.DirectExecute(
        "UPDATE character_reputation AS source "
        "INNER JOIN character_reputation AS clone "
        "  ON clone.faction = source.faction "
        " AND clone.guid = {} "
        "SET source.standing = IF(clone.standing > source.standing, clone.standing, source.standing), "
        "    source.flags = clone.flags "
        "WHERE source.guid = {}",
        cloneCharacterGuid,
        sourceCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
