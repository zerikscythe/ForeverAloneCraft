#include "service/WorldBotGearBand.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotGearBandTest, ComputesFiveLevelRefreshBands)
{
    EXPECT_EQ(ComputeWorldBotGearRefreshBand(1), 1u);
    EXPECT_EQ(ComputeWorldBotGearRefreshBand(5), 1u);
    EXPECT_EQ(ComputeWorldBotGearRefreshBand(6), 2u);
    EXPECT_EQ(ComputeWorldBotGearRefreshBand(10), 2u);
    EXPECT_EQ(ComputeWorldBotGearRefreshBand(11), 3u);
    EXPECT_EQ(ComputeWorldBotGearRefreshBand(80), 16u);
}
} // namespace service
} // namespace living_world