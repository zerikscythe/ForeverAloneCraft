#include "service/WorldBotRoutePlanning.h"

#include "Config.h"
#include "Log.h"

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

bool GetBoolOrDefault(JsonValue const& object, std::string const& key, bool fallback = false)
{
    JsonValue const* value = TryGetObjectMember(object, key);
    if (!value || value->type != JsonValue::Type::Boolean)
        return fallback;
    return value->boolValue;
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

struct Vector3f
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vector3f MakeVector(float fromX, float fromY, float fromZ, float toX, float toY, float toZ)
{
    return {toX - fromX, toY - fromY, toZ - fromZ};
}

float VectorLength(Vector3f const& v)
{
    return std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}

std::optional<Vector3f> Normalize(Vector3f const& v)
{
    float const length = VectorLength(v);
    if (length <= 0.001f)
        return std::nullopt;
    return Vector3f{v.x / length, v.y / length, v.z / length};
}

float Dot(Vector3f const& a, Vector3f const& b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
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

std::vector<WorldBotRoutePlanner::ZoneConnector> ParseZoneConnectors(
    std::string const& payload)
{
    JsonValue const root = JsonParser(payload).Parse();
    std::vector<WorldBotRoutePlanner::ZoneConnector> connectors;
    if (!root.IsObject())
        return connectors;

    JsonValue const* connectorsValue = TryGetObjectMember(root, "connectors");
    if (!connectorsValue || !connectorsValue->IsArray())
        return connectors;

    std::uint16_t const defaultMapId =
        static_cast<std::uint16_t>(GetNumberOrDefault(root, "map_id"));

    for (JsonValue const& connectorValue : connectorsValue->arrayValue)
    {
        if (!connectorValue.IsObject())
            continue;

        JsonValue const* fromValue = TryGetObjectMember(connectorValue, "from");
        JsonValue const* toValue = TryGetObjectMember(connectorValue, "to");
        if (!fromValue || !toValue || !fromValue->IsObject() || !toValue->IsObject())
            continue;

        WorldBotRoutePlanner::ZoneConnector connector;
        connector.connectorKey = GetStringOrDefault(connectorValue, "connector_key");
        connector.mapId = static_cast<std::uint16_t>(GetNumberOrDefault(
            connectorValue,
            "map_id",
            defaultMapId));
        connector.fromZoneId = static_cast<std::uint32_t>(GetNumberOrDefault(connectorValue, "from_zone_id"));
        connector.toZoneId = static_cast<std::uint32_t>(GetNumberOrDefault(connectorValue, "to_zone_id"));
        connector.fromX = static_cast<float>(GetNumberOrDefault(*fromValue, "world_x"));
        connector.fromY = static_cast<float>(GetNumberOrDefault(*fromValue, "world_y"));
        connector.fromZ = static_cast<float>(GetNumberOrDefault(*fromValue, "world_z"));
        connector.toX = static_cast<float>(GetNumberOrDefault(*toValue, "world_x"));
        connector.toY = static_cast<float>(GetNumberOrDefault(*toValue, "world_y"));
        connector.toZ = static_cast<float>(GetNumberOrDefault(*toValue, "world_z"));
        connector.bidirectional = GetBoolOrDefault(connectorValue, "bidirectional", true);

        if (connector.fromZoneId == 0 || connector.toZoneId == 0)
            continue;

        connectors.push_back(std::move(connector));
    }

    return connectors;
}

std::string BuildCacheKey(std::uint16_t mapId, std::uint32_t zoneId)
{
    return std::to_string(mapId) + ":" + std::to_string(zoneId);
}

std::optional<std::uint32_t> TryParseZoneIdFromRouteFilename(
    std::uint16_t mapId,
    std::string const& filename)
{
    std::ostringstream mapPrefix;
    mapPrefix << "map_";
    mapPrefix.width(3);
    mapPrefix.fill('0');
    mapPrefix << mapId;
    std::string const prefix = mapPrefix.str() + "__zone_";
    if (filename.rfind(prefix, 0) != 0)
        return std::nullopt;

    std::size_t const zoneStart = prefix.size();
    std::size_t const zoneEnd = filename.find("__", zoneStart);
    if (zoneEnd == std::string::npos || zoneEnd <= zoneStart)
        return std::nullopt;

    try
    {
        return static_cast<std::uint32_t>(std::stoul(filename.substr(zoneStart, zoneEnd - zoneStart)));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

struct TerminalNodeCandidate
{
    std::size_t nodeId = 0;
    Vector3f outwardHeading {};
};

struct PlannedEntryNode
{
    std::size_t nodeId = 0;
    float attachDistanceYards = 0.0f;
};

std::vector<TerminalNodeCandidate> CollectTerminalNodeCandidates(
    WorldBotRoutePlanner::ZoneRouteGraph const& graph)
{
    std::vector<TerminalNodeCandidate> terminals;
    for (std::size_t pathIndex = 0; pathIndex < graph.pathNodeIds.size(); ++pathIndex)
    {
        auto const& nodeIds = graph.pathNodeIds[pathIndex];
        if (nodeIds.size() < 2)
            continue;

        std::size_t const firstNodeId = nodeIds.front();
        std::size_t const secondNodeId = nodeIds[1];
        std::size_t const lastNodeId = nodeIds.back();
        std::size_t const beforeLastNodeId = nodeIds[nodeIds.size() - 2];

        auto const& first = graph.nodes[firstNodeId].waypoint;
        auto const& second = graph.nodes[secondNodeId].waypoint;
        auto const& last = graph.nodes[lastNodeId].waypoint;
        auto const& beforeLast = graph.nodes[beforeLastNodeId].waypoint;

        if (auto heading = Normalize(MakeVector(
            first.x, first.y, first.z,
            second.x, second.y, second.z)))
        {
            terminals.push_back({ firstNodeId, *heading });
        }

        if (auto heading = Normalize(MakeVector(
            beforeLast.x, beforeLast.y, beforeLast.z,
            last.x, last.y, last.z)))
        {
            terminals.push_back({ lastNodeId, *heading });
        }
    }

    return terminals;
}

std::pair<std::size_t, float> FindNearestNode(
    WorldBotRoutePlanner::ZoneRouteGraph const& graph,
    float x,
    float y,
    float z)
{
    float bestDistance = std::numeric_limits<float>::max();
    std::size_t bestNode = 0;
    for (std::size_t nodeId = 0; nodeId < graph.nodes.size(); ++nodeId)
    {
        auto const& waypoint = graph.nodes[nodeId].waypoint;
        float const distance = Distance3D(x, y, z, waypoint.x, waypoint.y, waypoint.z);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNode = nodeId;
        }
    }

    return { bestNode, bestDistance };
}

std::optional<PlannedEntryNode> SelectForwardEntryNode(
    WorldBotRoutePlanner::ZoneRouteGraph const& graph,
    float startX,
    float startY,
    float startZ,
    Vector3f const& seamHeading,
    std::optional<std::int32_t> preferredPathIndex,
    float maxAttachDistanceYards)
{
    auto const scoreCandidate =
        [&](std::size_t nodeId, bool preferredPath) -> std::optional<std::pair<float, float>>
        {
            auto const& waypoint = graph.nodes[nodeId].waypoint;
            float const distance = Distance3D(startX, startY, startZ, waypoint.x, waypoint.y, waypoint.z);
            if (distance > maxAttachDistanceYards)
                return std::nullopt;

            float directionalAlignment = 1.0f;
            if (auto heading = Normalize(MakeVector(
                    startX, startY, startZ,
                    waypoint.x, waypoint.y, waypoint.z)))
            {
                directionalAlignment = Dot(seamHeading, *heading);
            }

            if (directionalAlignment < 0.15f)
                return std::nullopt;

            float const preferredBonus = preferredPath ? 10.0f : 0.0f;
            float const score = preferredBonus + (directionalAlignment * 5.0f)
                - (distance / std::max(1.0f, maxAttachDistanceYards));
            return std::make_pair(score, distance);
        };

    std::optional<PlannedEntryNode> best;
    float bestScore = -std::numeric_limits<float>::max();

    auto considerNode = [&](std::size_t nodeId, bool preferredPath)
    {
        auto const scored = scoreCandidate(nodeId, preferredPath);
        if (!scored)
            return;

        if (!best || scored->first > bestScore)
        {
            bestScore = scored->first;
            best = PlannedEntryNode{ nodeId, scored->second };
        }
    };

    if (preferredPathIndex && *preferredPathIndex >= 0)
    {
        std::size_t const pathIndex = static_cast<std::size_t>(*preferredPathIndex);
        if (pathIndex < graph.pathNodeIds.size())
        {
            for (std::size_t nodeId : graph.pathNodeIds[pathIndex])
                considerNode(nodeId, true);
        }
    }

    if (best)
        return best;

    for (std::size_t nodeId = 0; nodeId < graph.nodes.size(); ++nodeId)
        considerNode(nodeId, false);

    return best;
}

std::optional<WorldBotResolvedTravelPlan> BuildResolvedTravelPlan(
    WorldBotRoutePlanner::ZoneRouteGraph const& graph,
    std::uint16_t mapId,
    std::uint32_t zoneId,
    float startX,
    float startY,
    float startZ,
    float destX,
    float destY,
    float destZ,
    std::size_t entryNodeId,
    float attachDistance,
    std::size_t exitNodeId,
    float detachDistance,
    WorldBotTravelCapabilityTier tier,
    WorldBotTravelCapabilityConfig const& capabilityConfig)
{
    constexpr float MinMeaningfulRouteDistanceYards = 15.0f;

    std::vector<float> distances(graph.nodes.size(), std::numeric_limits<float>::max());
    std::vector<std::size_t> previous(graph.nodes.size(), std::numeric_limits<std::size_t>::max());

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

        for (auto const& [neighborId, edgeCost] : graph.nodes[nodeId].neighbors)
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
        WorldBotRouteWaypoint waypoint = graph.nodes[nodePath[index]].waypoint;
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

WorldBotTravelCapabilityConfig LoadWorldBotTravelCapabilityConfig()
{
    WorldBotTravelCapabilityConfig config;
    config.footYardsPerSecond = sConfigMgr->GetOption<float>(
        "LivingWorld.RouteTravel.FootYardsPerSecond",
        config.footYardsPerSecond);
    config.groundBasicMultiplier = sConfigMgr->GetOption<float>(
        "LivingWorld.RouteTravel.GroundBasicMultiplier",
        config.groundBasicMultiplier);
    config.groundFastMultiplier = sConfigMgr->GetOption<float>(
        "LivingWorld.RouteTravel.GroundFastMultiplier",
        config.groundFastMultiplier);
    config.flightBasicMultiplier = sConfigMgr->GetOption<float>(
        "LivingWorld.RouteTravel.FlightBasicMultiplier",
        config.flightBasicMultiplier);
    config.flightFastMultiplier = sConfigMgr->GetOption<float>(
        "LivingWorld.RouteTravel.FlightFastMultiplier",
        config.flightFastMultiplier);
    config.taxiYardsPerSecond = sConfigMgr->GetOption<float>(
        "LivingWorld.RouteTravel.TaxiYardsPerSecond",
        config.taxiYardsPerSecond);
    return config;
}

WorldBotTravelCapabilityPolicy LoadWorldBotTravelCapabilityPolicy()
{
    WorldBotTravelCapabilityPolicy policy;
    policy.groundBasicMinLevel = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
        "LivingWorld.RouteTravel.GroundBasicMinLevel",
        policy.groundBasicMinLevel));
    policy.groundFastMinLevel = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
        "LivingWorld.RouteTravel.GroundFastMinLevel",
        policy.groundFastMinLevel));
    policy.flightBasicMinLevel = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
        "LivingWorld.RouteTravel.FlightBasicMinLevel",
        policy.flightBasicMinLevel));
    policy.flightFastMinLevel = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
        "LivingWorld.RouteTravel.FlightFastMinLevel",
        policy.flightFastMinLevel));
    return policy;
}

