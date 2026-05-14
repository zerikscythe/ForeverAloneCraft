#include "service/WorldBotArmorPenetrationBaseline.h"

#include <gtest/gtest.h>

using living_world::service::ApplyWorldBotArmorPenetration;
using living_world::service::BuildWorldBotArmorPenetrationCap;

TEST(WorldBotArmorPenetrationBaselineTest, BuildsWrathArmorPenetrationCap)
{
    EXPECT_NEAR(BuildWorldBotArmorPenetrationCap(10000.0f, 80.0f), 8410.83f, 0.01f);
}

TEST(WorldBotArmorPenetrationBaselineTest, AppliesPercentAgainstCappedArmorPool)
{
    EXPECT_NEAR(ApplyWorldBotArmorPenetration(10000.0f, 80.0f, 50.0f), 5794.58f, 0.01f);
}

TEST(WorldBotArmorPenetrationBaselineTest, ClampsAtFullCapAndFloorsArmor)
{
    EXPECT_NEAR(ApplyWorldBotArmorPenetration(10000.0f, 80.0f, 150.0f), 1589.17f, 0.01f);
    EXPECT_EQ(ApplyWorldBotArmorPenetration(-10.0f, 80.0f, 50.0f), 0.0f);
}
