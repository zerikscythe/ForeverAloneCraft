#pragma once

#include "service/BotActivitySessionComposer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace living_world
{
namespace service
{

enum class WorldBotTravelCapabilityTier : std::uint8_t
{
    Foot = 0,
    GroundBasic = 1,
    GroundFast = 2,
    FlightBasic = 3,
    FlightFast = 4,
    Taxi = 5,
};

struct WorldBotTravelCapabilityConfig
{
    float footYardsPerSecond = 4.5f;
    float groundBasicMultiplier = 1.6f;
    float groundFastMultiplier = 2.0f;
    float flightBasicMultiplier = 2.5f;
    float flightFastMultiplier = 4.1f;
    float taxiYardsPerSecond = 32.0f;
};

float ResolveWorldBotTravelSpeedYardsPerSecond(
    WorldBotTravelCapabilityTier tier,
    WorldBotTravelCapabilityConfig const& config = {});

struct WorldBotRouteWaypoint
{
    std::uint16_t mapId = 0;
    float         x = 0.0f;
    float         y = 0.0f;
    float         z = 0.0f;
    float         cumulativeDistanceYards = 0.0f;
    std::string   routeKey;
    std::int32_t  pathIndex = -1;
    std::int32_t  pointIndex = -1;
};

struct WorldBotResolvedTravelPlan
{
    std::uint16_t mapId = 0;
    std::uint32_t zoneId = 0;
    float attachDistanceYards = 0.0f;
    float routeDistanceYards = 0.0f;
    float detachDistanceYards = 0.0f;
    float totalDistanceYards = 0.0f;
    float speedYardsPerSecond = 0.0f;
    std::uint32_t etaMs = 0;
    std::vector<WorldBotRouteWaypoint> waypoints;

    [[nodiscard]] bool empty() const { return waypoints.empty(); }
};

struct WorldBotTravelPositionSample
{
    std::uint16_t mapId = 0;
    float         x = 0.0f;
    float         y = 0.0f;
    float         z = 0.0f;
};

WorldBotTravelPositionSample SampleWorldBotTravelPlanPosition(
    WorldBotResolvedTravelPlan const& plan,
    float startX,
    float startY,
    float startZ,
    float traveledDistanceYards);

class WorldBotRoutePlanner
{
public:
    explicit WorldBotRoutePlanner(std::filesystem::path routeExportRoot);

    [[nodiscard]] std::optional<WorldBotResolvedTravelPlan> ResolveSameZoneTravelPlan(
        std::uint16_t mapId,
        std::uint32_t zoneId,
        float startX,
        float startY,
        float startZ,
        float destX,
        float destY,
        float destZ,
        WorldBotTravelCapabilityTier tier,
        WorldBotTravelCapabilityConfig const& capabilityConfig = {}) const;

    struct RouteConnectionRef
    {
        std::int32_t pathIndex = -1;
        std::int32_t anchorIndex = -1;
    };

    struct RouteAnchor
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct RoutePoint
    {
        std::uint16_t mapId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float distanceFromStartYards = 0.0f;
    };

    struct RoutePath
    {
        std::string routeKey;
        std::vector<RouteAnchor> anchors;
        std::vector<RoutePoint> points;
        std::optional<RouteConnectionRef> startConnection;
        std::optional<RouteConnectionRef> endConnection;
    };

    struct GraphNode
    {
        WorldBotRouteWaypoint waypoint;
        std::vector<std::pair<std::size_t, float>> neighbors;
    };

    struct ZoneRouteGraph
    {
        std::uint16_t mapId = 0;
        std::uint32_t zoneId = 0;
        std::string routeGroupKey;
        std::vector<RoutePath> paths;
        std::vector<GraphNode> nodes;
        std::vector<std::vector<std::size_t>> pathNodeIds;
    };

private:
    [[nodiscard]] std::optional<ZoneRouteGraph> LoadZoneGraph(
        std::uint16_t mapId,
        std::uint32_t zoneId) const;

    [[nodiscard]] std::optional<std::filesystem::path> FindRouteExportPath(
        std::uint16_t mapId,
        std::uint32_t zoneId) const;

    std::filesystem::path _routeExportRoot;
    mutable std::unordered_map<std::string, std::optional<ZoneRouteGraph>> _graphCache;
};

using WorldBotRoutePlanResolver = std::function<std::optional<WorldBotResolvedTravelPlan>(
    service::AmbientStep const& step,
    std::uint16_t startMapId,
    float startX,
    float startY,
    float startZ)>;

} // namespace service
} // namespace living_world
