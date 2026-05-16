#pragma once

#include <cstdint>
#include <functional>
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

bool IsWorldBotTaxiNodeUsableForFaction(
    WorldBotTaxiNode const& node,
    std::uint8_t faction);

class WorldBotTaxiNetwork
{
public:
    WorldBotTaxiNetwork() = default;
    explicit WorldBotTaxiNetwork(std::vector<WorldBotTaxiNode> nodes);

    [[nodiscard]] std::vector<WorldBotTaxiNode> const& GetNodes() const;

    [[nodiscard]] std::vector<WorldBotTaxiNode> GetKnownNodes(
        std::unordered_set<std::uint32_t> const& exploredZoneIds,
        std::uint8_t faction) const;

private:
    std::vector<WorldBotTaxiNode> _nodes;
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
