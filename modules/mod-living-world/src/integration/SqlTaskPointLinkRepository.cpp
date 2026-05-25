#include "integration/SqlTaskPointLinkRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
namespace
{
model::TaskPointLinkEntry BuildTaskPointLink(Field const* f)
{
    model::TaskPointLinkEntry entry;
    entry.fromPointKey = f[0].Get<std::string>();
    entry.toPointKey = f[1].Get<std::string>();
    entry.linkKind = f[2].Get<std::string>();
    entry.manualVerified = f[3].Get<bool>();
    entry.successCount = f[4].Get<std::uint32_t>();
    entry.failureCount = f[5].Get<std::uint32_t>();
    entry.source = f[6].IsNull() ? "" : f[6].Get<std::string>();
    entry.notes = f[7].IsNull() ? "" : f[7].Get<std::string>();

    entry.fromPoint.pointId = f[8].Get<std::uint32_t>();
    entry.fromPoint.pointKey = f[9].Get<std::string>();
    entry.fromPoint.zoneId = f[10].Get<std::uint32_t>();
    entry.fromPoint.mapId = f[11].Get<std::uint16_t>();
    entry.fromPoint.pointType = f[12].Get<std::string>();
    entry.fromPoint.pointName = f[13].Get<std::string>();
    entry.fromPoint.x = f[14].Get<float>();
    entry.fromPoint.y = f[15].Get<float>();
    entry.fromPoint.z = f[16].Get<float>();

    entry.toPoint.pointId = f[17].Get<std::uint32_t>();
    entry.toPoint.pointKey = f[18].Get<std::string>();
    entry.toPoint.zoneId = f[19].Get<std::uint32_t>();
    entry.toPoint.mapId = f[20].Get<std::uint16_t>();
    entry.toPoint.pointType = f[21].Get<std::string>();
    entry.toPoint.pointName = f[22].Get<std::string>();
    entry.toPoint.x = f[23].Get<float>();
    entry.toPoint.y = f[24].Get<float>();
    entry.toPoint.z = f[25].Get<float>();
    return entry;
}
} // namespace

std::vector<model::TaskPointLinkEntry> SqlTaskPointLinkRepository::LoadLocalNavigationLinks(
    std::uint16_t mapId,
    std::uint32_t zoneId,
    std::string const& linkKind) const
{
    std::vector<model::TaskPointLinkEntry> result;
    if (mapId == 0 || zoneId == 0)
        return result;

    QueryResult qr = WorldDatabase.Query(
        "SELECT l.from_point_key, l.to_point_key, l.link_kind, l.manual_verified, "
        "l.success_count, l.failure_count, l.source, l.notes, "
        "fp.point_id, fp.point_key, fp.zone_id, fp.map_id, fp.point_type, fp.point_name, fp.x, fp.y, fp.z, "
        "tp.point_id, tp.point_key, tp.zone_id, tp.map_id, tp.point_type, tp.point_name, tp.x, tp.y, tp.z "
        "FROM living_world_task_point_link l "
        "JOIN living_world_task_point fp ON fp.point_key = l.from_point_key "
        "JOIN living_world_task_point tp ON tp.point_key = l.to_point_key "
        "WHERE l.link_kind = '{}' "
        "  AND (l.success_count > 0 OR l.manual_verified <> 0) "
        "  AND fp.map_id = {} AND tp.map_id = {} "
        "  AND fp.zone_id = {} AND tp.zone_id = {} "
        "ORDER BY l.manual_verified DESC, l.success_count DESC, l.failure_count ASC, "
        "fp.point_key ASC, tp.point_key ASC",
        linkKind,
        mapId,
        mapId,
        zoneId,
        zoneId);
    if (!qr)
        return result;

    do
    {
        result.push_back(BuildTaskPointLink(qr->Fetch()));
    } while (qr->NextRow());

    return result;
}

} // namespace integration
} // namespace living_world
