#include "service/WorldBotMeleeHitBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotMeleeHitBaselineTest, ReducesMissChanceByHitBonus)
{
    EXPECT_EQ(AdjustWorldBotMeleeMissChance(800, 3.25f), 475);
}

TEST(WorldBotMeleeHitBaselineTest, FloorsMissChanceAtZero)
{
    EXPECT_EQ(AdjustWorldBotMeleeMissChance(200, 4.0f), 0);
}
} // namespace service
} // namespace living_world
