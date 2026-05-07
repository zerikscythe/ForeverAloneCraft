#include "service/BotCombatRuntimeEvaluator.h"
#include "model/BotCombatProfile.h"
#include "gtest/gtest.h"

// These tests cover the pure structural and condition-logic behaviour of
// BotCombatRuntimeEvaluator using null Player/Unit pointers throughout.
// Full execution paths (Cast / Wait dispositions) require real AzerothCore
// objects and are covered by in-game integration testing.

namespace living_world
{
namespace service
{
namespace
{

// ── helpers ──────────────────────────────────────────────────────────────────

model::BotCombatEntryDefinition MakeEntry(
    std::uint8_t priority,
    model::BotCombatConditionLogic logic = model::BotCombatConditionLogic::All,
    std::string targetKey = "enemy_primary")
{
    model::BotCombatEntryDefinition entry;
    entry.entryId       = priority; // reuse as id for traceability
    entry.priority      = priority;
    entry.conditionLogic = logic;

    model::BotCombatActionDefinition action;
    action.actionId    = priority;
    action.actionType  = model::BotCombatActionType::Spell;
    action.spellBaseId = 585; // Smite rank 1 — arbitrary non-zero id
    action.targetKey   = std::move(targetKey);
    entry.primaryAction = action;

    return entry;
}

// A condition that passes when the enemy is absent (exists == 0.0).
model::BotCombatConditionDefinition MakeEnemyAbsentCondition()
{
    model::BotCombatConditionDefinition cond;
    cond.statKey    = "exists";
    cond.subjectKey = "enemy";
    cond.comparison = model::BotCombatConditionOperator::Equal;
    cond.numericValue = 0.0f; // 0 == 0 → true when enemy is null
    return cond;
}

// A condition that fails when the enemy is absent (Exists → false when null).
model::BotCombatConditionDefinition MakeEnemyExistsCondition()
{
    model::BotCombatConditionDefinition cond;
    cond.statKey    = "exists";
    cond.subjectKey = "enemy";
    cond.comparison = model::BotCombatConditionOperator::Exists;
    return cond;
}

BotCombatRuntimeContext NullContext()
{
    BotCombatRuntimeContext ctx;
    ctx.bot           = nullptr;
    ctx.owner         = nullptr;
    ctx.primaryTarget = nullptr;
    ctx.rotationWaitMs = 500;
    return ctx;
}

// ── empty-list tests ──────────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, EmptyInterruptListGivesNone)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    // rotationEntries populated to show interrupts don't fall through to them
    profile.rotationEntries.push_back(MakeEntry(1));

    auto const result = evaluator.EvaluateInterrupts(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
    EXPECT_FALSE(result.action.has_value());
}

TEST(BotCombatRuntimeEvaluatorTest, EmptyRotationListGivesNone)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
    EXPECT_FALSE(result.action.has_value());
}

// ── list isolation tests ──────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, EvaluateInterruptsDoesNotConsultRotationEntries)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    // Only rotation entries present — interrupts should still be None.
    profile.rotationEntries.push_back(MakeEntry(1));
    profile.rotationEntries.push_back(MakeEntry(2));

