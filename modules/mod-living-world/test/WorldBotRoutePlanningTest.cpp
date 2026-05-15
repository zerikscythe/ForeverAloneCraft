#include "service/WorldBotRoutePlanning.h"

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>

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

} // namespace service
} // namespace living_world
