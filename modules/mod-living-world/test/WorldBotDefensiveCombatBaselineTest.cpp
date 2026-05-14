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
} // namespace service
} // namespace living_world