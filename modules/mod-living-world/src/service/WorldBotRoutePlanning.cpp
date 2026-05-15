#include "service/WorldBotRoutePlanning.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace living_world
{
namespace service
{
namespace
{

struct JsonValue
{
    enum class Type : std::uint8_t
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object,
    };

    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    Array arrayValue;
    Object objectValue;

    [[nodiscard]] bool IsNull() const { return type == Type::Null; }
    [[nodiscard]] bool IsNumber() const { return type == Type::Number; }
    [[nodiscard]] bool IsString() const { return type == Type::String; }
    [[nodiscard]] bool IsArray() const { return type == Type::Array; }
    [[nodiscard]] bool IsObject() const { return type == Type::Object; }
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view input)
        : _input(input)
    {
    }

    JsonValue Parse()
    {
        JsonValue value = ParseValue();
        SkipWhitespace();
        if (!AtEnd())
            throw std::runtime_error("Unexpected trailing JSON content");
        return value;
    }

private:
    JsonValue ParseValue()
    {
        SkipWhitespace();
        if (AtEnd())
            throw std::runtime_error("Unexpected end of JSON input");

        char const ch = Peek();
        if (ch == '{')
            return ParseObject();
        if (ch == '[')
            return ParseArray();
        if (ch == '"')
            return ParseString();
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)))
            return ParseNumber();
        if (StartsWith("true"))
            return ParseBoolean(true);
        if (StartsWith("false"))
            return ParseBoolean(false);
        if (StartsWith("null"))
            return ParseNull();

        throw std::runtime_error("Unsupported JSON token");
    }

    JsonValue ParseObject()
    {
        Consume('{');
        JsonValue value;
        value.type = JsonValue::Type::Object;

        SkipWhitespace();
        if (TryConsume('}'))
            return value;

        while (true)
        {
            JsonValue key = ParseString();
            SkipWhitespace();
            Consume(':');
            value.objectValue.emplace(std::move(key.stringValue), ParseValue());
            SkipWhitespace();
            if (TryConsume('}'))
                return value;
            Consume(',');
        }
    }

    JsonValue ParseArray()
    {
        Consume('[');
        JsonValue value;
        value.type = JsonValue::Type::Array;

        SkipWhitespace();
        if (TryConsume(']'))
            return value;

        while (true)
        {
            value.arrayValue.push_back(ParseValue());
            SkipWhitespace();
            if (TryConsume(']'))
                return value;
            Consume(',');
        }
    }

    JsonValue ParseString()
    {
        Consume('"');
        JsonValue value;
        value.type = JsonValue::Type::String;

        while (!AtEnd())
        {
            char const ch = Get();
            if (ch == '"')
                return value;

            if (ch == '\\')
            {
                if (AtEnd())
                    throw std::runtime_error("Unterminated JSON escape");

                char const escaped = Get();
                switch (escaped)
                {
                    case '"':
                    case '\\':
                    case '/':
                        value.stringValue.push_back(escaped);
                        break;
                    case 'b':
                        value.stringValue.push_back('\b');
                        break;
                    case 'f':
                        value.stringValue.push_back('\f');
                        break;
                    case 'n':
                        value.stringValue.push_back('\n');
                        break;
                    case 'r':
                        value.stringValue.push_back('\r');
                        break;
                    case 't':
                        value.stringValue.push_back('\t');
                        break;
                    default:
                        throw std::runtime_error("Unsupported JSON escape");
                }
                continue;
            }

            value.stringValue.push_back(ch);
        }

        throw std::runtime_error("Unterminated JSON string");
    }

    JsonValue ParseNumber()
    {
        std::size_t const start = _offset;
        if (Peek() == '-')
            ++_offset;

        while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
            ++_offset;

        if (!AtEnd() && Peek() == '.')
        {
            ++_offset;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
                ++_offset;
        }

        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E'))
        {
            ++_offset;
            if (!AtEnd() && (Peek() == '+' || Peek() == '-'))
                ++_offset;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
                ++_offset;
        }

        JsonValue value;
        value.type = JsonValue::Type::Number;
        value.numberValue = std::stod(std::string(_input.substr(start, _offset - start)));
        return value;
    }

    JsonValue ParseBoolean(bool boolValue)
    {
        _offset += boolValue ? 4u : 5u;
        JsonValue value;
        value.type = JsonValue::Type::Boolean;
        value.boolValue = boolValue;
        return value;
    }

    JsonValue ParseNull()
    {
        _offset += 4u;
        return {};
    }

    void SkipWhitespace()
    {
        while (!AtEnd() && std::isspace(static_cast<unsigned char>(_input[_offset])))
            ++_offset;
    }

    [[nodiscard]] bool StartsWith(char const* text) const
    {
        std::size_t const length = std::char_traits<char>::length(text);
        return _input.substr(_offset, length) == text;
    }

    void Consume(char expected)
    {
        SkipWhitespace();
        if (AtEnd() || _input[_offset] != expected)
            throw std::runtime_error("Unexpected JSON token");
        ++_offset;
    }

    [[nodiscard]] bool TryConsume(char expected)
    {
        SkipWhitespace();
        if (AtEnd() || _input[_offset] != expected)
            return false;
        ++_offset;
        return true;
    }

    [[nodiscard]] char Peek() const
    {
        return _input[_offset];
    }

    [[nodiscard]] char Get()
    {
        return _input[_offset++];
    }

    [[nodiscard]] bool AtEnd() const
    {
        return _offset >= _input.size();
    }

    std::string_view _input;
    std::size_t _offset = 0;
};

