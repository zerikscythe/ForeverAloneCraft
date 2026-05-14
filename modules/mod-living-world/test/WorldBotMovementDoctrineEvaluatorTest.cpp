#include "service/WorldBotMovementDoctrineEvaluator.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotMovementDoctrineEvaluatorTest, HazardOverrideTakesPrecedenceOverAllOtherPostures)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.targetDistance = 2.0f;
    situation.healthPct = 90.0f;
    situation.isTankStyle = true;
    situation.hazard.active = true;

    model::WorldBotMovementDecision const decision = EvaluateWorldBotMovementDoctrine(situation);
    EXPECT_EQ(decision.source, model::WorldBotMovementDecisionSource::HazardOverride);
    EXPECT_EQ(decision.posture, model::WorldBotCombatPosture::HazardEscape);
    EXPECT_FALSE(decision.allowHardCasts);
}

TEST(WorldBotMovementDoctrineEvaluatorTest, RangedStyleKitesWhenTargetTooClose)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.targetDistance = 6.0f;
    situation.healthPct = 80.0f;
    situation.isRangedStyle = true;

    model::WorldBotMovementDecision const decision = EvaluateWorldBotMovementDoctrine(situation);
    EXPECT_EQ(decision.source, model::WorldBotMovementDecisionSource::PostureDoctrine);
    EXPECT_EQ(decision.posture, model::WorldBotCombatPosture::Kite);
    EXPECT_FALSE(decision.allowHardCasts);
}

TEST(WorldBotMovementDoctrineEvaluatorTest, RangedStyleClosesWhenTargetTooFar)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.targetDistance = 35.0f;
    situation.healthPct = 80.0f;
    situation.isRangedStyle = true;

    model::WorldBotMovementDecision const decision = EvaluateWorldBotMovementDoctrine(situation);
    EXPECT_EQ(decision.posture, model::WorldBotCombatPosture::Close);
    EXPECT_FALSE(decision.allowHardCasts);
}

TEST(WorldBotMovementDoctrineEvaluatorTest, HealerRetreatsWhenLowHealth)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.targetDistance = 20.0f;
    situation.healthPct = 20.0f;
    situation.isHealerStyle = true;

    model::WorldBotMovementDecision const decision = EvaluateWorldBotMovementDoctrine(situation);
    EXPECT_EQ(decision.posture, model::WorldBotCombatPosture::Retreat);
    EXPECT_FALSE(decision.allowHardCasts);
}

TEST(WorldBotMovementDoctrineEvaluatorTest, TankClosesThenHolds)
{
    model::WorldBotCombatSituation approach;
    approach.hasVictim = true;
    approach.targetDistance = 8.0f;
    approach.isTankStyle = true;

    model::WorldBotMovementDecision const closeDecision = EvaluateWorldBotMovementDoctrine(approach);
    EXPECT_EQ(closeDecision.posture, model::WorldBotCombatPosture::Close);

    model::WorldBotCombatSituation hold = approach;
    hold.targetDistance = 3.0f;
    model::WorldBotMovementDecision const holdDecision = EvaluateWorldBotMovementDoctrine(hold);
    EXPECT_EQ(holdDecision.posture, model::WorldBotCombatPosture::Hold);
}

} // namespace service
} // namespace living_world