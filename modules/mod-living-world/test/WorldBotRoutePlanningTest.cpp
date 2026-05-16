#include "service/WorldBotRoutePlanning.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace living_world
{
namespace service
{

namespace
{

std::filesystem::path WriteRouteFixture(std::string const& payload)
{
    std::filesystem::path const dir =
        std::filesystem::temp_directory_path() / "lw_route_planner_test";
    std::filesystem::create_directories(dir);
    std::filesystem::path const file = dir / "map_000__zone_12__demo__routes.json";
    std::ofstream output(file, std::ios::out | std::ios::trunc | std::ios::binary);
    output << payload;
    output.close();
    return dir;
}

std::filesystem::path WriteRouteFixture(
    std::string const& filename,
    std::string const& payload)
{
    std::filesystem::path const dir =
        std::filesystem::temp_directory_path() / "lw_route_planner_test_multi";
    std::filesystem::create_directories(dir);
    std::filesystem::path const file = dir / filename;
    std::ofstream output(file, std::ios::out | std::ios::trunc | std::ios::binary);
    output << payload;
    output.close();
    return dir;
}

} // namespace

TEST(WorldBotRoutePlanningTest, ResolvesSameZoneBranchAwarePlanFromExportedJson)
{
    std::filesystem::path const root = WriteRouteFixture(R"json(
{
  "route_group_key": "Demo",
  "zone_name": "Demo Zone",
  "zone_id": 12,
  "map_id": 0,
  "sample_spacing_yards": 25.0,
  "path_count": 2,
  "movement_point_count": 5,
  "paths": [
    {
      "path_index": 0,
      "path_key": "Demo_01",
      "start_connection": null,
      "end_connection": null,
      "anchors": [
        { "world_x": 0.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 50.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 100.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 0.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 50.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 50.0 },
        { "point_index": 2, "map_id": 0, "world_x": 100.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 }
      ]
    },
    {
      "path_index": 1,
      "path_key": "Demo_02",
      "start_connection": { "path_index": 0, "anchor_index": 1 },
      "end_connection": null,
      "anchors": [
        { "world_x": 50.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 50.0, "world_y": 50.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 50.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 50.0, "world_y": 50.0, "world_z": 0.0, "distance_from_start_yards": 50.0 }
      ]
    }
  ]
}
)json");

    WorldBotRoutePlanner planner(root);
    auto const plan = planner.ResolveSameZoneTravelPlan(
        0,
        12,
        2.0f,
        0.0f,
        0.0f,
        50.0f,
        52.0f,
        0.0f,
        WorldBotTravelCapabilityTier::Foot);

    ASSERT_TRUE(plan.has_value());
    EXPECT_GT(plan->totalDistanceYards, 100.0f);
    EXPECT_LT(plan->totalDistanceYards, 110.0f);
    EXPECT_FALSE(plan->waypoints.empty());
    EXPECT_NE(std::find_if(
        plan->waypoints.begin(),
        plan->waypoints.end(),
        [](WorldBotRouteWaypoint const& waypoint)
        {
            return waypoint.routeKey == "Demo_02";
        }),
        plan->waypoints.end());

    WorldBotTravelPositionSample const sample = SampleWorldBotTravelPlanPosition(
        *plan,
        2.0f,
        0.0f,
        0.0f,
        55.0f);
    EXPECT_NEAR(sample.x, 50.0f, 5.0f);
    EXPECT_NEAR(sample.y, 5.0f, 5.0f);
}

TEST(WorldBotRoutePlanningTest, ResolvesCapabilityTierByLevel)
{
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(12, false),
        WorldBotTravelCapabilityTier::Foot);
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(20, false),
        WorldBotTravelCapabilityTier::GroundBasic);
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(40, false),
        WorldBotTravelCapabilityTier::GroundFast);
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(60, true),
        WorldBotTravelCapabilityTier::FlightBasic);
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(70, true),
        WorldBotTravelCapabilityTier::FlightFast);
}

