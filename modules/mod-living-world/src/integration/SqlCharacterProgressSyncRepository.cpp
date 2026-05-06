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
    // Honor and kills use GREATEST so we only ever move them forward:
    // if the bot spent honor (totalHonorPoints < snapshot), we don't penalize
    // the source; if the bot earned net positive honor we carry it forward.
    // totalKills is strictly cumulative so GREATEST is always correct.
    CharacterDatabase.DirectExecute(
        "UPDATE characters SET level={}, xp={}, money={}, "
        "totalHonorPoints = GREATEST(totalHonorPoints, {}), "
        "totalKills = GREATEST(totalKills, {}) "
        "WHERE guid={}",
        snapshot.level,
        snapshot.experience,
        snapshot.money,
        snapshot.totalHonorPoints,
        snapshot.totalKills,
        characterGuid);
    return true;
}
} // namespace integration
} // namespace living_world