JsonValue const* TryGetObjectMember(JsonValue const& object, std::string const& key)
{
    if (!object.IsObject())
        return nullptr;

    auto const itr = object.objectValue.find(key);
    if (itr == object.objectValue.end())
        return nullptr;
    return &itr->second;
}

double GetNumberOrDefault(JsonValue const& object, std::string const& key, double fallback = 0.0)
{
    JsonValue const* value = TryGetObjectMember(object, key);
    if (!value || !value->IsNumber())
        return fallback;
    return value->numberValue;
}

std::string GetStringOrDefault(JsonValue const& object, std::string const& key)
{
    JsonValue const* value = TryGetObjectMember(object, key);
    if (!value || !value->IsString())
        return {};
    return value->stringValue;
}

std::optional<WorldBotRoutePlanner::RouteConnectionRef> ParseConnection(JsonValue const* connectionValue)
{
    if (!connectionValue || connectionValue->IsNull())
        return std::nullopt;

    WorldBotRoutePlanner::RouteConnectionRef connection;
    connection.pathIndex = static_cast<std::int32_t>(GetNumberOrDefault(*connectionValue, "path_index", -1.0));
    connection.anchorIndex = static_cast<std::int32_t>(GetNumberOrDefault(*connectionValue, "anchor_index", -1.0));
    if (connection.pathIndex < 0 || connection.anchorIndex < 0)
        return std::nullopt;

    return connection;
}

float Distance3D(float ax, float ay, float az, float bx, float by, float bz)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    float const dz = az - bz;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

void AddNeighbor(
    WorldBotRoutePlanner::GraphNode& from,
    std::size_t neighborId,
    float distance)
{
    auto const existing = std::find_if(
        from.neighbors.begin(),
        from.neighbors.end(),
        [neighborId](auto const& entry) { return entry.first == neighborId; });
    if (existing != from.neighbors.end())
    {
        existing->second = std::min(existing->second, distance);
        return;
    }

    from.neighbors.emplace_back(neighborId, distance);
}

