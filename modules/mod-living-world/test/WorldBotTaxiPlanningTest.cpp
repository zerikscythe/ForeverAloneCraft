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

WorldBotResolvedTravelPlan MakeGroundPlan(
    std::uint32_t zoneId,
    float totalDistanceYards,
    std::uint32_t etaMs)
{
    WorldBotResolvedTravelPlan plan;
    plan.mapId = 0;
    plan.zoneId = zoneId;
    plan.totalDistanceYards = totalDistanceYards;
    plan.routeDistanceYards = totalDistanceYards;
    plan.speedYardsPerSecond = etaMs > 0
        ? totalDistanceYards / (static_cast<float>(etaMs) / 1000.0f)
        : 0.0f;
    plan.etaMs = etaMs;

    WorldBotRouteWaypoint waypoint;
    waypoint.mapId = 0;
    waypoint.x = static_cast<float>(zoneId);
    waypoint.y = totalDistanceYards;
    waypoint.z = 0.0f;
    waypoint.cumulativeDistanceYards = totalDistanceYards;
    waypoint.routeKey = "test";
    plan.waypoints.push_back(waypoint);
    return plan;
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

TEST(WorldBotTaxiPlanningTest, ClassifiesSpecialTaxiNodesBeforeMapValidation)
{
    EXPECT_EQ(
        ClassifyWorldBotTaxiNodeForPlanner(9770568u, "Transport, Undercity", false),
        WorldBotTaxiNodeClassification::Transport);
    EXPECT_EQ(
        ClassifyWorldBotTaxiNodeForPlanner(12u, "Quest - New Agamand -> Venomspite", false),
        WorldBotTaxiNodeClassification::Quest);
    EXPECT_EQ(
        ClassifyWorldBotTaxiNodeForPlanner(999999u, "Some Broken Flight Master", false),
        WorldBotTaxiNodeClassification::InvalidMap);
    EXPECT_EQ(
        ClassifyWorldBotTaxiNodeForPlanner(0u, "Crossroads, The Barrens", true),
        WorldBotTaxiNodeClassification::Standard);
    EXPECT_STREQ(
        DescribeWorldBotTaxiNodeClassification(WorldBotTaxiNodeClassification::Transport),
        "transport");
}

TEST(WorldBotTaxiPlanningTest, ResolvesShortestKnownRouteAcrossKnownTaxiGraph)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(1, 12, true, true),
            MakeNode(2, 40, true, true),
            MakeNode(3, 10, true, true),
            MakeNode(4, 44, true, true),
        },
        {
            { 100, 1, 2, 0, 100.0f, 10000u },
            { 101, 2, 3, 0, 50.0f, 5000u },
            { 102, 1, 4, 0, 30.0f, 3000u },
            { 103, 4, 3, 0, 40.0f, 4000u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u, 40u, 10u, 44u };
    auto const route = network.ResolveKnownRoute(1u, 3u, exploredZones, 1);

    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->nodeIds.size(), 3u);
    EXPECT_EQ(route->nodeIds[0], 1u);
    EXPECT_EQ(route->nodeIds[1], 4u);
    EXPECT_EQ(route->nodeIds[2], 3u);
    EXPECT_EQ(route->links.size(), 2u);
    EXPECT_EQ(route->totalEtaMs, 7000u);
    EXPECT_FLOAT_EQ(route->totalDistanceYards, 70.0f);
}

TEST(WorldBotTaxiPlanningTest, KnownRouteFailsWhenRequiredIntermediateZoneIsUnknown)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(1, 12, true, true),
            MakeNode(2, 40, true, true),
            MakeNode(3, 10, true, true),
        },
        {
            { 100, 1, 2, 0, 100.0f, 10000u },
            { 101, 2, 3, 0, 50.0f, 5000u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u, 10u };
    auto const route = network.ResolveKnownRoute(1u, 3u, exploredZones, 1);

    EXPECT_FALSE(route.has_value());
}

TEST(WorldBotTaxiPlanningTest, FindsNearestKnownNodeWithinExploredZones)
{
    WorldBotTaxiNetwork const network({
        MakeNode(1, 12, true, true),
        MakeNode(2, 40, true, true),
        MakeNode(3, 12, true, true),
    });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u };
    auto const node = network.FindNearestKnownNode(0, 2.2f, 12.1f, 0.0f, exploredZones, 1, 100.0f);

    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->nodeId, 3u);
}