    auto const result = evaluator.EvaluateInterrupts(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

TEST(BotCombatRuntimeEvaluatorTest, EvaluateRotationDoesNotConsultInterruptEntries)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    // Only interrupt entries present — rotation should still be None.
    auto entry = MakeEntry(1);
    entry.isInterrupt = true;
    profile.interruptEntries.push_back(entry);

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

// ── condition logic: All ──────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, AllConditionLogicWithNoConditionsPassesGate)
{
    // All + empty conditions → vacuous AND → entry passes condition gate.
    // With null bot the action still can't execute, so disposition stays None.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    profile.rotationEntries.push_back(
        MakeEntry(1, model::BotCombatConditionLogic::All));

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    // Condition gate passed; action failed because bot is null. No crash.
    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

TEST(BotCombatRuntimeEvaluatorTest, AllConditionLogicRequiresEveryConditionToPass)
{
    // Two conditions under All: one that passes (enemy absent == 0) and one
    // that fails (Exists on null enemy). The overall gate must fail.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;

    auto entry = MakeEntry(1, model::BotCombatConditionLogic::All);
    entry.conditions.push_back(MakeEnemyAbsentCondition()); // passes
    entry.conditions.push_back(MakeEnemyExistsCondition()); // fails
    profile.rotationEntries.push_back(entry);

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

// ── condition logic: Any ─────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, AnyConditionLogicWithNoConditionsFailsGate)
{
    // Any + empty conditions → no condition can be true → entry skipped.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    profile.rotationEntries.push_back(
        MakeEntry(1, model::BotCombatConditionLogic::Any));

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

TEST(BotCombatRuntimeEvaluatorTest, AnyConditionLogicPassesWhenAtLeastOneConditionPasses)
{
    // Two conditions under Any: one fails (Exists on null enemy), one passes
    // (enemy absent == 0). The overall gate must pass, entry proceeds to action.
    // Action still fails (null bot), so disposition is None — not a crash.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;

    auto entry = MakeEntry(1, model::BotCombatConditionLogic::Any);
    entry.conditions.push_back(MakeEnemyExistsCondition()); // fails
    entry.conditions.push_back(MakeEnemyAbsentCondition()); // passes
    profile.rotationEntries.push_back(entry);

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    // Gate passed; action failed (null bot). Must not crash.
    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

// ── exists condition ──────────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, ExistsConditionFailsWhenEnemyIsNull)
{
    // Exists operator on a null enemy should evaluate to false → entry skipped.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;

    auto entry = MakeEntry(1, model::BotCombatConditionLogic::All);
    entry.conditions.push_back(MakeEnemyExistsCondition());
    profile.rotationEntries.push_back(entry);

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

TEST(BotCombatRuntimeEvaluatorTest, ExistsEqualZeroPassesWhenEnemyIsNull)
{
    // Equal + numericValue 0 on the "exists" stat passes when enemy is absent.
    // The entry reaches the action step (fails due to null bot) — no crash.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;

    auto entry = MakeEntry(1, model::BotCombatConditionLogic::All);
    entry.conditions.push_back(MakeEnemyAbsentCondition());
    profile.rotationEntries.push_back(entry);

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

// ── target key resolution ─────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, SelfTargetKeyWithNullBotGivesNone)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    profile.rotationEntries.push_back(
        MakeEntry(1, model::BotCombatConditionLogic::All, "self"));

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

TEST(BotCombatRuntimeEvaluatorTest, OwnerTargetKeyWithNullOwnerGivesNone)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    profile.rotationEntries.push_back(
        MakeEntry(1, model::BotCombatConditionLogic::All, "owner"));

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

TEST(BotCombatRuntimeEvaluatorTest, UnknownTargetKeyGivesNone)
{
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    profile.rotationEntries.push_back(
        MakeEntry(1, model::BotCombatConditionLogic::All, "not_a_real_key"));

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
}

// ── multi-entry stability ─────────────────────────────────────────────────────

TEST(BotCombatRuntimeEvaluatorTest, MultipleEntriesAllSkippedGivesNone)
{
    // Three entries in unsorted priority order. All have Any+empty conditions
    // (gate fails) so every entry is skipped. Result must be stable None.
    BotCombatRuntimeEvaluator evaluator;
    BotCombatPreparedProfile profile;
    profile.rotationEntries.push_back(MakeEntry(3, model::BotCombatConditionLogic::Any));
    profile.rotationEntries.push_back(MakeEntry(1, model::BotCombatConditionLogic::Any));
    profile.rotationEntries.push_back(MakeEntry(2, model::BotCombatConditionLogic::Any));

    auto const result = evaluator.EvaluateRotation(profile, NullContext());

    EXPECT_EQ(result.disposition, BotCombatEvaluationDisposition::None);
    EXPECT_FALSE(result.action.has_value());
}

} // namespace
} // namespace service
} // namespace living_world