void AddBidirectionalEdge(
    std::vector<WorldBotRoutePlanner::GraphNode>& nodes,
    std::size_t a,
    std::size_t b,
    float distance)
{
    if (a == b || a >= nodes.size() || b >= nodes.size())
        return;

    AddNeighbor(nodes[a], b, distance);
    AddNeighbor(nodes[b], a, distance);
}

std::optional<WorldBotRoutePlanner::ZoneRouteGraph> ParseZoneRouteGraph(
    std::string const& payload)
{
    JsonValue const root = JsonParser(payload).Parse();
    if (!root.IsObject())
        return std::nullopt;

    WorldBotRoutePlanner::ZoneRouteGraph graph;
    graph.mapId = static_cast<std::uint16_t>(GetNumberOrDefault(root, "map_id"));
    graph.zoneId = static_cast<std::uint32_t>(GetNumberOrDefault(root, "zone_id"));
    graph.routeGroupKey = GetStringOrDefault(root, "route_group_key");

    JsonValue const* pathsValue = TryGetObjectMember(root, "paths");
    if (!pathsValue || !pathsValue->IsArray())
        return std::nullopt;

    graph.paths.reserve(pathsValue->arrayValue.size());
    graph.pathNodeIds.reserve(pathsValue->arrayValue.size());

    for (std::size_t pathIndex = 0; pathIndex < pathsValue->arrayValue.size(); ++pathIndex)
    {
        JsonValue const& pathValue = pathsValue->arrayValue[pathIndex];
        if (!pathValue.IsObject())
            continue;

        WorldBotRoutePlanner::RoutePath path;
        path.routeKey = GetStringOrDefault(pathValue, "path_key");
        path.startConnection = ParseConnection(TryGetObjectMember(pathValue, "start_connection"));
        path.endConnection = ParseConnection(TryGetObjectMember(pathValue, "end_connection"));

        if (JsonValue const* anchorsValue = TryGetObjectMember(pathValue, "anchors");
            anchorsValue && anchorsValue->IsArray())
        {
            for (JsonValue const& anchorValue : anchorsValue->arrayValue)
            {
                if (!anchorValue.IsObject())
                    continue;

                WorldBotRoutePlanner::RouteAnchor anchor;
                anchor.x = static_cast<float>(GetNumberOrDefault(anchorValue, "world_x"));
                anchor.y = static_cast<float>(GetNumberOrDefault(anchorValue, "world_y"));
                anchor.z = static_cast<float>(GetNumberOrDefault(anchorValue, "world_z"));
                path.anchors.push_back(anchor);
            }
        }

        std::vector<std::size_t> nodeIds;
        if (JsonValue const* pointsValue = TryGetObjectMember(pathValue, "movement_points");
            pointsValue && pointsValue->IsArray())
        {
            path.points.reserve(pointsValue->arrayValue.size());
            nodeIds.reserve(pointsValue->arrayValue.size());

            for (std::size_t pointIndex = 0; pointIndex < pointsValue->arrayValue.size(); ++pointIndex)
            {
                JsonValue const& pointValue = pointsValue->arrayValue[pointIndex];
                if (!pointValue.IsObject())
                    continue;

                WorldBotRoutePlanner::RoutePoint point;
                point.mapId = static_cast<std::uint16_t>(GetNumberOrDefault(pointValue, "map_id", graph.mapId));
                point.x = static_cast<float>(GetNumberOrDefault(pointValue, "world_x"));
                point.y = static_cast<float>(GetNumberOrDefault(pointValue, "world_y"));
                point.z = static_cast<float>(GetNumberOrDefault(pointValue, "world_z"));
                point.distanceFromStartYards = static_cast<float>(GetNumberOrDefault(pointValue, "distance_from_start_yards"));
                path.points.push_back(point);

                WorldBotRoutePlanner::GraphNode node;
                node.waypoint.mapId = point.mapId;
                node.waypoint.x = point.x;
                node.waypoint.y = point.y;
                node.waypoint.z = point.z;
                node.waypoint.cumulativeDistanceYards = point.distanceFromStartYards;
                node.waypoint.routeKey = path.routeKey;
                node.waypoint.pathIndex = static_cast<std::int32_t>(pathIndex);
                node.waypoint.pointIndex = static_cast<std::int32_t>(pointIndex);

                nodeIds.push_back(graph.nodes.size());
                graph.nodes.push_back(std::move(node));
            }
        }

        graph.pathNodeIds.push_back(std::move(nodeIds));
        graph.paths.push_back(std::move(path));
    }

    for (std::size_t pathIndex = 0; pathIndex < graph.paths.size(); ++pathIndex)
    {
        auto const& path = graph.paths[pathIndex];
        auto const& nodeIds = graph.pathNodeIds[pathIndex];
        for (std::size_t pointIndex = 1; pointIndex < nodeIds.size(); ++pointIndex)
        {
            std::size_t const prevId = nodeIds[pointIndex - 1];
            std::size_t const currId = nodeIds[pointIndex];
            float distance = Distance3D(
                graph.nodes[prevId].waypoint.x,
                graph.nodes[prevId].waypoint.y,
                graph.nodes[prevId].waypoint.z,
                graph.nodes[currId].waypoint.x,
                graph.nodes[currId].waypoint.y,
                graph.nodes[currId].waypoint.z);

            if (pointIndex < path.points.size())
            {
                float const segmentDistance = std::fabs(
                    path.points[pointIndex].distanceFromStartYards
                    - path.points[pointIndex - 1].distanceFromStartYards);
                if (segmentDistance > 0.0f)
                    distance = segmentDistance;
            }

            AddBidirectionalEdge(graph.nodes, prevId, currId, distance);
        }
    }

    auto resolveAnchorNodeId = [&graph](std::int32_t pathIndex, std::int32_t anchorIndex)
        -> std::optional<std::size_t>
    {
        if (pathIndex < 0 || anchorIndex < 0)
            return std::nullopt;

        std::size_t const resolvedPathIndex = static_cast<std::size_t>(pathIndex);
        std::size_t const resolvedAnchorIndex = static_cast<std::size_t>(anchorIndex);
        if (resolvedPathIndex >= graph.paths.size())
            return std::nullopt;
        if (resolvedAnchorIndex >= graph.paths[resolvedPathIndex].anchors.size())
            return std::nullopt;
        if (graph.pathNodeIds[resolvedPathIndex].empty())
            return std::nullopt;

        WorldBotRoutePlanner::RouteAnchor const& anchor =
            graph.paths[resolvedPathIndex].anchors[resolvedAnchorIndex];
        float bestDistance = std::numeric_limits<float>::max();
        std::size_t bestNodeId = graph.pathNodeIds[resolvedPathIndex].front();

        for (std::size_t nodeId : graph.pathNodeIds[resolvedPathIndex])
        {
            WorldBotRouteWaypoint const& waypoint = graph.nodes[nodeId].waypoint;
            float const candidateDistance = Distance3D(
                anchor.x, anchor.y, anchor.z,
                waypoint.x, waypoint.y, waypoint.z);
            if (candidateDistance < bestDistance)
            {
                bestDistance = candidateDistance;
                bestNodeId = nodeId;
            }
        }

        return bestNodeId;
    };

    for (std::size_t pathIndex = 0; pathIndex < graph.paths.size(); ++pathIndex)
    {
        auto const& path = graph.paths[pathIndex];
        auto const& nodeIds = graph.pathNodeIds[pathIndex];
        if (nodeIds.empty())
            continue;

        if (path.startConnection)
        {
            if (auto const targetNodeId = resolveAnchorNodeId(
                path.startConnection->pathIndex,
                path.startConnection->anchorIndex))
            {
                float const distance = Distance3D(
                    graph.nodes[nodeIds.front()].waypoint.x,
                    graph.nodes[nodeIds.front()].waypoint.y,
                    graph.nodes[nodeIds.front()].waypoint.z,
                    graph.nodes[*targetNodeId].waypoint.x,
                    graph.nodes[*targetNodeId].waypoint.y,
                    graph.nodes[*targetNodeId].waypoint.z);
                AddBidirectionalEdge(graph.nodes, nodeIds.front(), *targetNodeId, distance);
            }
        }

        if (path.endConnection)
        {
            if (auto const targetNodeId = resolveAnchorNodeId(
                path.endConnection->pathIndex,
                path.endConnection->anchorIndex))
            {
                float const distance = Distance3D(
                    graph.nodes[nodeIds.back()].waypoint.x,
                    graph.nodes[nodeIds.back()].waypoint.y,
                    graph.nodes[nodeIds.back()].waypoint.z,
                    graph.nodes[*targetNodeId].waypoint.x,
                    graph.nodes[*targetNodeId].waypoint.y,
                    graph.nodes[*targetNodeId].waypoint.z);
                AddBidirectionalEdge(graph.nodes, nodeIds.back(), *targetNodeId, distance);
            }
        }
    }

    return graph;
}