WorldBotTravelCapabilityTier ResolveWorldBotTravelCapabilityTierForLevel(
    std::uint8_t level,
    bool allowFlightNetwork,
    WorldBotTravelCapabilityPolicy const& policy)
{
    if (allowFlightNetwork)
    {
        if (level >= policy.flightFastMinLevel)
            return WorldBotTravelCapabilityTier::FlightFast;
        if (level >= policy.flightBasicMinLevel)
            return WorldBotTravelCapabilityTier::FlightBasic;
    }

    if (level >= policy.groundFastMinLevel)
        return WorldBotTravelCapabilityTier::GroundFast;
    if (level >= policy.groundBasicMinLevel)
        return WorldBotTravelCapabilityTier::GroundBasic;

    return WorldBotTravelCapabilityTier::Foot;
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

std::vector<std::uint32_t> WorldBotRoutePlanner::DiscoverZoneIdsForMap(
    std::uint16_t mapId) const
{
    std::vector<std::uint32_t> zoneIds;
    if (_routeExportRoot.empty() || !std::filesystem::exists(_routeExportRoot))
        return zoneIds;

    for (auto const& entry : std::filesystem::directory_iterator(_routeExportRoot))
    {
        if (!entry.is_regular_file())
            continue;

        if (auto const zoneId = TryParseZoneIdFromRouteFilename(
            mapId,
            entry.path().filename().string()))
        {
            zoneIds.push_back(*zoneId);
        }
    }

    std::sort(zoneIds.begin(), zoneIds.end());
    zoneIds.erase(std::unique(zoneIds.begin(), zoneIds.end()), zoneIds.end());
    return zoneIds;
}

std::optional<std::uint32_t> WorldBotRoutePlanner::ResolveNearestZoneIdForMapPosition(
    std::uint16_t mapId,
    float x,
    float y,
    float z,
    float maxDistanceYards) const
{
    std::optional<std::uint32_t> bestZoneId;
    float bestDistance = std::numeric_limits<float>::max();

    for (std::uint32_t zoneId : DiscoverZoneIdsForMap(mapId))
    {
        auto const graph = LoadZoneGraph(mapId, zoneId);
        if (!graph || graph->nodes.empty())
            continue;

        for (auto const& node : graph->nodes)
        {
            float const distance = Distance3D(
                x, y, z,
                node.waypoint.x, node.waypoint.y, node.waypoint.z);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestZoneId = zoneId;
            }
        }
    }

    if (!bestZoneId || bestDistance > maxDistanceYards)
        return std::nullopt;

    return bestZoneId;
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

std::optional<std::filesystem::path> WorldBotRoutePlanner::FindConnectorManifestPath(
    std::uint16_t mapId) const
{
    if (_routeExportRoot.empty() || !std::filesystem::exists(_routeExportRoot))
        return std::nullopt;

    std::ostringstream mapPrefix;
    mapPrefix << "map_";
    mapPrefix.width(3);
    mapPrefix.fill('0');
    mapPrefix << mapId;
    std::string const filename = mapPrefix.str() + "__connectors.json";
    std::filesystem::path const candidate = _routeExportRoot / filename;
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate))
        return candidate;

    return std::nullopt;
}

