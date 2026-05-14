#include "service/WorldBotSpellHitBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotSpellHitBaselineTest, AddsSpellHitBonusToHitChance)
{
    EXPECT_EQ(AdjustWorldBotMagicSpellHitChance(8300, 4.25f), 8725);
}

TEST(WorldBotSpellHitBaselineTest, AllowsCoreClampToOwnCaps)
{
    EXPECT_EQ(AdjustWorldBotMagicSpellHitChance(9900, 2.0f), 10100);
}
} // namespace service
} // namespace living_world
