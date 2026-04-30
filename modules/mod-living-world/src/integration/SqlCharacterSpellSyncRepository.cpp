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
    // This AzerothCore branch stores only (guid, spell, specMask) in
    // character_spell. There is no active/disabled state column here.
    // Copy learned spells additively and preserve the clone's spec mask.
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO character_spell (guid, spell, specMask) "
        "SELECT {}, spell, specMask "
        "FROM character_spell "
        "WHERE guid = {}",
        sourceCharacterGuid,
        cloneCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
