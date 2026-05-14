#include "service/WorldBotExpertiseBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotExpertiseBaselineTest, ConvertsExpertiseToDodgeParryReduction)
{
    EXPECT_FLOAT_EQ(BuildWorldBotExpertiseDodgeOrParryReductionPct(16.0f), 4.0f);
}

TEST(WorldBotExpertiseBaselineTest, ReducesDodgeOrParryChance)
{
    EXPECT_EQ(AdjustWorldBotDodgeOrParryChanceForExpertise(900, 12.0f), 600);
}

TEST(WorldBotExpertiseBaselineTest, FloorsDodgeOrParryChanceAtZero)
{
    EXPECT_EQ(AdjustWorldBotDodgeOrParryChanceForExpertise(100, 12.0f), 0);
}
} // namespace service
} // namespace living_world
