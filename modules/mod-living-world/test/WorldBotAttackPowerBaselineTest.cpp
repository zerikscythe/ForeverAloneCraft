#include "service/WorldBotAttackPowerBaseline.h"

#include "SharedDefines.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotAttackPowerBaselineTest, DerivesWarriorAttackPowerFromLevelAndStrength)
{
    WorldBotAttackPowerBaseline const baseline =
        BuildWorldBotAttackPowerBaseline(CLASS_WARRIOR, 40, 50, 30);

    EXPECT_EQ(baseline.meleeAttackPower, 200);
    EXPECT_EQ(baseline.rangedAttackPower, 60);
}

TEST(WorldBotAttackPowerBaselineTest, DerivesHunterMeleeAndRangedAttackPower)
{
    WorldBotAttackPowerBaseline const baseline =
        BuildWorldBotAttackPowerBaseline(CLASS_HUNTER, 30, 35, 60);

    EXPECT_EQ(baseline.meleeAttackPower, 135);
    EXPECT_EQ(baseline.rangedAttackPower, 110);
}

TEST(WorldBotAttackPowerBaselineTest, FloorsNegativeBaselineValuesAtZero)
{
    WorldBotAttackPowerBaseline const baseline =
        BuildWorldBotAttackPowerBaseline(CLASS_MAGE, 1, 5, 5);

    EXPECT_EQ(baseline.meleeAttackPower, 0);
    EXPECT_EQ(baseline.rangedAttackPower, 0);
}
} // namespace service
} // namespace living_world