std::string BuildCacheKey(std::uint16_t mapId, std::uint32_t zoneId)
{
    return std::to_string(mapId) + ":" + std::to_string(zoneId);
}

} // namespace

float ResolveWorldBotTravelSpeedYardsPerSecond(
    WorldBotTravelCapabilityTier tier,
    WorldBotTravelCapabilityConfig const& config)
{
    float const footSpeed = std::max(0.1f, config.footYardsPerSecond);
    switch (tier)
    {
        case WorldBotTravelCapabilityTier::GroundBasic:
            return footSpeed * std::max(1.0f, config.groundBasicMultiplier);
        case WorldBotTravelCapabilityTier::GroundFast:
            return footSpeed * std::max(config.groundBasicMultiplier, config.groundFastMultiplier);
        case WorldBotTravelCapabilityTier::FlightBasic:
            return footSpeed * std::max(config.groundFastMultiplier, config.flightBasicMultiplier);
        case WorldBotTravelCapabilityTier::FlightFast:
            return footSpeed * std::max(config.flightBasicMultiplier, config.flightFastMultiplier);
        case WorldBotTravelCapabilityTier::Taxi:
            return std::max(footSpeed, config.taxiYardsPerSecond);
        case WorldBotTravelCapabilityTier::Foot:
        default:
            return footSpeed;
    }
}

