#include "service/WorldBotHasteBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotHasteBaselineTest, KeepsPositiveHasteBonus)
{
    EXPECT_FLOAT_EQ(NormalizeWorldBotHasteBonus(12.5f), 12.5f);
}

TEST(WorldBotHasteBaselineTest, FloorsExtremeNegativeHasteBeforeUnitApplication)
{
    EXPECT_FLOAT_EQ(NormalizeWorldBotHasteBonus(-150.0f), -99.0f);
}
} // namespace service
} // namespace living_world