TEST(WorldBotRoutePlanningTest, ResolvesCapabilityTierByCustomPolicy)
{
    WorldBotTravelCapabilityPolicy policy;
    policy.groundBasicMinLevel = 10;
    policy.groundFastMinLevel = 30;
    policy.flightBasicMinLevel = 50;
    policy.flightFastMinLevel = 65;

    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(12, false, policy),
        WorldBotTravelCapabilityTier::GroundBasic);
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(35, false, policy),
        WorldBotTravelCapabilityTier::GroundFast);
    EXPECT_EQ(
        ResolveWorldBotTravelCapabilityTierForLevel(55, true, policy),
        WorldBotTravelCapabilityTier::FlightBasic);
}

TEST(WorldBotRoutePlanningTest, FasterCapabilityTiersReduceEtaForSameRoute)
{
    std::filesystem::path const root = WriteRouteFixture(R"json(
{
  "route_group_key": "Demo",
  "zone_name": "Demo Zone",
  "zone_id": 12,
  "map_id": 0,
  "sample_spacing_yards": 25.0,
  "path_count": 1,
  "movement_point_count": 3,
  "paths": [
    {
      "path_index": 0,
      "path_key": "Demo_01",
      "start_connection": null,
      "end_connection": null,
      "anchors": [
        { "world_x": 0.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 100.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 200.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 0.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 100.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 },
        { "point_index": 2, "map_id": 0, "world_x": 200.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 200.0 }
      ]
    }
  ]
}
)json");

    WorldBotRoutePlanner planner(root);
    auto const footPlan = planner.ResolveSameZoneTravelPlan(
        0,
        12,
        0.0f,
        0.0f,
        0.0f,
        200.0f,
        0.0f,
        0.0f,
        WorldBotTravelCapabilityTier::Foot);
    auto const mountPlan = planner.ResolveSameZoneTravelPlan(
        0,
        12,
        0.0f,
        0.0f,
        0.0f,
        200.0f,
        0.0f,
        0.0f,
        WorldBotTravelCapabilityTier::GroundFast);

    ASSERT_TRUE(footPlan.has_value());
    ASSERT_TRUE(mountPlan.has_value());
    EXPECT_FLOAT_EQ(footPlan->totalDistanceYards, mountPlan->totalDistanceYards);
    EXPECT_GT(footPlan->etaMs, mountPlan->etaMs);
    EXPECT_GT(mountPlan->speedYardsPerSecond, footPlan->speedYardsPerSecond);
}

TEST(WorldBotRoutePlanningTest, AutomaticZoneTransitionPrefersForwardSeamOverBackwardReversal)
{
    std::filesystem::path const root = WriteRouteFixture(
        "map_000__zone_12__from_zone__routes.json",
        R"json(
{
  "route_group_key": "FromZone",
  "zone_name": "From Zone",
  "zone_id": 12,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "From_01",
      "anchors": [
        { "world_x": 0.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 100.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 200.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 0.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 100.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 },
        { "point_index": 2, "map_id": 0, "world_x": 200.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 200.0 }
      ]
    }
  ]
}
)json");

    WriteRouteFixture(
        "map_000__zone_40__to_zone__routes.json",
        R"json(
{
  "route_group_key": "ToZone",
  "zone_name": "To Zone",
  "zone_id": 40,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "To_Forward",
      "anchors": [
        { "world_x": 220.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 320.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 220.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 320.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 }
      ]
    },
    {
      "path_index": 1,
      "path_key": "To_Backward",
      "anchors": [
        { "world_x": 185.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 80.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 185.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 80.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 105.0 }
      ]
    }
  ]
}
)json");

    WorldBotRoutePlanner planner(root);
    auto const transition = planner.ResolveAutomaticZoneTransition(0, 12, 40, 60.0f);

    ASSERT_TRUE(transition.has_value());
    EXPECT_EQ(transition->fromWaypoint.routeKey, "From_01");
    EXPECT_EQ(transition->toWaypoint.routeKey, "To_Forward");
    EXPECT_GT(transition->sourceHeadingAlignment, 0.9f);
    EXPECT_GT(transition->targetHeadingAlignment, 0.9f);
    EXPECT_LT(transition->seamDistanceYards, 30.0f);
}

