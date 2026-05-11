#include "integration/SqlTaskPointRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

#include <limits>
#include <queue>
#include <unordered_map>

namespace living_world
{
namespace integration
{
namespace
{
model::TaskPointEntry BuildPoint(Field const* f)
{
    model::TaskPointEntry p;
    p.pointId    = f[0].Get<std::uint32_t>();
    p.pointKey   = f[1].Get<std::string>();
    p.zoneId     = f[2].Get<std::uint32_t>();
    p.mapId      = f[3].Get<std::uint16_t>();
    p.pointType  = f[4].Get<std::string>();
    p.pointName  = f[5].Get<std::string>();
    p.x          = f[6].Get<float>();
    p.y          = f[7].Get<float>();
    p.z          = f[8].Get<float>();
    return p;
}

model::TaskTransitRouteEntry BuildRoute(Field const* f)
{
    model::TaskTransitRouteEntry r;
    r.routeId         = f[0].Get<std::uint32_t>();
    r.routeKey        = f[1].Get<std::string>();
    r.sourcePointKey  = f[2].Get<std::string>();
    r.destPointKey    = f[3].Get<std::string>();
    r.transitType     = f[4].Get<std::string>();
    r.requiredFaction = f[5].Get<std::uint8_t>();
    r.minLevel        = f[6].Get<std::uint8_t>();
    r.maxLevel        = f[7].Get<std::uint8_t>();
    r.durationSec     = f[8].Get<std::uint32_t>();
    r.displayName     = f[9].Get<std::string>();
    r.sourceZoneId    = f[10].Get<std::uint32_t>();
    r.sourceMapId     = f[11].Get<std::uint16_t>();
    r.sourcePointName = f[12].Get<std::string>();
    r.sourceX         = f[13].Get<float>();
    r.sourceY         = f[14].Get<float>();
    r.sourceZ         = f[15].Get<float>();
    r.destZoneId      = f[16].Get<std::uint32_t>();
    r.destMapId       = f[17].Get<std::uint16_t>();
    r.destPointName   = f[18].Get<std::string>();
    r.destX           = f[19].Get<float>();
    r.destY           = f[20].Get<float>();
    r.destZ           = f[21].Get<float>();
    return r;
}

model::ZoneAnchorEntry BuildZoneAnchor(Field const* f)
{
    model::ZoneAnchorEntry e;
    e.anchorId        = f[0].Get<std::uint32_t>();
    e.zoneId          = f[1].Get<std::uint32_t>();
    e.pointKey        = f[2].Get<std::string>();
    e.anchorRole      = f[3].Get<std::string>();
    e.requiredFaction = f[4].Get<std::uint8_t>();
    e.minLevel        = f[5].Get<std::uint8_t>();
    e.maxLevel        = f[6].Get<std::uint8_t>();
    e.weight          = f[7].Get<std::uint8_t>();
    e.notes           = f[8].IsNull() ? "" : f[8].Get<std::string>();
    return e;
}

model::ZoneContentEntry BuildZoneContent(Field const* f)
{
    model::ZoneContentEntry e;
    e.contentId        = f[0].Get<std::uint32_t>();
    e.zoneId           = f[1].Get<std::uint32_t>();
    e.contentKind      = f[2].Get<std::string>();
    e.subjectId        = f[3].IsNull() ? 0u : f[3].Get<std::uint32_t>();
    e.subjectKey       = f[4].IsNull() ? "" : f[4].Get<std::string>();
    e.displayName      = f[5].Get<std::string>();
    e.requiredFaction  = f[6].Get<std::uint8_t>();
    e.minLevel         = f[7].Get<std::uint8_t>();
    e.maxLevel         = f[8].Get<std::uint8_t>();
    e.minSkill         = f[9].Get<std::uint16_t>();
    e.maxSkill         = f[10].Get<std::uint16_t>();
    e.weight           = f[11].Get<std::uint8_t>();
    e.anchorPointKey   = f[12].IsNull() ? "" : f[12].Get<std::string>();
    e.returnAnchorRole = f[13].IsNull() ? "" : f[13].Get<std::string>();
    e.notes            = f[14].IsNull() ? "" : f[14].Get<std::string>();
    return e;
}
} // namespace

std::optional<model::TaskPointEntry> SqlTaskPointRepository::FindByKey(std::string const& pointKey) const
{
    QueryResult qr = WorldDatabase.Query(
        "SELECT point_id, point_key, zone_id, map_id, point_type, point_name, x, y, z "
        "FROM living_world_task_point WHERE point_key = '{}' LIMIT 1",
        pointKey);
    if (!qr)
        return std::nullopt;

    return BuildPoint(qr->Fetch());
}

std::optional<model::TaskPointEntry> SqlTaskPointRepository::FindByZoneAndType(
    std::uint32_t zoneId,
    std::string const& pointType) const
{
    QueryResult qr = WorldDatabase.Query(
        "SELECT point_id, point_key, zone_id, map_id, point_type, point_name, x, y, z "
        "FROM living_world_task_point "
        "WHERE zone_id = {} AND point_type = '{}' "
        "ORDER BY point_key ASC LIMIT 1",
        zoneId,
        pointType);
    if (!qr)
        return std::nullopt;

    return BuildPoint(qr->Fetch());
}

std::optional<model::ZoneAnchorEntry> SqlTaskPointRepository::FindZoneAnchor(
    std::uint32_t zoneId,
    std::string const& anchorRole,
    std::uint8_t faction,
    std::uint8_t level) const
{
    QueryResult qr = WorldDatabase.Query(
        "SELECT anchor_id, zone_id, point_key, anchor_role, required_faction, min_level, max_level, weight, notes "
        "FROM living_world_zone_anchor "
        "WHERE zone_id = {} AND anchor_role = '{}' "
        "  AND (required_faction = 0 OR required_faction = {}) "
        "  AND min_level <= {} AND max_level >= {} "
        "ORDER BY weight DESC, point_key ASC LIMIT 1",
        zoneId,
        anchorRole,
        faction,
        level,
        level);
    if (!qr)
        return std::nullopt;

    return BuildZoneAnchor(qr->Fetch());
}

std::vector<model::ZoneContentEntry> SqlTaskPointRepository::LoadZoneContentByKind(
    std::string const& contentKind,
    std::uint8_t faction,
    std::uint8_t level) const
{
    std::vector<model::ZoneContentEntry> result;
    QueryResult qr = WorldDatabase.Query(
        "SELECT content_id, zone_id, content_kind, subject_id, subject_key, display_name, "
        "required_faction, min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes "
        "FROM living_world_zone_content "
        "WHERE content_kind = '{}' "
        "  AND (required_faction = 0 OR required_faction = {}) "
        "  AND min_level <= {} AND max_level >= {} "
        "ORDER BY weight DESC, zone_id ASC, content_id ASC",
        contentKind,
        faction,
        level,
        level);
    if (!qr)
        return result;

    do
    {
        result.push_back(BuildZoneContent(qr->Fetch()));
    } while (qr->NextRow());

    return result;
}

std::vector<model::ZoneContentEntry> SqlTaskPointRepository::LoadZoneContentByZoneAndKind(
    std::uint32_t zoneId,
    std::string const& contentKind,
    std::uint8_t faction,
    std::uint8_t level) const
{
    std::vector<model::ZoneContentEntry> result;
    QueryResult qr = WorldDatabase.Query(
        "SELECT content_id, zone_id, content_kind, subject_id, subject_key, display_name, "
        "required_faction, min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes "
        "FROM living_world_zone_content "
        "WHERE zone_id = {} AND content_kind = '{}' "
        "  AND (required_faction = 0 OR required_faction = {}) "
        "  AND min_level <= {} AND max_level >= {} "
        "ORDER BY weight DESC, content_id ASC",
        zoneId,
        contentKind,
        faction,
        level,
        level);
    if (!qr)
        return result;

    do
    {
        result.push_back(BuildZoneContent(qr->Fetch()));
    } while (qr->NextRow());

    return result;
}

std::vector<model::TaskTransitRouteEntry> SqlTaskPointRepository::FindTransitPathForZones(
    std::uint32_t sourceZoneId,
    std::uint32_t destZoneId,
    std::uint8_t faction,
    std::uint8_t level) const
{
    std::vector<model::TaskTransitRouteEntry> result;
    if (sourceZoneId == 0 || destZoneId == 0 || sourceZoneId == destZoneId)
        return result;

    QueryResult qr = WorldDatabase.Query(
        "SELECT r.route_id, r.route_key, r.source_point_key, r.dest_point_key, "
        "r.transit_type, r.required_faction, r.min_level, r.max_level, r.duration_sec, r.display_name, "
        "sp.zone_id, sp.map_id, sp.point_name, sp.x, sp.y, sp.z, "
        "dp.zone_id, dp.map_id, dp.point_name, dp.x, dp.y, dp.z "
        "FROM living_world_transit_route r "
        "JOIN living_world_task_point sp ON sp.point_key = r.source_point_key "
        "JOIN living_world_task_point dp ON dp.point_key = r.dest_point_key "
        "WHERE (r.required_faction = 0 OR r.required_faction = {}) "
        "  AND r.min_level <= {} AND r.max_level >= {} "
        "ORDER BY sp.zone_id ASC, dp.zone_id ASC, "
        "FIELD(r.transit_type, 'portal', 'zeppelin', 'boat', 'taxi'), r.duration_sec ASC",
        faction,
        level,
        level);
    if (!qr)
        return result;

    std::vector<model::TaskTransitRouteEntry> routes;
    do
    {
        routes.push_back(BuildRoute(qr->Fetch()));
    } while (qr->NextRow());

    if (routes.empty())
        return result;

    std::unordered_map<std::uint32_t, std::vector<std::size_t>> edgesByZone;
    for (std::size_t i = 0; i < routes.size(); ++i)
        edgesByZone[routes[i].sourceZoneId].push_back(i);

    using QueueEntry = std::pair<std::uint32_t, std::uint32_t>; // cost, zone
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> open;
    std::unordered_map<std::uint32_t, std::uint32_t> bestCost;
    std::unordered_map<std::uint32_t, std::uint32_t> previousZone;
    std::unordered_map<std::uint32_t, std::size_t> previousRouteIndex;

    bestCost[sourceZoneId] = 0;
    open.emplace(0u, sourceZoneId);

    while (!open.empty())
    {
        auto const [cost, zoneId] = open.top();
        open.pop();

        auto const bestItr = bestCost.find(zoneId);
        if (bestItr == bestCost.end() || cost != bestItr->second)
            continue;

        if (zoneId == destZoneId)
            break;

        auto const edgeItr = edgesByZone.find(zoneId);
        if (edgeItr == edgesByZone.end())
            continue;

        for (std::size_t const routeIndex : edgeItr->second)
        {
            model::TaskTransitRouteEntry const& route = routes[routeIndex];
            std::uint32_t const nextZoneId = route.destZoneId;
            std::uint32_t const nextCost = cost + std::max<std::uint32_t>(1u, route.durationSec);

            auto const nextItr = bestCost.find(nextZoneId);
            if (nextItr != bestCost.end() && nextItr->second <= nextCost)
                continue;

            bestCost[nextZoneId] = nextCost;
            previousZone[nextZoneId] = zoneId;
            previousRouteIndex[nextZoneId] = routeIndex;
            open.emplace(nextCost, nextZoneId);
        }
    }

    if (bestCost.find(destZoneId) == bestCost.end())
        return result;

    std::vector<std::size_t> pathRouteIndices;
    for (std::uint32_t zoneId = destZoneId; zoneId != sourceZoneId; )
    {
        auto const routeItr = previousRouteIndex.find(zoneId);
        auto const zoneItr = previousZone.find(zoneId);
        if (routeItr == previousRouteIndex.end() || zoneItr == previousZone.end())
        {
            pathRouteIndices.clear();
            break;
        }

        pathRouteIndices.push_back(routeItr->second);
        zoneId = zoneItr->second;
    }

    if (pathRouteIndices.empty())
        return result;

    for (auto itr = pathRouteIndices.rbegin(); itr != pathRouteIndices.rend(); ++itr)
        result.push_back(routes[*itr]);

    return result;
}

} // namespace integration
} // namespace living_world