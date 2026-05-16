#include "integration/SqlBotExploredZoneRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{

void SqlBotExploredZoneRepository::EnsureSchema() const
{
    CharacterDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_explored_zone ("
        " bot_identity_id INT UNSIGNED NOT NULL,"
        " zone_id INT UNSIGNED NOT NULL,"
        " first_seen_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " last_seen_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " PRIMARY KEY (bot_identity_id, zone_id),"
        " KEY idx_lwbez_zone (zone_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

void SqlBotExploredZoneRepository::MarkExplored(
    std::uint32_t identityId,
    std::uint32_t zoneId) const
{
    if (identityId == 0 || zoneId == 0)
        return;

    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_explored_zone "
        "(bot_identity_id, zone_id, first_seen_at, last_seen_at) "
        "VALUES ({}, {}, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE last_seen_at = NOW()",
        identityId,
        zoneId);
}

std::vector<std::uint32_t> SqlBotExploredZoneRepository::LoadExploredZones(
    std::uint32_t identityId) const
{
    std::vector<std::uint32_t> zones;
    if (identityId == 0)
        return zones;

    QueryResult result = CharacterDatabase.Query(
        "SELECT zone_id "
        "FROM living_world_bot_explored_zone "
        "WHERE bot_identity_id = {} "
        "ORDER BY first_seen_at ASC, zone_id ASC",
        identityId);

    if (!result)
        return zones;

    do
    {
        Field const* fields = result->Fetch();
        zones.push_back(fields[0].Get<std::uint32_t>());
    } while (result->NextRow());

    return zones;
}

} // namespace integration
} // namespace living_world