WorldBotTravelPositionSample SampleWorldBotTravelPlanPosition(
    WorldBotResolvedTravelPlan const& plan,
    float startX,
    float startY,
    float startZ,
    float traveledDistanceYards)
{
    WorldBotTravelPositionSample sample;
    sample.mapId = plan.mapId;
    if (plan.waypoints.empty())
    {
        sample.x = startX;
        sample.y = startY;
        sample.z = startZ;
        return sample;
    }

    float remaining = std::clamp(traveledDistanceYards, 0.0f, std::max(0.0f, plan.totalDistanceYards));
    float segmentStartX = startX;
    float segmentStartY = startY;
    float segmentStartZ = startZ;

    for (WorldBotRouteWaypoint const& waypoint : plan.waypoints)
    {
        float const segmentLength = Distance3D(
            segmentStartX, segmentStartY, segmentStartZ,
            waypoint.x, waypoint.y, waypoint.z);
        if (segmentLength <= 0.001f)
        {
            segmentStartX = waypoint.x;
            segmentStartY = waypoint.y;
            segmentStartZ = waypoint.z;
            continue;
        }

        if (remaining <= segmentLength)
        {
            float const progress = remaining / segmentLength;
            sample.x = segmentStartX + ((waypoint.x - segmentStartX) * progress);
            sample.y = segmentStartY + ((waypoint.y - segmentStartY) * progress);
            sample.z = segmentStartZ + ((waypoint.z - segmentStartZ) * progress);
            return sample;
        }

        remaining -= segmentLength;
        segmentStartX = waypoint.x;
        segmentStartY = waypoint.y;
        segmentStartZ = waypoint.z;
    }

    WorldBotRouteWaypoint const& last = plan.waypoints.back();
    sample.x = last.x;
    sample.y = last.y;
    sample.z = last.z;
    return sample;
}

