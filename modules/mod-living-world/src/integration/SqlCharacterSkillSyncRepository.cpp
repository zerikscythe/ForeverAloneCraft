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
    // For skills the clone has that the source does not — insert them.
    // For skills both share — update source to whichever value is higher.
    // `value` is the current skill level; `max` is the current max cap.
    // We take GREATEST for both so neither shrinks.
    CharacterDatabase.DirectExecute(
        "INSERT INTO character_skills (guid, skill, value, max) "
        "SELECT {}, skill, value, max "
        "FROM character_skills WHERE guid = {} "
        "ON DUPLICATE KEY UPDATE "
        "  value = GREATEST(value, VALUES(value)), "
        "  max   = GREATEST(max,   VALUES(max))",
        sourceCharacterGuid,
        cloneCharacterGuid);

    return true;
}
} // namespace integration
} // namespace living_world
