#include "integration/SqlZoneIndexRepository.h"
#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
namespace
{
model::ZoneEntry BuildZoneEntry(Field const* f)
{
    model::ZoneEntry e;
    e.zoneId    = f[0].Get<std::uint32_t>();
    e.mapId     = f[1].Get<std::uint16_t>();
    e.zoneName  = f[2].Get<std::string>();
    e.faction   = f[3].Get<std::uint8_t>();
    e.zoneType  = f[4].Get<std::string>();
    e.hasHerbs  = f[5].Get<std::uint8_t>() != 0;
    e.hasOre    = f[6].Get<std::uint8_t>() != 0;
    e.hasFish   = f[7].Get<std::uint8_t>() != 0;
    e.minLevel  = f[8].Get<std::uint8_t>();
    e.maxLevel  = f[9].Get<std::uint8_t>();
    e.anchorX   = f[10].Get<float>();
    e.anchorY   = f[11].Get<float>();
    e.anchorZ   = f[12].Get<float>();
    return e;
}

constexpr char const* kSelectAll =
    "SELECT zone_id, map_id, zone_name, faction, zone_type, "
    "has_herbs, has_ore, has_fish, min_level, max_level, "
    "anchor_x, anchor_y, anchor_z "
    "FROM living_world_zone_index";
} // namespace

std::vector<model::ZoneEntry> SqlZoneIndexRepository::LoadAll() const
{
    std::vector<model::ZoneEntry> result;
    QueryResult qr = WorldDatabase.Query(kSelectAll);
    if (!qr)
        return result;
    do
    {
        result.push_back(BuildZoneEntry(qr->Fetch()));
    } while (qr->NextRow());
    return result;
}

std::optional<model::ZoneEntry> SqlZoneIndexRepository::Find(std::uint32_t zoneId) const
{
    QueryResult qr = WorldDatabase.Query(
        "{} WHERE zone_id = {}", kSelectAll, zoneId);
    if (!qr)
        return std::nullopt;
    return BuildZoneEntry(qr->Fetch());
}

} // namespace integration
} // namespace living_world
