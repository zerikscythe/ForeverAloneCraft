#include "integration/SqlCharacterProgressSyncRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
bool SqlCharacterProgressSyncRepository::SyncProgressToCharacter(
    std::uint64_t characterGuid,
    model::CharacterProgressSnapshot const& snapshot)
{
    // DirectExecute blocks until the query is committed, ensuring the row is
    // durable before a bot session loads the character.
    CharacterDatabase.DirectExecute(
        "UPDATE characters SET level={}, xp={}, money={} WHERE guid={}",
        snapshot.level,
        snapshot.experience,
        snapshot.money,
        characterGuid);
    return true;
}
} // namespace integration
} // namespace living_world
