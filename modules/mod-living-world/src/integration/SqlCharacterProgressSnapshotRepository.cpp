#include "integration/SqlCharacterProgressSnapshotRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
std::optional<model::CharacterProgressSnapshot>
SqlCharacterProgressSnapshotRepository::LoadSnapshot(
    std::uint64_t characterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT c.level, c.xp, c.money, "
        "COALESCE((SELECT COUNT(*) FROM character_queststatus_rewarded WHERE guid = c.guid), 0), "
        "COALESCE((SELECT COUNT(*) FROM character_achievement WHERE guid = c.guid), 0), "
        "COALESCE((SELECT SUM(GREATEST(standing, 0)) FROM character_reputation WHERE guid = c.guid), 0), "
        "c.totalHonorPoints, c.totalKills "
        "FROM characters c WHERE c.guid = {} LIMIT 1",
        characterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    Field const* fields = result->Fetch();
    model::CharacterProgressSnapshot snapshot;
    snapshot.level = fields[0].Get<std::uint8_t>();
    snapshot.experience = fields[1].Get<std::uint32_t>();
    snapshot.money = fields[2].Get<std::uint32_t>();
    snapshot.completedQuestCount = fields[3].Get<std::uint32_t>();
    snapshot.achievementCount = fields[4].Get<std::uint32_t>();
    snapshot.totalReputationStanding = fields[5].Get<std::int64_t>();
    snapshot.totalHonorPoints = fields[6].Get<std::uint32_t>();
    snapshot.totalKills = fields[7].Get<std::uint32_t>();
    return snapshot;
}
} // namespace integration
} // namespace living_world