std::vector<WorldBotRoutePlanner::ZoneConnector> WorldBotRoutePlanner::LoadConnectorsForMap(
    std::uint16_t mapId) const
{
    std::string const cacheKey = std::to_string(mapId);
    auto const cached = _connectorCache.find(cacheKey);
    if (cached != _connectorCache.end())
        return cached->second;

    std::vector<ZoneConnector> connectors;
    auto const manifestPath = FindConnectorManifestPath(mapId);
    if (!manifestPath)
    {
        _connectorCache.emplace(cacheKey, connectors);
        return connectors;
    }

    std::ifstream input(*manifestPath, std::ios::in | std::ios::binary);
    if (!input.is_open())
    {
        _connectorCache.emplace(cacheKey, connectors);
        return connectors;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    connectors = ParseZoneConnectors(buffer.str());
    _connectorCache.emplace(cacheKey, connectors);
    return connectors;
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
    WorldBotTravelCapabilityConfig const& capabilityConfig,
    float maxAttachDistanceYards,
    float maxDetachDistanceYards) const
{
    auto const graph = LoadZoneGraph(mapId, zoneId);
    if (!graph || graph->nodes.empty())
        return std::nullopt;

    auto const [entryNodeId, attachDistance] = FindNearestNode(*graph, startX, startY, startZ);
    auto const [exitNodeId, detachDistance] = FindNearestNode(*graph, destX, destY, destZ);
    if (attachDistance > maxAttachDistanceYards || detachDistance > maxDetachDistanceYards)
        return std::nullopt;

    return BuildResolvedTravelPlan(
        *graph,
        mapId,
        zoneId,
        startX,
        startY,
        startZ,
        destX,
        destY,
        destZ,
        entryNodeId,
        attachDistance,
        exitNodeId,
        detachDistance,
        tier,
        capabilityConfig);
}

std::optional<WorldBotResolvedTravelPlan> WorldBotRoutePlanner::ResolveTravelPlan(
    std::uint16_t mapId,
    std::uint32_t startZoneIdHint,
    std::uint32_t destZoneId,
    float startX,
    float startY,
    float startZ,
    float destX,
    float destY,
    float destZ,
    WorldBotTravelCapabilityTier tier,
    WorldBotTravelCapabilityConfig const& capabilityConfig) const
{
    bool const debugTrace = sConfigMgr->GetOption<bool>("LivingWorld.DebugRouteHarnessEnabled", false);
    if (destZoneId == 0)
    {
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve failed: missing dest zone map={} start_zone_hint={} start=({}, {}, {}) dest=({}, {}, {})",
                mapId, startZoneIdHint, startX, startY, startZ, destX, destY, destZ);
        }
        return std::nullopt;
    }

    auto resolveSameZone =
        [&](std::uint32_t zoneId, float maxAttachDistanceYards = 250.0f) -> std::optional<WorldBotResolvedTravelPlan>
        {
            return ResolveSameZoneTravelPlan(
                mapId,
                zoneId,
                startX,
                startY,
                startZ,
                destX,
                destY,
                destZ,
                tier,
                capabilityConfig,
                maxAttachDistanceYards,
                250.0f);
        };

    bool const startZoneIsRouted =
        startZoneIdHint != 0
        && static_cast<bool>(LoadZoneGraph(mapId, startZoneIdHint));
    float const offNetworkAttachLimit = startZoneIsRouted ? 250.0f : 1200.0f;

    if (startZoneIdHint != 0 && startZoneIdHint == destZoneId)
    {
        if (auto plan = resolveSameZone(destZoneId, offNetworkAttachLimit))
        {
            if (debugTrace)
            {
                LOG_INFO("server.worldserver",
                    "[LivingWorldRoutePlanner] resolve same-zone success map={} zone={} attach_limit={} total_yd={} eta_ms={}",
                    mapId, destZoneId, offNetworkAttachLimit, plan->totalDistanceYards, plan->etaMs);
            }
            return plan;
        }
    }

    std::optional<std::uint32_t> sourceZoneId;
    if (startZoneIsRouted)
        sourceZoneId = startZoneIdHint;

    if (!sourceZoneId)
        sourceZoneId = ResolveNearestZoneIdForMapPosition(
            mapId,
            startX,
            startY,
            startZ,
            offNetworkAttachLimit);

    if (!sourceZoneId)
    {
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve failed: no source zone map={} start_zone_hint={} off_network_attach_limit={} start=({}, {}, {}) dest_zone={}",
                mapId, startZoneIdHint, offNetworkAttachLimit, startX, startY, startZ, destZoneId);
        }
        return resolveSameZone(destZoneId, offNetworkAttachLimit);
    }

    if (*sourceZoneId == destZoneId)
    {
        auto plan = resolveSameZone(destZoneId, offNetworkAttachLimit);
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve source==dest map={} zone={} attach_limit={} success={}",
                mapId, destZoneId, offNetworkAttachLimit, plan.has_value());
        }
        return plan;
    }

    auto seam = ResolveExplicitZoneTransition(mapId, *sourceZoneId, destZoneId);
    if (!seam)
        seam = ResolveAutomaticZoneTransition(mapId, *sourceZoneId, destZoneId);
    if (!seam)
    {
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve failed: no seam map={} source_zone={} dest_zone={}",
                mapId, *sourceZoneId, destZoneId);
        }
        return std::nullopt;
    }

    auto const sourcePlan = ResolveSameZoneTravelPlan(
        mapId,
        *sourceZoneId,
        startX,
        startY,
        startZ,
        seam->fromWaypoint.x,
        seam->fromWaypoint.y,
        seam->fromWaypoint.z,
        tier,
        capabilityConfig,
        offNetworkAttachLimit,
        250.0f);
    if (!sourcePlan)
    {
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve failed: source leg map={} source_zone={} attach_limit={} seam_from=({}, {}, {}) start=({}, {}, {})",
                mapId, *sourceZoneId, offNetworkAttachLimit,
                seam->fromWaypoint.x, seam->fromWaypoint.y, seam->fromWaypoint.z,
                startX, startY, startZ);
        }
        return std::nullopt;
    }

    auto const destGraph = LoadZoneGraph(mapId, destZoneId);
    if (!destGraph || destGraph->nodes.empty())
        return std::nullopt;

    auto const [defaultExitNodeId, detachDistance] = FindNearestNode(*destGraph, destX, destY, destZ);
    if (detachDistance > 250.0f)
    {
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve failed: dest detach too far map={} dest_zone={} dest=({}, {}, {}) detach_yd={}",
                mapId, destZoneId, destX, destY, destZ, detachDistance);
        }
        return std::nullopt;
    }

    auto seamHeading = Normalize(MakeVector(
        seam->fromWaypoint.x, seam->fromWaypoint.y, seam->fromWaypoint.z,
        seam->toWaypoint.x, seam->toWaypoint.y, seam->toWaypoint.z));

    std::optional<PlannedEntryNode> entryNode;
    if (seamHeading)
    {
        entryNode = SelectForwardEntryNode(
            *destGraph,
            seam->toWaypoint.x,
            seam->toWaypoint.y,
            seam->toWaypoint.z,
            *seamHeading,
            seam->toPathIndex >= 0 ? std::optional<std::int32_t>(seam->toPathIndex) : std::nullopt,
            250.0f);
    }

    if (!entryNode)
    {
        auto const [fallbackEntryNodeId, fallbackAttachDistance] =
            FindNearestNode(*destGraph, seam->toWaypoint.x, seam->toWaypoint.y, seam->toWaypoint.z);
        if (fallbackAttachDistance > 250.0f)
        {
            if (debugTrace)
            {
                LOG_INFO("server.worldserver",
                    "[LivingWorldRoutePlanner] resolve failed: dest entry too far map={} dest_zone={} seam_to=({}, {}, {}) attach_yd={}",
                    mapId, destZoneId,
                    seam->toWaypoint.x, seam->toWaypoint.y, seam->toWaypoint.z,
                    fallbackAttachDistance);
            }
            return std::nullopt;
        }
        entryNode = PlannedEntryNode{ fallbackEntryNodeId, fallbackAttachDistance };
    }

    auto const destPlan = BuildResolvedTravelPlan(
        *destGraph,
        mapId,
        destZoneId,
        seam->toWaypoint.x,
        seam->toWaypoint.y,
        seam->toWaypoint.z,
        destX,
        destY,
        destZ,
        entryNode->nodeId,
        entryNode->attachDistanceYards,
        defaultExitNodeId,
        detachDistance,
        tier,
        capabilityConfig);
    if (!destPlan)
    {
        if (debugTrace)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorldRoutePlanner] resolve failed: dest leg map={} dest_zone={} dest=({}, {}, {}) seam_to=({}, {}, {}) entry_path={} entry_point={}",
                mapId, destZoneId,
                destX, destY, destZ,
                seam->toWaypoint.x, seam->toWaypoint.y, seam->toWaypoint.z,
                seam->toPathIndex, seam->toPointIndex);
        }
        return std::nullopt;
    }

    WorldBotResolvedTravelPlan merged;
    merged.mapId = mapId;
    merged.zoneId = destZoneId;
    merged.attachDistanceYards = sourcePlan->attachDistanceYards;
    merged.detachDistanceYards = destPlan->detachDistanceYards;
    merged.speedYardsPerSecond = ResolveWorldBotTravelSpeedYardsPerSecond(tier, capabilityConfig);

    merged.waypoints = sourcePlan->waypoints;
    merged.waypoints.push_back(seam->toWaypoint);

    auto samePoint = [](WorldBotRouteWaypoint const& a, WorldBotRouteWaypoint const& b)
    {
        return Distance3D(a.x, a.y, a.z, b.x, b.y, b.z) <= 0.01f;
    };

    for (std::size_t index = 0; index < destPlan->waypoints.size(); ++index)
    {
        if (!merged.waypoints.empty() && samePoint(merged.waypoints.back(), destPlan->waypoints[index]))
            continue;
        merged.waypoints.push_back(destPlan->waypoints[index]);
    }

    float cumulativeDistance = 0.0f;
    float prevX = startX;
    float prevY = startY;
    float prevZ = startZ;
    for (WorldBotRouteWaypoint& waypoint : merged.waypoints)
    {
        cumulativeDistance += Distance3D(prevX, prevY, prevZ, waypoint.x, waypoint.y, waypoint.z);
        waypoint.cumulativeDistanceYards = cumulativeDistance;
        prevX = waypoint.x;
        prevY = waypoint.y;
        prevZ = waypoint.z;
    }

    merged.totalDistanceYards = cumulativeDistance;
    merged.routeDistanceYards = std::max(
        0.0f,
        merged.totalDistanceYards - merged.attachDistanceYards - merged.detachDistanceYards);
    merged.etaMs = static_cast<std::uint32_t>(
        std::max(1.0f, (merged.totalDistanceYards / std::max(0.1f, merged.speedYardsPerSecond)) * 1000.0f));

    if (debugTrace)
    {
        LOG_INFO("server.worldserver",
            "[LivingWorldRoutePlanner] resolve success map={} source_zone={} dest_zone={} connector='{}' explicit={} total_yd={} eta_ms={} waypoints={}",
            mapId,
            *sourceZoneId,
            destZoneId,
            seam->connectorKey,
            seam->explicitConnector,
            merged.totalDistanceYards,
            merged.etaMs,
            merged.waypoints.size());
    }

    return merged;
}