TEST(WorldBotTaxiPlanningTest, ResolvesTravelCandidateFromNearestKnownTaxiNodes)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(1, 12, true, true),
            MakeNode(2, 40, true, true),
            MakeNode(3, 10, true, true),
            MakeNode(4, 44, true, true),
        },
        {
            { 100, 1, 4, 0, 30.0f, 3000u },
            { 101, 4, 3, 0, 40.0f, 4000u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u, 10u, 44u };
    auto const candidate = network.ResolveTravelCandidate(
        0,
        1.2f,
        12.0f,
        0.0f,
        0,
        2.8f,
        10.0f,
        0.0f,
        exploredZones,
        1,
        100.0f,
        100.0f);

    ASSERT_TRUE(candidate.has_value());
    EXPECT_EQ(candidate->sourceNode.nodeId, 1u);
    EXPECT_EQ(candidate->destinationNode.nodeId, 3u);
    ASSERT_EQ(candidate->route.nodeIds.size(), 3u);
    EXPECT_EQ(candidate->route.nodeIds[1], 4u);
    EXPECT_FLOAT_EQ(candidate->route.totalDistanceYards, 70.0f);
}

TEST(WorldBotTaxiPlanningTest, ResolvesBestTaxiJourneyWithPartialLandingWhenNeeded)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(1, 12, true, true),
            MakeNode(2, 40, true, true),
            MakeNode(3, 44, true, true),
        },
        {
            { 100, 1, 2, 0, 20.0f, 2000u },
            { 101, 2, 3, 0, 30.0f, 3000u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u, 40u, 44u };
    WorldBotGroundRouteResolver const resolver =
        [](std::uint16_t /*mapId*/,
           std::uint32_t startZoneIdHint,
           std::uint32_t destZoneId,
           float /*startX*/,
           float /*startY*/,
           float /*startZ*/,
           float /*destX*/,
           float /*destY*/,
           float /*destZ*/,
           WorldBotTravelCapabilityTier /*tier*/,
           WorldBotTravelCapabilityConfig const& /*config*/)
            -> std::optional<WorldBotResolvedTravelPlan>
        {
            if (startZoneIdHint == 12u && destZoneId == 12u)
                return MakeGroundPlan(12u, 10.0f, 1000u);
            if (startZoneIdHint == 12u && destZoneId == 40u)
                return MakeGroundPlan(40u, 80.0f, 8000u);
            if (startZoneIdHint == 12u && destZoneId == 44u)
                return MakeGroundPlan(44u, 120.0f, 12000u);
            if (startZoneIdHint == 40u && destZoneId == 10u)
                return MakeGroundPlan(10u, 80.0f, 8000u);
            if (startZoneIdHint == 44u && destZoneId == 10u)
                return MakeGroundPlan(10u, 15.0f, 1500u);
            return std::nullopt;
        };

    auto const journey = ResolveBestTaxiJourney(
        network,
        resolver,
        0,
        12u,
        10u,
        0.0f,
        0.0f,
        0.0f,
        500.0f,
        500.0f,
        0.0f,
        exploredZones,
        1,
        WorldBotTravelCapabilityTier::GroundBasic);

    ASSERT_TRUE(journey.has_value());
    EXPECT_EQ(journey->taxiCandidate.sourceNode.nodeId, 1u);
    EXPECT_EQ(journey->taxiCandidate.destinationNode.nodeId, 3u);
    EXPECT_EQ(journey->sourceGroundPlan.zoneId, 12u);
    EXPECT_EQ(journey->destinationGroundPlan.zoneId, 10u);
    EXPECT_EQ(journey->totalEtaMs, 7500u);
    EXPECT_FLOAT_EQ(journey->totalDistanceYards, 75.0f);
}

TEST(WorldBotTaxiPlanningTest, ResolvesTaxiJourneyWithDirectLocalFlightMasterFallback)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(25, 17, false, true),
            MakeNode(80, 17, false, true),
        },
        {
            { 462, 25, 80, 0, 1545.4f, 48295u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 17u };
    auto const directRoute = network.ResolveKnownRoute(25, 80, exploredZones, 2);
    ASSERT_TRUE(directRoute.has_value());
    ASSERT_FALSE(directRoute->empty());

    WorldBotGroundRouteResolver const resolver =
        [](std::uint16_t /*mapId*/,
           std::uint32_t /*startZoneIdHint*/,
           std::uint32_t /*destZoneId*/,
           float /*startX*/,
           float /*startY*/,
           float /*startZ*/,
           float /*destX*/,
           float /*destY*/,
           float /*destZ*/,
           WorldBotTravelCapabilityTier /*tier*/,
           WorldBotTravelCapabilityConfig const& /*config*/)
            -> std::optional<WorldBotResolvedTravelPlan>
        {
            return std::nullopt;
        };

    auto const journey = ResolveBestTaxiJourney(
        network,
        resolver,
        0,
        17u,
        17u,
        25.0f,
        17.0f,
        0.0f,
        80.0f,
        17.0f,
        0.0f,
        exploredZones,
        2,
        WorldBotTravelCapabilityTier::GroundFast);

    ASSERT_TRUE(journey.has_value());
    EXPECT_EQ(journey->taxiCandidate.sourceNode.nodeId, 25u);
    EXPECT_EQ(journey->taxiCandidate.destinationNode.nodeId, 80u);
    EXPECT_FLOAT_EQ(journey->sourceGroundPlan.totalDistanceYards, 0.0f);
    EXPECT_FLOAT_EQ(journey->destinationGroundPlan.totalDistanceYards, 0.0f);
    EXPECT_EQ(journey->totalEtaMs, 48295u);
}

