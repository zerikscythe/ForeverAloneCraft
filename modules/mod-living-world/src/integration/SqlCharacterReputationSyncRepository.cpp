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
    // For each faction the clone has standing in, insert or update the source
    // row: existing standing is replaced only if the clone's value is higher.
    // This is safe because reputation can only legitimately grow during a
    // bot session.
    CharacterDatabase.DirectExecute(
        "INSERT INTO character_reputation (guid, faction, standing, flags) "
        "SELECT {}, faction, standing, flags "
        "FROM character_reputation WHERE guid = {} "
        "ON DUPLICATE KEY UPDATE "
        "standing = IF(VALUES(standing) > standing, VALUES(standing), standing), "
        "flags = VALUES(flags)",
        sourceCharacterGuid,
        cloneCharacterGuid);
    return true;
}
} // namespace integration
} // namespace living_world
