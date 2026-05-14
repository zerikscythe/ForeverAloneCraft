#include "service/WorldBotManaRegenBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotManaRegenBaselineTest, BuildsSpiritIntellectManaRegenPerSecond)
{
    EXPECT_NEAR(BuildWorldBotSpiritManaRegenPerSecond(100.0f, 50.0f, 0.2f, 1.0f), 100.0f, 0.001f);
}

TEST(WorldBotManaRegenBaselineTest, AppliesMp5AndSpiritForNormalRegen)
{
    EXPECT_NEAR(BuildWorldBotManaRegenPerTick(10.0f, 4.0f, 0.0f, false, 1.0f, 2000), 28.0f, 0.001f);
}

TEST(WorldBotManaRegenBaselineTest, AppliesInterruptedSpiritPercent)
{
    EXPECT_NEAR(BuildWorldBotManaRegenPerTick(10.0f, 4.0f, 30.0f, true, 1.0f, 2000), 14.0f, 0.001f);
}
} // namespace service
} // namespace living_world
