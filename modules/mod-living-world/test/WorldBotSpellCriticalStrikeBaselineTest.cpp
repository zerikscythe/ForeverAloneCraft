#include "service/WorldBotSpellCriticalStrikeBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotSpellCriticalStrikeBaselineTest, DerivesCritFromIntellectAndDbcValues)
{
    WorldBotSpellCriticalStrikeBaseline const baseline =
        BuildWorldBotSpellCriticalStrikeBaseline(200, 0.0100f, 0.0002f);

    EXPECT_FLOAT_EQ(baseline.critChance, 5.0f);
}

TEST(WorldBotSpellCriticalStrikeBaselineTest, FloorsNegativeBaselineAtZero)
{
    WorldBotSpellCriticalStrikeBaseline const baseline =
        BuildWorldBotSpellCriticalStrikeBaseline(0, -0.05f, 0.0001f);

    EXPECT_FLOAT_EQ(baseline.critChance, 0.0f);
}

TEST(WorldBotSpellCriticalStrikeBaselineTest, AdjustsExistingCreatureCritByPlayerLikeDelta)
{
    EXPECT_FLOAT_EQ(AdjustWorldBotSpellCriticalStrikeChance(5.0f, 7.25f), 7.25f);
    EXPECT_FLOAT_EQ(AdjustWorldBotSpellCriticalStrikeChance(7.0f, 7.25f), 9.25f);
    EXPECT_FLOAT_EQ(AdjustWorldBotSpellCriticalStrikeChance(0.0f, 0.0f), 0.0f);
}
} // namespace service
} // namespace living_world