std::optional<WorldBotZoneTransitionCandidate> WorldBotRoutePlanner::ResolveExplicitZoneTransition(
    std::uint16_t mapId,
    std::uint32_t fromZoneId,
    std::uint32_t toZoneId,
    float maxAttachDistanceYards) const
{
    auto const fromGraph = LoadZoneGraph(mapId, fromZoneId);
    auto const toGraph = LoadZoneGraph(mapId, toZoneId);
    if (!fromGraph || !toGraph)
        return std::nullopt;

    auto const connectors = LoadConnectorsForMap(mapId);
    if (connectors.empty())
        return std::nullopt;

    auto nearestNodeDistance =
        [](WorldBotRoutePlanner::ZoneRouteGraph const& graph, float x, float y, float z)
            -> std::optional<WorldBotRouteWaypoint>
        {
            if (graph.nodes.empty())
                return std::nullopt;

            float bestDistance = std::numeric_limits<float>::max();
            std::optional<WorldBotRouteWaypoint> best;
            for (auto const& node : graph.nodes)
            {
                float const distance = Distance3D(
                    x, y, z,
                    node.waypoint.x, node.waypoint.y, node.waypoint.z);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = node.waypoint;
                }
            }

            if (!best)
                return std::nullopt;

            best->cumulativeDistanceYards = bestDistance;
            return best;
        };

    for (ZoneConnector const& connector : connectors)
    {
        bool reversed = false;
        if (connector.fromZoneId == fromZoneId && connector.toZoneId == toZoneId)
        {
            reversed = false;
        }
        else if (connector.bidirectional && connector.fromZoneId == toZoneId && connector.toZoneId == fromZoneId)
        {
            reversed = true;
        }
        else
        {
            continue;
        }

        float const fromX = reversed ? connector.toX : connector.fromX;
        float const fromY = reversed ? connector.toY : connector.fromY;
        float const fromZ = reversed ? connector.toZ : connector.fromZ;
        float const toX = reversed ? connector.fromX : connector.toX;
        float const toY = reversed ? connector.fromY : connector.toY;
        float const toZ = reversed ? connector.fromZ : connector.toZ;

        auto const fromNearest = nearestNodeDistance(*fromGraph, fromX, fromY, fromZ);
        auto const toNearest = nearestNodeDistance(*toGraph, toX, toY, toZ);
        if (!fromNearest || !toNearest)
            continue;
        if (fromNearest->cumulativeDistanceYards > maxAttachDistanceYards
            || toNearest->cumulativeDistanceYards > maxAttachDistanceYards)
        {
            continue;
        }

        WorldBotZoneTransitionCandidate candidate;
        candidate.connectorKey = connector.connectorKey;
        candidate.explicitConnector = true;
        candidate.fromPathIndex = fromNearest->pathIndex;
        candidate.fromPointIndex = fromNearest->pointIndex;
        candidate.toPathIndex = toNearest->pathIndex;
        candidate.toPointIndex = toNearest->pointIndex;
        candidate.fromWaypoint = *fromNearest;
        candidate.fromWaypoint.mapId = mapId;
        candidate.fromWaypoint.x = fromX;
        candidate.fromWaypoint.y = fromY;
        candidate.fromWaypoint.z = fromZ;
        candidate.toWaypoint = *toNearest;
        candidate.toWaypoint.mapId = mapId;
        candidate.toWaypoint.x = toX;
        candidate.toWaypoint.y = toY;
        candidate.toWaypoint.z = toZ;
        candidate.seamDistanceYards = Distance3D(fromX, fromY, fromZ, toX, toY, toZ);
        candidate.sourceHeadingAlignment = 1.0f;
        candidate.targetHeadingAlignment = 1.0f;
        candidate.score = 1000.0f - candidate.seamDistanceYards;
        return candidate;
    }

    return std::nullopt;
}

