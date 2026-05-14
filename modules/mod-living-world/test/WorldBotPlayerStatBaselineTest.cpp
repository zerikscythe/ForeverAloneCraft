#include "service/WorldBotPlayerStatBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotPlayerStatBaselineTest, CopiesPlayerPrimaryStatsAndBaseVitals)
{
    PlayerClassLevelInfo classInfo;
    classInfo.basehealth = 4321;
    classInfo.basemana = 2100;

    PlayerLevelInfo levelInfo;
    levelInfo.stats[STAT_STRENGTH] = 11;
    levelInfo.stats[STAT_AGILITY] = 22;
    levelInfo.stats[STAT_STAMINA] = 33;
    levelInfo.stats[STAT_INTELLECT] = 44;
    levelInfo.stats[STAT_SPIRIT] = 55;

    WorldBotPlayerStatBaseline const baseline =
        BuildWorldBotPlayerStatBaseline(classInfo, levelInfo);

    EXPECT_EQ(baseline.baseHealth, 4321u);
    EXPECT_EQ(baseline.baseMana, 2100u);
    EXPECT_EQ(baseline.stats[STAT_STRENGTH], 11u);
    EXPECT_EQ(baseline.stats[STAT_AGILITY], 22u);
    EXPECT_EQ(baseline.stats[STAT_STAMINA], 33u);
    EXPECT_EQ(baseline.stats[STAT_INTELLECT], 44u);
    EXPECT_EQ(baseline.stats[STAT_SPIRIT], 55u);
}

TEST(WorldBotPlayerStatBaselineTest, DerivesBaseArmorFromAgilityLikePlayerInit)
{
    PlayerClassLevelInfo classInfo;
    PlayerLevelInfo levelInfo;
    levelInfo.stats[STAT_AGILITY] = 37;

    WorldBotPlayerStatBaseline const baseline =
        BuildWorldBotPlayerStatBaseline(classInfo, levelInfo);

    EXPECT_EQ(baseline.baseArmor, 74u);
}
} // namespace service
} // namespace living_world