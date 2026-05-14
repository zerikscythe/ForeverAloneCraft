#include "service/WorldBotDefensiveCombatBaseline.h"

#include "SharedDefines.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotDefensiveCombatBaselineTest, DerivesShieldBlockAndParryForShieldClasses)
{
    WorldBotDefensiveCombatBaseline const baseline =
        BuildWorldBotDefensiveCombatBaseline(CLASS_WARRIOR, 100, 0.0002f, true);

    EXPECT_TRUE(baseline.canParry);
    EXPECT_TRUE(baseline.canBlock);
    EXPECT_FLOAT_EQ(baseline.parryChance, 5.0f);
    EXPECT_FLOAT_EQ(baseline.blockChance, 5.0f);
    EXPECT_GT(baseline.dodgeChance, 0.0f);
}

TEST(WorldBotDefensiveCombatBaselineTest, RemovesParryAndBlockForNonEligibleClasses)
{
    WorldBotDefensiveCombatBaseline const baseline =
        BuildWorldBotDefensiveCombatBaseline(CLASS_PRIEST, 100, 0.0002f, true);

    EXPECT_FALSE(baseline.canParry);
    EXPECT_FALSE(baseline.canBlock);
    EXPECT_FLOAT_EQ(baseline.parryChance, 0.0f);
    EXPECT_FLOAT_EQ(baseline.blockChance, 0.0f);
}

TEST(WorldBotDefensiveCombatBaselineTest, FloorsNegativeDodgeBaselinesAtZero)
{
    WorldBotDefensiveCombatBaseline const baseline =
        BuildWorldBotDefensiveCombatBaseline(CLASS_HUNTER, 1, 0.0001f, false);

    EXPECT_FLOAT_EQ(baseline.dodgeChance, 0.0f);
}

TEST(WorldBotDefensiveCombatBaselineTest, AdjustsExistingCreatureChanceByPlayerLikeDelta)
{
    EXPECT_EQ(AdjustWorldBotDefensiveChance(650, 4.25f), 575);
    EXPECT_EQ(AdjustWorldBotDefensiveChance(500, 0.0f), 0);
}

TEST(WorldBotDefensiveCombatBaselineTest, AppliesDefenseAndAvoidanceRatings)
{
    WorldBotDefensiveCombatBaseline const baseline =
        BuildWorldBotDefensiveCombatBaseline(
            CLASS_WARRIOR,
            100,
            0.0002f,
            true,
            25.0f,
            10.0f,
            15.0f,
            20.0f);

    WorldBotDefensiveCombatBaseline const unrated =
        BuildWorldBotDefensiveCombatBaseline(CLASS_WARRIOR, 100, 0.0002f, true);

    EXPECT_GT(baseline.dodgeChance, unrated.dodgeChance);
    EXPECT_GT(baseline.parryChance, unrated.parryChance);
    EXPECT_FLOAT_EQ(baseline.blockChance, 26.0f);
}

TEST(WorldBotDefensiveCombatBaselineTest, DefenseRatingAddsMissAndCritSuppression)
{
    EXPECT_GT(BuildWorldBotDefenseMissChanceBonusPct(CLASS_PALADIN, 30.0f), 0.0f);
    EXPECT_FLOAT_EQ(BuildWorldBotDefenseCritSuppressionPct(30.0f), 1.2f);
}
} // namespace service
} // namespace living_world
