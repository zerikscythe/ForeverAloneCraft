#include "service/SharedHazardEvaluation.h"

#include "gtest/gtest.h"

#include <chrono>

namespace living_world
{
namespace service
{
namespace
{

Position MakePosition(float x, float y, float z = 0.0f)
{
    Position position;
    position.Relocate(x, y, z);
    return position;
}

} // namespace

TEST(SharedHazardEvaluationTest, RepeatedDamageWhileStationaryTriggersAfterConfiguredTicks)
{
    using namespace std::chrono_literals;

    model::HazardTuning tuning;
    SharedHazardEvaluationState state;
    auto const start = std::chrono::steady_clock::now();

    SharedHazardEvaluationResult const first = EvaluateSharedHazard(
        state,
        start,
        100.0f,
        MakePosition(0.0f, 0.0f),
        false,
        0,
        0.0f,
        tuning);
    EXPECT_FALSE(first.dangerDetectedNow);
    EXPECT_FALSE(first.repeatedDamageTriggered);

    SharedHazardEvaluationResult const second = EvaluateSharedHazard(
        state,
        start + 500ms,
        97.0f,
        MakePosition(0.5f, 0.5f),
        false,
        0,
        0.0f,
        tuning);
    EXPECT_FALSE(second.dangerDetectedNow);
    EXPECT_EQ(second.consecutiveDamageTicks, 1);

    SharedHazardEvaluationResult const third = EvaluateSharedHazard(
        state,
        start + 1000ms,
        94.0f,
        MakePosition(0.75f, 0.75f),
        false,
        0,
        0.0f,
        tuning);
    EXPECT_TRUE(third.dangerDetectedNow);
    EXPECT_TRUE(third.repeatedDamageTriggered);
    EXPECT_TRUE(third.commitWindowActive);
    EXPECT_EQ(third.consecutiveDamageTicks, 2);
    EXPECT_GT(third.severity, 0.0f);
}

TEST(SharedHazardEvaluationTest, CommitWindowPersistsBrieflyAfterImmediateDangerClears)
{
    using namespace std::chrono_literals;

    model::HazardTuning tuning;
    SharedHazardEvaluationState state;
    auto const start = std::chrono::steady_clock::now();

    SharedHazardEvaluationResult const danger = EvaluateSharedHazard(
        state,
        start,
        100.0f,
        MakePosition(10.0f, 20.0f),
        true,
        12345,
        1.0f,
        tuning);
    EXPECT_TRUE(danger.dangerDetectedNow);
    EXPECT_TRUE(danger.commitWindowActive);
    EXPECT_EQ(danger.hazardSpellId, 12345u);

    SharedHazardEvaluationResult const persisted = EvaluateSharedHazard(
        state,
        start + 1000ms,
        100.0f,
        MakePosition(10.0f, 20.0f),
        false,
        0,
        0.0f,
        tuning);
    EXPECT_FALSE(persisted.dangerDetectedNow);
    EXPECT_TRUE(persisted.commitWindowActive);
    EXPECT_FLOAT_EQ(persisted.severity, 1.0f);
    EXPECT_EQ(persisted.hazardSpellId, 12345u);

    SharedHazardEvaluationResult const expired = EvaluateSharedHazard(
        state,
        start + 2501ms,
        100.0f,
        MakePosition(10.0f, 20.0f),
        false,
        0,
        0.0f,
        tuning);
    EXPECT_FALSE(expired.dangerDetectedNow);
    EXPECT_FALSE(expired.commitWindowActive);
    EXPECT_FLOAT_EQ(expired.severity, 0.0f);
    EXPECT_EQ(expired.hazardSpellId, 0u);
}

} // namespace service
} // namespace living_world