#include "SharedDefines.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
std::uint32_t ResolveWorldBotSpawnPower(
    Powers powerType,
    std::uint32_t maxPower)
{
    if (maxPower == 0)
        return 0;

    switch (powerType)
    {
        case POWER_MANA:
        case POWER_ENERGY:
            return maxPower;
        case POWER_RAGE:
        case POWER_RUNIC_POWER:
            return 0;
        default:
            return maxPower;
    }
}
} // namespace

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
} // namespace service
} // namespace living_world