#include "service/WorldBotBlockValueBaseline.h"

#include <gtest/gtest.h>

using living_world::service::BuildWorldBotShieldBlockValue;

TEST(WorldBotBlockValueBaselineTest, MirrorsPlayerStrengthAndFlatBlockFormula)
{
    EXPECT_EQ(BuildWorldBotShieldBlockValue(true, 80.0f, 45.0f, 1.0f), 75u);
}

TEST(WorldBotBlockValueBaselineTest, AppliesShieldBlockValueMultiplier)
{
    EXPECT_EQ(BuildWorldBotShieldBlockValue(true, 80.0f, 45.0f, 1.25f), 93u);
}

TEST(WorldBotBlockValueBaselineTest, RequiresShieldAndFloorsNegativeValues)
{
    EXPECT_EQ(BuildWorldBotShieldBlockValue(false, 80.0f, 45.0f, 1.0f), 0u);
    EXPECT_EQ(BuildWorldBotShieldBlockValue(true, 10.0f, 0.0f, 1.0f), 0u);
}