TEST(WorldBotTaxiPlanningTest, PrefersGroundWhenTaxiIsSlower)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(1, 12, true, true),
            MakeNode(2, 40, true, true),
        },
        {
            { 100, 1, 2, 0, 50.0f, 5000u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u, 40u };
    WorldBotGroundRouteResolver const resolver =
        [](std::uint16_t /*mapId*/,
           std::uint32_t startZoneIdHint,
           std::uint32_t destZoneId,
           float /*startX*/,
           float /*startY*/,
           float /*startZ*/,
           float /*destX*/,
           float /*destY*/,
           float /*destZ*/,
           WorldBotTravelCapabilityTier /*tier*/,
           WorldBotTravelCapabilityConfig const& /*config*/)
            -> std::optional<WorldBotResolvedTravelPlan>
        {
            if (startZoneIdHint == 12u && destZoneId == 10u)
                return MakeGroundPlan(10u, 60.0f, 6000u);
            if (startZoneIdHint == 12u && destZoneId == 12u)
                return MakeGroundPlan(12u, 10.0f, 1000u);
            if (startZoneIdHint == 40u && destZoneId == 10u)
                return MakeGroundPlan(10u, 25.0f, 2500u);
            return std::nullopt;
        };

    auto const option = ResolveBestTravelOption(
        network,
        resolver,
        0,
        12u,
        10u,
        0.0f,
        0.0f,
        0.0f,
        500.0f,
        500.0f,
        0.0f,
        exploredZones,
        1,
        WorldBotTravelCapabilityTier::GroundBasic);

    ASSERT_TRUE(option.has_value());
    EXPECT_EQ(option->mode, WorldBotTravelOptionMode::Ground);
    ASSERT_TRUE(option->groundPlan.has_value());
    EXPECT_EQ(option->groundPlan->etaMs, 6000u);
    ASSERT_TRUE(option->taxiJourney.has_value());
    EXPECT_EQ(option->taxiJourney->totalEtaMs, 8500u);
}

TEST(WorldBotTaxiPlanningTest, ChoosesFullTaxiWhenItBeatsGround)
{
    WorldBotTaxiNetwork const network(
        {
            MakeNode(1, 12, true, true),
            MakeNode(2, 10, true, true),
        },
        {
            { 100, 1, 2, 0, 40.0f, 4000u },
        });

    std::unordered_set<std::uint32_t> const exploredZones = { 12u, 10u };
    WorldBotGroundRouteResolver const resolver =
        [](std::uint16_t /*mapId*/,
           std::uint32_t startZoneIdHint,
           std::uint32_t destZoneId,
           float /*startX*/,
           float /*startY*/,
           float /*startZ*/,
           float /*destX*/,
           float /*destY*/,
           float /*destZ*/,
           WorldBotTravelCapabilityTier /*tier*/,
           WorldBotTravelCapabilityConfig const& /*config*/)
            -> std::optional<WorldBotResolvedTravelPlan>
        {
            if (startZoneIdHint == 12u && destZoneId == 10u)
                return MakeGroundPlan(10u, 120.0f, 12000u);
            if (startZoneIdHint == 12u && destZoneId == 12u)
                return MakeGroundPlan(12u, 8.0f, 800u);
            if (startZoneIdHint == 10u && destZoneId == 10u)
                return MakeGroundPlan(10u, 6.0f, 600u);
            return std::nullopt;
        };

    auto const option = ResolveBestTravelOption(
        network,
        resolver,
        0,
        12u,
        10u,
        0.0f,
        0.0f,
        0.0f,
        500.0f,
        500.0f,
        0.0f,
        exploredZones,
        1,
        WorldBotTravelCapabilityTier::GroundBasic);

    ASSERT_TRUE(option.has_value());
    EXPECT_EQ(option->mode, WorldBotTravelOptionMode::TaxiFull);
    EXPECT_TRUE(option->usesTaxi());
    ASSERT_TRUE(option->taxiJourney.has_value());
    EXPECT_EQ(option->taxiJourney->totalEtaMs, 5400u);
    ASSERT_TRUE(option->groundPlan.has_value());
    EXPECT_EQ(option->groundPlan->etaMs, 12000u);
}

} // namespace service
} // namespace living_world
