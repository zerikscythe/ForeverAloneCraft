#include "service/WorldBotTaxiPlanning.h"

#include "gtest/gtest.h"

#include <unordered_set>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{

WorldBotTaxiNode MakeNode(
    std::uint32_t nodeId,
    std::uint32_t zoneId,
    bool alliance,
    bool horde)
{
    WorldBotTaxiNode node;
    node.nodeId = nodeId;
    node.mapId = 0;
    node.zoneId = zoneId;
    node.x = static_cast<float>(nodeId);
    node.y = static_cast<float>(zoneId);
    node.z = 0.0f;
    node.usableByAlliance = alliance;
    node.usableByHorde = horde;
    node.name = "Node " + std::to_string(nodeId);
    return node;
}

} // namespace

TEST(WorldBotTaxiPlanningTest, FiltersKnownNodesByExploredZonesAndFaction)
{
    WorldBotTaxiNetwork const network({
        MakeNode(1, 12, true, false),
        MakeNode(2, 40, true, true),
        MakeNode(3, 10, false, true),
        MakeNode(4, 0, true, true) });

    std::unordered_set<std::uint32_t> const exploredZones = { 10u, 12u };

    std::vector<WorldBotTaxiNode> const allianceKnown = network.GetKnownNodes(exploredZones, 1);
    ASSERT_EQ(allianceKnown.size(), 1u);
    EXPECT_EQ(allianceKnown.front().nodeId, 1u);

    std::vector<WorldBotTaxiNode> const hordeKnown = network.GetKnownNodes(exploredZones, 2);
    ASSERT_EQ(hordeKnown.size(), 1u);
    EXPECT_EQ(hordeKnown.front().nodeId, 3u);
}

TEST(WorldBotTaxiPlanningTest, UnknownFactionFallsBackToEitherSideUsability)
{
    WorldBotTaxiNetwork const network({
        MakeNode(1, 12, true, false),
        MakeNode(2, 12, false, true),
        MakeNode(3, 12, false, false) });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u };

    std::vector<WorldBotTaxiNode> const known = network.GetKnownNodes(exploredZones, 0);
    ASSERT_EQ(known.size(), 2u);
    EXPECT_EQ(known[0].nodeId, 1u);
    EXPECT_EQ(known[1].nodeId, 2u);
}

TEST(WorldBotTaxiPlanningTest, FactionUsabilityHelperHonorsFactionFlags)
{
    WorldBotTaxiNode const allianceNode = MakeNode(1, 12, true, false);
    WorldBotTaxiNode const hordeNode = MakeNode(2, 12, false, true);
    WorldBotTaxiNode const sharedNode = MakeNode(3, 12, true, true);

    EXPECT_TRUE(IsWorldBotTaxiNodeUsableForFaction(allianceNode, 1));
    EXPECT_FALSE(IsWorldBotTaxiNodeUsableForFaction(allianceNode, 2));
    EXPECT_TRUE(IsWorldBotTaxiNodeUsableForFaction(hordeNode, 2));
    EXPECT_FALSE(IsWorldBotTaxiNodeUsableForFaction(hordeNode, 1));
    EXPECT_TRUE(IsWorldBotTaxiNodeUsableForFaction(sharedNode, 1));
    EXPECT_TRUE(IsWorldBotTaxiNodeUsableForFaction(sharedNode, 2));
}

} // namespace service
} // namespace living_world