TEST(WorldBotRoutePlanningTest, ResolvesCrossZoneTravelPlanThroughForwardSeam)
{
    std::filesystem::path const root = WriteRouteFixture(
        "map_000__zone_12__from_zone__routes.json",
        R"json(
{
  "route_group_key": "FromZone",
  "zone_name": "From Zone",
  "zone_id": 12,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "From_01",
      "anchors": [
        { "world_x": 0.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 100.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 200.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 0.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 100.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 },
        { "point_index": 2, "map_id": 0, "world_x": 200.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 200.0 }
      ]
    }
  ]
}
)json");

    WriteRouteFixture(
        "map_000__zone_40__to_zone__routes.json",
        R"json(
{
  "route_group_key": "ToZone",
  "zone_name": "To Zone",
  "zone_id": 40,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "To_Forward",
      "anchors": [
        { "world_x": 220.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 320.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 220.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 320.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 }
      ]
    }
  ]
}
)json");

    WorldBotRoutePlanner planner(root);
    auto const plan = planner.ResolveTravelPlan(
        0,
        12,
        40,
        0.0f,
        0.0f,
        0.0f,
        320.0f,
        0.0f,
        0.0f,
        WorldBotTravelCapabilityTier::Foot);

    ASSERT_TRUE(plan.has_value());
    EXPECT_GT(plan->totalDistanceYards, 300.0f);
    EXPECT_LT(plan->totalDistanceYards, 330.0f);
    ASSERT_FALSE(plan->waypoints.empty());
    EXPECT_EQ(plan->waypoints.front().routeKey, "From_01");
    EXPECT_EQ(plan->waypoints.back().routeKey, "To_Forward");
    EXPECT_NE(
        std::find_if(
            plan->waypoints.begin(),
            plan->waypoints.end(),
            [](WorldBotRouteWaypoint const& waypoint)
            {
                return waypoint.routeKey == "To_Forward" && waypoint.x <= 220.1f;
            }),
        plan->waypoints.end());
}

TEST(WorldBotRoutePlanningTest, ExplicitZoneConnectorOverridesAutomaticSeamGuess)
{
    std::filesystem::path const root = WriteRouteFixture(
        "map_000__zone_12__from_zone__routes.json",
        R"json(
{
  "route_group_key": "FromZone",
  "zone_name": "From Zone",
  "zone_id": 12,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "From_01",
      "anchors": [
        { "world_x": 0.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 100.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 200.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 0.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 100.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 },
        { "point_index": 2, "map_id": 0, "world_x": 200.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 200.0 }
      ]
    }
  ]
}
)json");

    WriteRouteFixture(
        "map_000__zone_40__to_zone__routes.json",
        R"json(
{
  "route_group_key": "ToZone",
  "zone_name": "To Zone",
  "zone_id": 40,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "To_Forward",
      "anchors": [
        { "world_x": 220.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 320.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 220.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 320.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 }
      ]
    }
  ]
}
)json");

    WriteRouteFixture(
        "map_000__connectors.json",
        R"json(
{
  "map_id": 0,
  "connectors": [
    {
      "connector_key": "Authored_12_40",
      "from_zone_id": 12,
      "to_zone_id": 40,
      "bidirectional": true,
      "from": { "world_x": 190.0, "world_y": 0.0, "world_z": 0.0 },
      "to": { "world_x": 260.0, "world_y": 0.0, "world_z": 0.0 }
    }
  ]
}
)json");

    WorldBotRoutePlanner planner(root);

    auto const explicitTransition = planner.ResolveExplicitZoneTransition(0, 12, 40);
    ASSERT_TRUE(explicitTransition.has_value());
    EXPECT_TRUE(explicitTransition->explicitConnector);
    EXPECT_EQ(explicitTransition->connectorKey, "Authored_12_40");
    EXPECT_NEAR(explicitTransition->fromWaypoint.x, 190.0f, 0.01f);
    EXPECT_NEAR(explicitTransition->toWaypoint.x, 260.0f, 0.01f);

    auto const plan = planner.ResolveTravelPlan(
        0,
        12,
        40,
        0.0f,
        0.0f,
        0.0f,
        320.0f,
        0.0f,
        0.0f,
        WorldBotTravelCapabilityTier::Foot);

    ASSERT_TRUE(plan.has_value());
    EXPECT_NE(
        std::find_if(
            plan->waypoints.begin(),
            plan->waypoints.end(),
            [](WorldBotRouteWaypoint const& waypoint)
            {
                return std::fabs(waypoint.x - 260.0f) < 0.01f;
            }),
        plan->waypoints.end());
}

