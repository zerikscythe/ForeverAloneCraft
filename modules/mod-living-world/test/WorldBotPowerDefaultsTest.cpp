#include "service/WorldBotPowerBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotPowerDefaultsTest, StartsManaAndEnergyAtFull)
{
    EXPECT_EQ(ResolveWorldBotSpawnPower(POWER_MANA, 1200), 1200u);
    EXPECT_EQ(ResolveWorldBotSpawnPower(POWER_ENERGY, 100), 100u);
}

TEST(WorldBotPowerDefaultsTest, StartsRageAndRunicPowerAtZero)
{
    EXPECT_EQ(ResolveWorldBotSpawnPower(POWER_RAGE, 1000), 0u);
    EXPECT_EQ(ResolveWorldBotSpawnPower(POWER_RUNIC_POWER, 1000), 0u);
}

TEST(WorldBotPowerDefaultsTest, LeavesZeroMaxPowerAtZero)
{
    EXPECT_EQ(ResolveWorldBotSpawnPower(POWER_MANA, 0), 0u);
    EXPECT_EQ(ResolveWorldBotSpawnPower(POWER_RAGE, 0), 0u);
}

TEST(WorldBotPowerDefaultsTest, ResolvesPlayerLikePowerTypeByClass)
{
    EXPECT_EQ(ResolveWorldBotPowerType(CLASS_WARRIOR), POWER_RAGE);
    EXPECT_EQ(ResolveWorldBotPowerType(CLASS_ROGUE), POWER_ENERGY);
    EXPECT_EQ(ResolveWorldBotPowerType(CLASS_DEATH_KNIGHT), POWER_RUNIC_POWER);
    EXPECT_EQ(ResolveWorldBotPowerType(CLASS_MAGE), POWER_MANA);
}

TEST(WorldBotPowerDefaultsTest, UsesPlayerLikeNonManaCaps)
{
    EXPECT_EQ(ResolveWorldBotMaxPower(POWER_ENERGY, 0), 100u);
    EXPECT_EQ(ResolveWorldBotMaxPower(POWER_RAGE, 0), 1000u);
    EXPECT_EQ(ResolveWorldBotMaxPower(POWER_RUNIC_POWER, 0), 1000u);
    EXPECT_EQ(ResolveWorldBotMaxPower(POWER_MANA, 2400), 2400u);
}
} // namespace service
} // namespace living_world
