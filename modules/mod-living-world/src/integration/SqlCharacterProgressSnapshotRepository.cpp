#include "integration/SqlCharacterProgressSnapshotRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
std::optional<model::CharacterProgressSnapshot>
SqlCharacterProgressSnapshotRepository::LoadSnapshot(
    std::uint64_t characterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT level, xp, money FROM characters WHERE guid = {} LIMIT 1",
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
    return snapshot;
}
} // namespace integration
} // namespace living_world
