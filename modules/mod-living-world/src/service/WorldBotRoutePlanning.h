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

struct WorldBotTravelCapabilityPolicy
{
    std::uint8_t groundBasicMinLevel = 20;
    std::uint8_t groundFastMinLevel = 40;
    std::uint8_t flightBasicMinLevel = 60;
    std::uint8_t flightFastMinLevel = 70;
};

float ResolveWorldBotTravelSpeedYardsPerSecond(
    WorldBotTravelCapabilityTier tier,
    WorldBotTravelCapabilityConfig const& config = {});

WorldBotTravelCapabilityConfig LoadWorldBotTravelCapabilityConfig();
WorldBotTravelCapabilityPolicy LoadWorldBotTravelCapabilityPolicy();

WorldBotTravelCapabilityTier ResolveWorldBotTravelCapabilityTierForLevel(
    std::uint8_t level,
    bool allowFlightNetwork = false,
    WorldBotTravelCapabilityPolicy const& policy = {});

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

struct WorldBotZoneTransitionCandidate
{
    std::string connectorKey;
    bool explicitConnector = false;
    std::int32_t fromPathIndex = -1;
    std::int32_t fromPointIndex = -1;
    std::int32_t toPathIndex = -1;
    std::int32_t toPointIndex = -1;
    WorldBotRouteWaypoint fromWaypoint;
    WorldBotRouteWaypoint toWaypoint;
    float seamDistanceYards = 0.0f;
    float sourceHeadingAlignment = 0.0f;
    float targetHeadingAlignment = 0.0f;
    float score = 0.0f;
};

struct WorldBotTravelPositionSample
{
    std::uint16_t mapId = 0;
    float         x = 0.0f;
    float         y = 0.0f;
    float         z = 0.0f;
};

enum class WorldBotRoutePathKind : std::uint8_t
{
    MainRoute = 1,
    SubRoute = 2,
    Area = 3,
};

std::string ToString(WorldBotRoutePathKind kind);
std::optional<WorldBotRoutePathKind> TryParseWorldBotRoutePathKind(std::string const& value);

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

    [[nodiscard]] std::optional<WorldBotResolvedTravelPlan> ResolveTravelPlan(
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
        WorldBotTravelCapabilityConfig const& capabilityConfig = {}) const;

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
        WorldBotTravelCapabilityConfig const& capabilityConfig = {},
        float maxAttachDistanceYards = 250.0f,
        float maxDetachDistanceYards = 250.0f) const;

    [[nodiscard]] std::optional<WorldBotZoneTransitionCandidate> ResolveAutomaticZoneTransition(
        std::uint16_t mapId,
        std::uint32_t fromZoneId,
        std::uint32_t toZoneId,
        float maxSeamDistanceYards = 400.0f) const;

    [[nodiscard]] std::optional<WorldBotZoneTransitionCandidate> ResolveExplicitZoneTransition(
        std::uint16_t mapId,
        std::uint32_t fromZoneId,
        std::uint32_t toZoneId,
        float maxAttachDistanceYards = 250.0f) const;

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
        WorldBotRoutePathKind kind = WorldBotRoutePathKind::MainRoute;
        bool closedLoop = false;
        std::vector<std::string> resourceKinds;
        std::vector<std::string> resourceItems;
        std::string assistKind;
        std::vector<std::string> lowerContextKeys;
        std::vector<std::string> upperContextKeys;
        std::vector<std::string> destinationKeys;
        std::string lowerLabel;
        std::string upperLabel;
        std::vector<RouteAnchor> anchors;
        std::vector<RoutePoint> points;
        std::optional<RouteConnectionRef> startConnection;
        std::optional<RouteConnectionRef> endConnection;

        [[nodiscard]] bool SupportsResourceKind(std::string const& resourceKind) const;
        [[nodiscard]] bool SupportsLowerContextKey(std::string const& destinationKey) const;
        [[nodiscard]] bool SupportsUpperContextKey(std::string const& destinationKey) const;
        [[nodiscard]] bool SupportsDestinationKey(std::string const& destinationKey) const;
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

    struct ZoneConnector
    {
        std::string connectorKey;
        std::uint16_t mapId = 0;
        std::uint32_t fromZoneId = 0;
        std::uint32_t toZoneId = 0;
        float fromX = 0.0f;
        float fromY = 0.0f;
        float fromZ = 0.0f;
        float toX = 0.0f;
        float toY = 0.0f;
        float toZ = 0.0f;
        bool bidirectional = true;
    };

    [[nodiscard]] std::vector<RoutePath> LoadZonePaths(
        std::uint16_t mapId,
        std::uint32_t zoneId,
        std::optional<WorldBotRoutePathKind> kindFilter = std::nullopt) const;

private:
    [[nodiscard]] std::optional<ZoneRouteGraph> LoadZoneGraph(
        std::uint16_t mapId,
        std::uint32_t zoneId) const;

    [[nodiscard]] std::vector<std::uint32_t> DiscoverZoneIdsForMap(
        std::uint16_t mapId) const;

    [[nodiscard]] std::optional<std::uint32_t> ResolveNearestZoneIdForMapPosition(
        std::uint16_t mapId,
        float x,
        float y,
        float z,
        float maxDistanceYards = 250.0f) const;

    [[nodiscard]] std::optional<std::filesystem::path> FindRouteExportPath(
        std::uint16_t mapId,
        std::uint32_t zoneId) const;

    [[nodiscard]] std::optional<std::filesystem::path> FindConnectorManifestPath(
        std::uint16_t mapId) const;

    [[nodiscard]] std::vector<ZoneConnector> LoadConnectorsForMap(
        std::uint16_t mapId) const;

    std::filesystem::path _routeExportRoot;
    mutable std::unordered_map<std::string, std::optional<ZoneRouteGraph>> _graphCache;
    mutable std::unordered_map<std::string, std::vector<ZoneConnector>> _connectorCache;
};

using WorldBotRoutePlanResolver = std::function<std::optional<WorldBotResolvedTravelPlan>(
    service::AmbientStep const& step,
    std::uint16_t startMapId,
    float startX,
    float startY,
    float startZ)>;

} // namespace service
} // namespace living_world