TEST(WorldBotRoutePlanningTest, ExplicitConnectorCanJoinForwardIntoDestinationRoute)
{
    std::filesystem::path const root = WriteRouteFixture(
        "map_000__zone_12__from_zone__routes.json",
        R"json(
{
  "route_group_key": "FromZone",
  "zone_name": "From Zone",
  "zone_id": 12,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "From_01",
      "anchors": [
        { "world_x": 0.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 100.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 200.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 0.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 100.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 100.0 },
        { "point_index": 2, "map_id": 0, "world_x": 200.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 200.0 }
      ]
    }
  ]
}
)json");

    WriteRouteFixture(
        "map_000__zone_40__to_zone__routes.json",
        R"json(
{
  "route_group_key": "ToZone",
  "zone_name": "To Zone",
  "zone_id": 40,
  "map_id": 0,
  "paths": [
    {
      "path_index": 0,
      "path_key": "To_Forward",
      "anchors": [
        { "world_x": 205.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 220.0, "world_y": 0.0, "world_z": 0.0 },
        { "world_x": 320.0, "world_y": 0.0, "world_z": 0.0 }
      ],
      "movement_points": [
        { "point_index": 0, "map_id": 0, "world_x": 205.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 0.0 },
        { "point_index": 1, "map_id": 0, "world_x": 220.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 15.0 },
        { "point_index": 2, "map_id": 0, "world_x": 320.0, "world_y": 0.0, "world_z": 0.0, "distance_from_start_yards": 115.0 }
      ]
    }
  ]
}
)json");

    WriteRouteFixture(
        "map_000__connectors.json",
        R"json(
{
  "map_id": 0,
  "connectors": [
    {
      "connector_key": "Authored_12_40_ForwardJoin",
      "from_zone_id": 12,
      "to_zone_id": 40,
      "bidirectional": true,
      "from": { "world_x": 200.0, "world_y": 0.0, "world_z": 0.0 },
      "to": { "world_x": 210.0, "world_y": 0.0, "world_z": 0.0 }
    }
  ]
}
)json");

    WorldBotRoutePlanner planner(root);
    auto const plan = planner.ResolveTravelPlan(
        0,
        12,
        40,
        0.0f,
        0.0f,
        0.0f,
        320.0f,
        0.0f,
        0.0f,
        WorldBotTravelCapabilityTier::Foot);

    ASSERT_TRUE(plan.has_value());
    auto connectorItr = std::find_if(
        plan->waypoints.begin(),
        plan->waypoints.end(),
        [](WorldBotRouteWaypoint const& waypoint)
        {
            return std::fabs(waypoint.x - 210.0f) < 0.01f;
        });
    ASSERT_NE(connectorItr, plan->waypoints.end());

    auto nextItr = std::next(connectorItr);
    ASSERT_NE(nextItr, plan->waypoints.end());
    EXPECT_NEAR(nextItr->x, 220.0f, 0.01f);
    EXPECT_NEAR(nextItr->y, 0.0f, 0.01f);
}

} // namespace service
} // namespace living_world
