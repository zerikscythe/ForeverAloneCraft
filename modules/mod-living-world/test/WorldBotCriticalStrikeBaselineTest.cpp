#include "service/WorldBotCriticalStrikeBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotCriticalStrikeBaselineTest, DerivesCritFromAgilityAndDbcValues)
{
    WorldBotCriticalStrikeBaseline const baseline =
        BuildWorldBotCriticalStrikeBaseline(150, 0.0125f, 0.0004f);

    EXPECT_FLOAT_EQ(baseline.critChance, 7.25f);
}

TEST(WorldBotCriticalStrikeBaselineTest, FloorsNegativeBaselineAtZero)
{
    WorldBotCriticalStrikeBaseline const baseline =
        BuildWorldBotCriticalStrikeBaseline(0, -0.05f, 0.0001f);

    EXPECT_FLOAT_EQ(baseline.critChance, 0.0f);
}

TEST(WorldBotCriticalStrikeBaselineTest, AdjustsExistingCreatureCritByPlayerLikeDelta)
{
    EXPECT_EQ(AdjustWorldBotCriticalStrikeChance(500, 7.25f), 725);
    EXPECT_EQ(AdjustWorldBotCriticalStrikeChance(500, 0.0f), 0);
}
} // namespace service
} // namespace living_world