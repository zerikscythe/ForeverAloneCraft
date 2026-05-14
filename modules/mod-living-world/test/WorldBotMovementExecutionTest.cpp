#include "service/WorldBotMovementExecution.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotMovementExecutionTest, HoldProducesNoMovement)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.movementStyle = model::WorldBotMovementStyle::TurretCaster;
    situation.targetDistance = 24.0f;

    model::WorldBotMovementDecision decision;
    decision.posture = model::WorldBotCombatPosture::Hold;

    WorldBotMovementExecutionPlan const plan = BuildWorldBotMovementExecutionPlan(
        situation, decision, 24.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ(plan.kind, WorldBotMovementPlanKind::None);
}

TEST(WorldBotMovementExecutionTest, MeleeCloseProducesChase)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.movementStyle = model::WorldBotMovementStyle::StickyMelee;
    situation.targetDistance = 8.0f;

    model::WorldBotMovementDecision decision;
    decision.posture = model::WorldBotCombatPosture::Close;

    WorldBotMovementExecutionPlan const plan = BuildWorldBotMovementExecutionPlan(
        situation, decision, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ(plan.kind, WorldBotMovementPlanKind::Chase);
    EXPECT_NEAR(plan.chaseDistance, 1.5f, 0.01f);
}

TEST(WorldBotMovementExecutionTest, RangedCloseUsesStandOffChaseDistance)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.movementStyle = model::WorldBotMovementStyle::MobileRanged;
    situation.targetDistance = 35.0f;

    model::WorldBotMovementDecision decision;
    decision.posture = model::WorldBotCombatPosture::Close;

    WorldBotMovementExecutionPlan const plan = BuildWorldBotMovementExecutionPlan(
        situation, decision, 35.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ(plan.kind, WorldBotMovementPlanKind::Chase);
    EXPECT_NEAR(plan.chaseDistance, 24.0f, 0.01f);
}

TEST(WorldBotMovementExecutionTest, KiteProducesMovePointAwayFromTarget)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.movementStyle = model::WorldBotMovementStyle::TurretCaster;
    situation.targetDistance = 6.0f;

    model::WorldBotMovementDecision decision;
    decision.posture = model::WorldBotCombatPosture::Kite;

    WorldBotMovementExecutionPlan const plan = BuildWorldBotMovementExecutionPlan(
        situation, decision, 6.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ(plan.kind, WorldBotMovementPlanKind::MovePoint);
    EXPECT_GT(plan.pointX, 6.0f);
}

TEST(WorldBotMovementExecutionTest, RetreatProducesLongerRangeMovePoint)
{
    model::WorldBotCombatSituation situation;
    situation.hasVictim = true;
    situation.movementStyle = model::WorldBotMovementStyle::BacklineHealer;
    situation.targetDistance = 10.0f;

    model::WorldBotMovementDecision decision;
    decision.posture = model::WorldBotCombatPosture::Retreat;

    WorldBotMovementExecutionPlan const plan = BuildWorldBotMovementExecutionPlan(
        situation, decision, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ(plan.kind, WorldBotMovementPlanKind::MovePoint);
    EXPECT_GT(plan.pointX, 20.0f);
}

} // namespace service
} // namespace living_world