std::optional<WorldBotZoneTransitionCandidate> WorldBotRoutePlanner::ResolveAutomaticZoneTransition(
    std::uint16_t mapId,
    std::uint32_t fromZoneId,
    std::uint32_t toZoneId,
    float maxSeamDistanceYards) const
{
    auto const fromGraph = LoadZoneGraph(mapId, fromZoneId);
    auto const toGraph = LoadZoneGraph(mapId, toZoneId);
    if (!fromGraph || !toGraph)
        return std::nullopt;

    auto const fromTerminals = CollectTerminalNodeCandidates(*fromGraph);
    auto const toTerminals = CollectTerminalNodeCandidates(*toGraph);
    if (fromTerminals.empty() || toTerminals.empty())
        return std::nullopt;

    constexpr float MinDirectionalAlignment = 0.15f;

    std::optional<WorldBotZoneTransitionCandidate> best;
    for (TerminalNodeCandidate const& fromTerminal : fromTerminals)
    {
        auto const& fromWaypoint = fromGraph->nodes[fromTerminal.nodeId].waypoint;

        for (TerminalNodeCandidate const& toTerminal : toTerminals)
        {
            auto const& toWaypoint = toGraph->nodes[toTerminal.nodeId].waypoint;
            float const seamDistance = Distance3D(
                fromWaypoint.x, fromWaypoint.y, fromWaypoint.z,
                toWaypoint.x, toWaypoint.y, toWaypoint.z);
            if (seamDistance > maxSeamDistanceYards)
                continue;

            auto seamHeading = Normalize(MakeVector(
                fromWaypoint.x, fromWaypoint.y, fromWaypoint.z,
                toWaypoint.x, toWaypoint.y, toWaypoint.z));
            if (!seamHeading)
                continue;

            float const sourceAlignment = Dot(fromTerminal.outwardHeading, *seamHeading);
            float const targetAlignment = Dot(*seamHeading, toTerminal.outwardHeading);
            if (sourceAlignment < MinDirectionalAlignment || targetAlignment < MinDirectionalAlignment)
                continue;

            float const score = (sourceAlignment * 2.0f) + targetAlignment
                - (seamDistance / std::max(1.0f, maxSeamDistanceYards));

            if (!best || score > best->score)
            {
                best = WorldBotZoneTransitionCandidate{
                    "",
                    false,
                    fromWaypoint.pathIndex,
                    fromWaypoint.pointIndex,
                    toWaypoint.pathIndex,
                    toWaypoint.pointIndex,
                    fromWaypoint,
                    toWaypoint,
                    seamDistance,
                    sourceAlignment,
                    targetAlignment,
                    score
                };
            }
        }
    }

    return best;
}

} // namespace service
} // namespace living_world