WorldBotRoutePlanner::WorldBotRoutePlanner(std::filesystem::path routeExportRoot)
    : _routeExportRoot(std::move(routeExportRoot))
{
}

std::optional<std::filesystem::path> WorldBotRoutePlanner::FindRouteExportPath(
    std::uint16_t mapId,
    std::uint32_t zoneId) const
{
    if (_routeExportRoot.empty() || !std::filesystem::exists(_routeExportRoot))
        return std::nullopt;

    std::ostringstream mapPrefix;
    mapPrefix << "map_";
    mapPrefix.width(3);
    mapPrefix.fill('0');
    mapPrefix << mapId;
    std::string const prefix = mapPrefix.str() + "__zone_" + std::to_string(zoneId) + "__";

    for (auto const& entry : std::filesystem::directory_iterator(_routeExportRoot))
    {
        if (!entry.is_regular_file())
            continue;

        std::string const filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) != 0)
            continue;
        if (filename.size() < std::string("__routes.json").size()
            || filename.substr(filename.size() - std::string("__routes.json").size()) != "__routes.json")
            continue;
        return entry.path();
    }

    return std::nullopt;
}

std::optional<WorldBotRoutePlanner::ZoneRouteGraph> WorldBotRoutePlanner::LoadZoneGraph(
    std::uint16_t mapId,
    std::uint32_t zoneId) const
{
    std::string const cacheKey = BuildCacheKey(mapId, zoneId);
    auto const cached = _graphCache.find(cacheKey);
    if (cached != _graphCache.end())
        return cached->second;

    auto const routePath = FindRouteExportPath(mapId, zoneId);
    if (!routePath)
    {
        _graphCache.emplace(cacheKey, std::nullopt);
        return std::nullopt;
    }

    std::ifstream input(*routePath, std::ios::in | std::ios::binary);
    if (!input.is_open())
    {
        _graphCache.emplace(cacheKey, std::nullopt);
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    auto parsed = ParseZoneRouteGraph(buffer.str());
    if (!parsed)
    {
        _graphCache.emplace(cacheKey, std::nullopt);
        return std::nullopt;
    }

    _graphCache.emplace(cacheKey, parsed);
    return parsed;
}

std::optional<WorldBotResolvedTravelPlan> WorldBotRoutePlanner::ResolveSameZoneTravelPlan(
    std::uint16_t mapId,
    std::uint32_t zoneId,
    float startX,
    float startY,
    float startZ,
    float destX,
    float destY,
    float destZ,
    WorldBotTravelCapabilityTier tier,
    WorldBotTravelCapabilityConfig const& capabilityConfig) const
{
    auto const graph = LoadZoneGraph(mapId, zoneId);
    if (!graph || graph->nodes.empty())
        return std::nullopt;

    constexpr float MaxAttachDistanceYards = 250.0f;
    constexpr float MaxDetachDistanceYards = 250.0f;
    constexpr float MinMeaningfulRouteDistanceYards = 15.0f;

    auto const nearestNode = [&graph](float x, float y, float z) -> std::pair<std::size_t, float>
    {
        float bestDistance = std::numeric_limits<float>::max();
        std::size_t bestNode = 0;
        for (std::size_t nodeId = 0; nodeId < graph->nodes.size(); ++nodeId)
        {
            auto const& waypoint = graph->nodes[nodeId].waypoint;
            float const distance = Distance3D(x, y, z, waypoint.x, waypoint.y, waypoint.z);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestNode = nodeId;
            }
        }
        return { bestNode, bestDistance };
    };

    auto const [entryNodeId, attachDistance] = nearestNode(startX, startY, startZ);
    auto const [exitNodeId, detachDistance] = nearestNode(destX, destY, destZ);
    if (attachDistance > MaxAttachDistanceYards || detachDistance > MaxDetachDistanceYards)
        return std::nullopt;

    std::vector<float> distances(graph->nodes.size(), std::numeric_limits<float>::max());
    std::vector<std::size_t> previous(graph->nodes.size(), std::numeric_limits<std::size_t>::max());

    using QueueEntry = std::pair<float, std::size_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
    distances[entryNodeId] = 0.0f;
    queue.push({0.0f, entryNodeId});

    while (!queue.empty())
    {
        auto const [cost, nodeId] = queue.top();
        queue.pop();
        if (cost > distances[nodeId])
            continue;
        if (nodeId == exitNodeId)
            break;

        for (auto const& [neighborId, edgeCost] : graph->nodes[nodeId].neighbors)
        {
            float const nextCost = cost + edgeCost;
            if (nextCost >= distances[neighborId])
                continue;
            distances[neighborId] = nextCost;
            previous[neighborId] = nodeId;
            queue.push({nextCost, neighborId});
        }
    }

    if (!std::isfinite(distances[exitNodeId]))
        return std::nullopt;

    float const routeDistance = distances[exitNodeId];
    if ((attachDistance + routeDistance + detachDistance) < MinMeaningfulRouteDistanceYards)
        return std::nullopt;

    std::vector<std::size_t> nodePath;
    for (std::size_t nodeId = exitNodeId;
         nodeId != std::numeric_limits<std::size_t>::max();
         nodeId = previous[nodeId])
    {
        nodePath.push_back(nodeId);
        if (nodeId == entryNodeId)
            break;
    }

    if (nodePath.empty() || nodePath.back() != entryNodeId)
        return std::nullopt;

    std::reverse(nodePath.begin(), nodePath.end());

    WorldBotResolvedTravelPlan plan;
    plan.mapId = mapId;
    plan.zoneId = zoneId;
    plan.attachDistanceYards = attachDistance;
    plan.routeDistanceYards = routeDistance;
    plan.detachDistanceYards = detachDistance;
    plan.totalDistanceYards = attachDistance + routeDistance + detachDistance;
    plan.speedYardsPerSecond = ResolveWorldBotTravelSpeedYardsPerSecond(tier, capabilityConfig);
    plan.etaMs = static_cast<std::uint32_t>(
        std::max(1.0f, (plan.totalDistanceYards / std::max(0.1f, plan.speedYardsPerSecond)) * 1000.0f));

    plan.waypoints.reserve(nodePath.size() + 1u);
    float cumulativeDistance = attachDistance;
    for (std::size_t index = 0; index < nodePath.size(); ++index)
    {
        WorldBotRouteWaypoint waypoint = graph->nodes[nodePath[index]].waypoint;
        if (index > 0)
        {
            WorldBotRouteWaypoint const& prev = plan.waypoints.back();
            cumulativeDistance += Distance3D(
                prev.x, prev.y, prev.z,
                waypoint.x, waypoint.y, waypoint.z);
        }
        waypoint.cumulativeDistanceYards = cumulativeDistance;
        plan.waypoints.push_back(std::move(waypoint));
    }

    if (detachDistance > 1.0f)
    {
        WorldBotRouteWaypoint finalWaypoint;
        finalWaypoint.mapId = mapId;
        finalWaypoint.x = destX;
        finalWaypoint.y = destY;
        finalWaypoint.z = destZ;
        finalWaypoint.cumulativeDistanceYards = plan.totalDistanceYards;
        plan.waypoints.push_back(std::move(finalWaypoint));
    }

    return plan;
}

} // namespace service
} // namespace living_world
