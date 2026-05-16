#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace living_world
{
namespace service
{

struct WorldBotTaxiNode
{
    std::uint32_t nodeId = 0;
    std::uint16_t mapId = 0;
    std::uint32_t zoneId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool usableByAlliance = false;
    bool usableByHorde = false;
    std::string name;
};

struct WorldBotTaxiPathLink
{
    std::uint32_t pathId = 0;
    std::uint32_t fromNodeId = 0;
    std::uint32_t toNodeId = 0;
    std::uint32_t price = 0;
    float rideDistanceYards = 0.0f;
    std::uint32_t rideEtaMs = 0;
};

struct WorldBotResolvedTaxiRoute
{
    std::vector<std::uint32_t> nodeIds;
    std::vector<WorldBotTaxiPathLink> links;
    float totalDistanceYards = 0.0f;
    std::uint32_t totalEtaMs = 0;

    [[nodiscard]] bool empty() const { return links.empty(); }
};

bool IsWorldBotTaxiNodeUsableForFaction(
    WorldBotTaxiNode const& node,
    std::uint8_t faction);

class WorldBotTaxiNetwork
{
public:
    WorldBotTaxiNetwork() = default;
    explicit WorldBotTaxiNetwork(std::vector<WorldBotTaxiNode> nodes);
    WorldBotTaxiNetwork(
        std::vector<WorldBotTaxiNode> nodes,
        std::vector<WorldBotTaxiPathLink> links);

    [[nodiscard]] std::vector<WorldBotTaxiNode> const& GetNodes() const;
    [[nodiscard]] std::vector<WorldBotTaxiPathLink> const& GetLinks() const;

    [[nodiscard]] std::vector<WorldBotTaxiNode> GetKnownNodes(
        std::unordered_set<std::uint32_t> const& exploredZoneIds,
        std::uint8_t faction) const;

    [[nodiscard]] std::optional<WorldBotResolvedTaxiRoute> ResolveKnownRoute(
        std::uint32_t sourceNodeId,
        std::uint32_t destinationNodeId,
        std::unordered_set<std::uint32_t> const& exploredZoneIds,
        std::uint8_t faction) const;

private:
    std::vector<WorldBotTaxiNode> _nodes;
    std::vector<WorldBotTaxiPathLink> _links;
};

using WorldBotTaxiZoneResolver = std::function<std::uint32_t(
    std::uint16_t mapId,
    float x,
    float y,
    float z)>;

WorldBotTaxiNetwork LoadWorldBotTaxiNetwork(
    WorldBotTaxiZoneResolver zoneResolver = {});

} // namespace service
} // namespace living_world
