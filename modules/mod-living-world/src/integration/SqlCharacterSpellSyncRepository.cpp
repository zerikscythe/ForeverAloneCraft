#include "integration/SqlCharacterSpellSyncRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
bool SqlCharacterSpellSyncRepository::SyncSpellsFromCloneToSource(
    std::uint64_t sourceCharacterGuid,
    std::uint64_t cloneCharacterGuid)
{
    // Copy spells the clone learned that the source does not have.
    // active=1 means the spell is currently learned (not removed/replaced).
    // INSERT IGNORE preserves the source's existing spell rows untouched.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_spell (guid, spell, active, disabled) "
        "SELECT {}, spell, active, disabled "
        "FROM character_spell "
        "WHERE guid = {} AND active = 1",
        sourceCharacterGuid,
        cloneCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
