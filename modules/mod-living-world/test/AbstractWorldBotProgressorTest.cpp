#include "ai/AbstractWorldBotProgressor.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace ai
{

TEST(AbstractWorldBotProgressorTest, TimedActivityAdvancesAfterDuration)
{
    service::AmbientSession session;
    service::AmbientStep step;
    step.type = service::AmbientStepType::Idle;
    step.mapId = 0;
    step.x = 1.0f;
    step.y = 2.0f;
    step.z = 3.0f;
    step.durationSec = 10;
    session.steps.push_back(step);

    AbstractWorldBotProgressState state;
    state.stepStartMapId = 0;
    state.stepStartX = 1.0f;
    state.stepStartY = 2.0f;
    state.stepStartZ = 3.0f;

    auto outcome = AdvanceAbstractWorldBotProgress(session, state, 5000u);
    EXPECT_FALSE(outcome.sessionComplete);
    EXPECT_EQ(state.currentStep, 0u);
    EXPECT_EQ(state.stepElapsedMs, 5000u);

    outcome = AdvanceAbstractWorldBotProgress(session, state, 5000u);
    EXPECT_TRUE(outcome.sessionComplete);
    EXPECT_EQ(state.currentStep, 1u);
    EXPECT_EQ(state.stepElapsedMs, 0u);
}

TEST(AbstractWorldBotProgressorTest, SameMapTravelUsesDistanceAndInterpolates)
{
    service::AmbientSession session;
    service::AmbientStep step;
    step.type = service::AmbientStepType::Travel;
    step.mapId = 1;
    step.x = 90.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    session.steps.push_back(step);

    AbstractWorldBotProgressState state;
    state.stepStartMapId = 1;
    state.stepStartX = 0.0f;
    state.stepStartY = 0.0f;
    state.stepStartZ = 0.0f;

    AbstractWorldBotProgressConfig cfg;
    cfg.travelYardsPerSecond = 9.0f;

    EXPECT_EQ(ComputeAbstractWorldBotStepDurationMs(step, state, cfg), 10000u);

    auto outcome = AdvanceAbstractWorldBotProgress(session, state, 5000u, cfg);
    EXPECT_FALSE(outcome.sessionComplete);
    EXPECT_EQ(state.currentStep, 0u);

    auto pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_EQ(pos.mapId, 1u);
    EXPECT_FLOAT_EQ(pos.x, 45.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

TEST(AbstractWorldBotProgressorTest, CrossMapTravelFallsBackToConfiguredDuration)
{
    service::AmbientSession session;
    service::AmbientStep step;
    step.type = service::AmbientStepType::Travel;
    step.mapId = 530;
    step.x = 100.0f;
    step.y = 200.0f;
    step.z = 50.0f;
    session.steps.push_back(step);

    AbstractWorldBotProgressState state;
    state.stepStartMapId = 0;
    state.stepStartX = 0.0f;
    state.stepStartY = 0.0f;
    state.stepStartZ = 0.0f;

    AbstractWorldBotProgressConfig cfg;
    cfg.crossMapTravelMs = 30000u;

    EXPECT_EQ(ComputeAbstractWorldBotStepDurationMs(step, state, cfg), 30000u);

    auto pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_EQ(pos.mapId, 530u);
    EXPECT_FLOAT_EQ(pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.y, 200.0f);
    EXPECT_FLOAT_EQ(pos.z, 50.0f);
}

TEST(AbstractWorldBotProgressorTest, RoutePlanResolverOverridesDirectTravelTimingAndInterpolation)
{
    service::AmbientSession session;
    service::AmbientStep step;
    step.type = service::AmbientStepType::Travel;
    step.mapId = 1;
    step.x = 100.0f;
    step.y = 50.0f;
    step.z = 0.0f;
    session.steps.push_back(step);

    AbstractWorldBotProgressState state;
    state.stepStartMapId = 1;
    state.stepStartX = 0.0f;
    state.stepStartY = 0.0f;
    state.stepStartZ = 0.0f;

    AbstractWorldBotProgressConfig cfg;
    cfg.routePlanResolver =
        [](service::AmbientStep const&,
           std::uint16_t,
           float,
           float,
           float) -> std::optional<service::WorldBotResolvedTravelPlan>
        {
            service::WorldBotResolvedTravelPlan plan;
            plan.mapId = 1;
            plan.zoneId = 12;
            plan.totalDistanceYards = 150.0f;
            plan.speedYardsPerSecond = 10.0f;
            plan.etaMs = 15000u;

            service::WorldBotRouteWaypoint first;
            first.mapId = 1;
            first.x = 50.0f;
            first.y = 0.0f;
            first.z = 0.0f;
            first.cumulativeDistanceYards = 50.0f;
            plan.waypoints.push_back(first);

            service::WorldBotRouteWaypoint second;
            second.mapId = 1;
            second.x = 50.0f;
            second.y = 50.0f;
            second.z = 0.0f;
            second.cumulativeDistanceYards = 100.0f;
            plan.waypoints.push_back(second);

            service::WorldBotRouteWaypoint third;
            third.mapId = 1;
            third.x = 100.0f;
            third.y = 50.0f;
            third.z = 0.0f;
            third.cumulativeDistanceYards = 150.0f;
            plan.waypoints.push_back(third);

            return plan;
        };

    EXPECT_EQ(ComputeAbstractWorldBotStepDurationMs(step, state, cfg), 15000u);

    auto outcome = AdvanceAbstractWorldBotProgress(session, state, 7500u, cfg);
    EXPECT_FALSE(outcome.sessionComplete);
    EXPECT_EQ(state.currentStep, 0u);

    auto pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_EQ(pos.mapId, 1u);
    EXPECT_NEAR(pos.x, 50.0f, 0.01f);
    EXPECT_NEAR(pos.y, 25.0f, 0.01f);
    EXPECT_NEAR(pos.z, 0.0f, 0.01f);
}

} // namespace ai
} // namespace living_world
