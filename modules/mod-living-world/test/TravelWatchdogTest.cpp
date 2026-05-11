#include "ai/TravelWatchdog.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace ai
{

TEST(TravelWatchdogTest, FirstObservationDoesNotImmediatelyFlagStuck)
{
    TravelWatchdogState state;
    EXPECT_EQ(UpdateTravelWatchdog(state, 10.0f, 20.0f, 30.0f, 500u), TravelWatchdogSignal::None);
}

TEST(TravelWatchdogTest, AccumulatedProgressResetsStagnation)
{
    TravelWatchdogState state;
    UpdateTravelWatchdog(state, 0.0f, 0.0f, 0.0f, 500u);
    EXPECT_EQ(UpdateTravelWatchdog(state, 0.5f, 0.0f, 0.0f, 500u), TravelWatchdogSignal::None);
    EXPECT_EQ(UpdateTravelWatchdog(state, 1.6f, 0.0f, 0.0f, 500u), TravelWatchdogSignal::None);
    EXPECT_EQ(state.stagnantMs, 0u);
}

TEST(TravelWatchdogTest, FlagsStuckWhenNoMeaningfulProgressForThreshold)
{
    TravelWatchdogState state;
    TravelWatchdogConfig cfg;
    cfg.stagnantLimitMs = 1500;
    cfg.timeoutMs = 30000;
    cfg.progressThreshold = 1.5f;

    EXPECT_EQ(UpdateTravelWatchdog(state, 0.0f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::None);
    EXPECT_EQ(UpdateTravelWatchdog(state, 0.1f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::None);
    EXPECT_EQ(UpdateTravelWatchdog(state, 0.2f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::None);
    EXPECT_EQ(UpdateTravelWatchdog(state, 0.3f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::Stuck);
}

TEST(TravelWatchdogTest, FlagsTimeoutAtConfiguredLimit)
{
    TravelWatchdogState state;
    TravelWatchdogConfig cfg;
    cfg.stagnantLimitMs = 30000;
    cfg.timeoutMs = 1000;
    cfg.progressThreshold = 1.5f;

    EXPECT_EQ(UpdateTravelWatchdog(state, 0.0f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::None);
    EXPECT_EQ(UpdateTravelWatchdog(state, 2.0f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::Timeout);
}

TEST(TravelWatchdogTest, TimeoutWinsIfBothThresholdsAreReached)
{
    TravelWatchdogState state;
    TravelWatchdogConfig cfg;
    cfg.stagnantLimitMs = 1000;
    cfg.timeoutMs = 1000;
    cfg.progressThreshold = 1.5f;

    EXPECT_EQ(UpdateTravelWatchdog(state, 0.0f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::None);
    EXPECT_EQ(UpdateTravelWatchdog(state, 0.0f, 0.0f, 0.0f, 500u, cfg), TravelWatchdogSignal::Timeout);
}

} // namespace ai
} // namespace living_world