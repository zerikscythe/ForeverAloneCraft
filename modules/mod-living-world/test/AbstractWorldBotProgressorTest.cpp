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
    state.stepStartKnown = true;
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
    state.stepStartKnown = true;
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
    state.stepStartKnown = true;
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
    state.stepStartKnown = true;
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

TEST(AbstractWorldBotProgressorTest, TravelOptionResolverUsesTaxiJourneyTimingAndInterpolation)
{
    service::AmbientSession session;
    service::AmbientStep step;
    step.type = service::AmbientStepType::Travel;
    step.mapId = 1;
    step.x = 300.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    session.steps.push_back(step);

    AbstractWorldBotProgressState state;
    state.stepStartKnown = true;
    state.stepStartMapId = 1;
    state.stepStartX = 0.0f;
    state.stepStartY = 0.0f;
    state.stepStartZ = 0.0f;

    service::WorldBotResolvedTaxiJourney journey;
    journey.sourceGroundPlan.mapId = 1;
    journey.sourceGroundPlan.zoneId = 12;
    journey.sourceGroundPlan.totalDistanceYards = 100.0f;
    journey.sourceGroundPlan.speedYardsPerSecond = 10.0f;
    journey.sourceGroundPlan.etaMs = 10000u;
    journey.sourceGroundPlan.waypoints.push_back({1, 100.0f, 0.0f, 0.0f, 100.0f});

    journey.taxiCandidate.sourceNode = {100u, 1u, 12u, 100.0f, 0.0f, 0.0f, true, true, "Goldshire"};
    journey.taxiCandidate.destinationNode = {200u, 1u, 40u, 200.0f, 0.0f, 0.0f, true, true, "Sentinel Hill"};
    journey.taxiCandidate.route.links.push_back({900u, 100u, 200u, 0u, 100.0f, 20000u});
    journey.taxiCandidate.route.totalDistanceYards = 100.0f;
    journey.taxiCandidate.route.totalEtaMs = 20000u;

    journey.destinationGroundPlan.mapId = 1;
    journey.destinationGroundPlan.zoneId = 40;
    journey.destinationGroundPlan.totalDistanceYards = 100.0f;
    journey.destinationGroundPlan.speedYardsPerSecond = 10.0f;
    journey.destinationGroundPlan.etaMs = 10000u;
    journey.destinationGroundPlan.waypoints.push_back({1, 300.0f, 0.0f, 0.0f, 100.0f});

    journey.totalDistanceYards = 300.0f;
    journey.totalEtaMs = 40000u;

    service::WorldBotResolvedTravelOption option;
    option.mode = service::WorldBotTravelOptionMode::TaxiFull;
    option.taxiJourney = journey;
    option.totalDistanceYards = journey.totalDistanceYards;
    option.totalEtaMs = journey.totalEtaMs;

    ASSERT_TRUE(option.taxiJourney.has_value());
    ASSERT_FALSE(option.taxiJourney->empty());

    AbstractWorldBotProgressConfig cfg;
    cfg.travelOptionResolver =
        [option](service::AmbientStep const&,
                 std::uint16_t,
                 float,
                 float,
                 float) -> std::optional<service::WorldBotResolvedTravelOption>
        {
            return option;
        };

    EXPECT_EQ(ComputeAbstractWorldBotStepDurationMs(step, state, cfg), 40000u);

    state.stepElapsedMs = 5000u;
    auto phase = ResolveAbstractWorldBotTravelOptionPhase(option, state);
    ASSERT_TRUE(phase.has_value());
    EXPECT_EQ(phase->kind, AbstractWorldBotTravelPhaseKind::TaxiSourceGround);
    auto sample = ComputeAbstractWorldBotTravelOptionPosition(option, state);
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->x, 50.0f, 0.01f);
    auto pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_NEAR(pos.x, 50.0f, 0.01f);

    state.stepElapsedMs = 20000u;
    phase = ResolveAbstractWorldBotTravelOptionPhase(option, state);
    ASSERT_TRUE(phase.has_value());
    EXPECT_EQ(phase->kind, AbstractWorldBotTravelPhaseKind::TaxiFlight);
    pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_NEAR(pos.x, 150.0f, 0.01f);

    state.stepElapsedMs = 30000u;
    phase = ResolveAbstractWorldBotTravelOptionPhase(option, state);
    ASSERT_TRUE(phase.has_value());
    EXPECT_EQ(phase->kind, AbstractWorldBotTravelPhaseKind::TaxiDestinationGround);
    pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_NEAR(pos.x, 200.0f, 0.01f);

    state.stepElapsedMs = 35000u;
    pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_NEAR(pos.x, 250.0f, 0.01f);
}

TEST(AbstractWorldBotProgressorTest, MapZeroCanBeKnownSameMapTravel)
{
    service::AmbientStep step;
    step.type = service::AmbientStepType::Travel;
    step.mapId = 0;
    step.x = 90.0f;
    step.y = 0.0f;
    step.z = 0.0f;

    AbstractWorldBotProgressState state;
    state.stepStartKnown = true;
    state.stepStartMapId = 0;
    state.stepStartX = 0.0f;
    state.stepStartY = 0.0f;
    state.stepStartZ = 0.0f;

    AbstractWorldBotProgressConfig cfg;
    cfg.travelYardsPerSecond = 9.0f;
    cfg.crossMapTravelMs = 30000u;

    EXPECT_EQ(ComputeAbstractWorldBotStepDurationMs(step, state, cfg), 10000u);
}

TEST(AbstractWorldBotProgressorTest, ScriptedTransitUsesDurationAndInterpolation)
{
    service::AmbientSession session;
    service::AmbientStep step;
    step.type = service::AmbientStepType::Transit;
    step.mapId = 1;
    step.x = 100.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    step.durationSec = 20;
    step.transitType = "boat";
    step.transitSourceLabel = "Menethil Harbor";
    step.transitDestLabel = "Theramore";
    session.steps.push_back(step);

    AbstractWorldBotProgressState state;
    state.stepStartKnown = true;
    state.stepStartMapId = 1;
    state.stepStartX = 0.0f;
    state.stepStartY = 0.0f;
    state.stepStartZ = 0.0f;

    AbstractWorldBotProgressConfig cfg;
    EXPECT_EQ(ComputeAbstractWorldBotStepDurationMs(step, state, cfg), 20000u);

    state.stepElapsedMs = 10000u;
    auto pos = ComputeAbstractWorldBotInterpolatedPosition(session, state, cfg);
    EXPECT_EQ(pos.mapId, 1u);
    EXPECT_FLOAT_EQ(pos.x, 50.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

} // namespace ai
} // namespace living_world
