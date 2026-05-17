#include "script/WorldBotHotZoneTracker.h"

#include "gtest/gtest.h"

namespace living_world::script
{

TEST(WorldBotHotZoneTrackerTest, MarkedZoneIsImmediatelyHot)
{
    ClearWorldBotHotZoneStateForTests();

    MarkWorldBotZoneHot(1u, 12u, 1000u);
    EXPECT_TRUE(IsWorldBotZoneHot(1u, 12u, 1000u));
}

TEST(WorldBotHotZoneTrackerTest, MapZeroZoneIsTrackedNormally)
{
    ClearWorldBotHotZoneStateForTests();

    MarkWorldBotZoneHot(0u, 35u, 1000u);
    EXPECT_TRUE(IsWorldBotZoneHot(0u, 35u, 1000u));
}

TEST(WorldBotHotZoneTrackerTest, ZoneExpiresAfterCooldown)
{
    ClearWorldBotHotZoneStateForTests();

    MarkWorldBotZoneHot(1u, 12u, 1000u);
    EXPECT_FALSE(IsWorldBotZoneHot(1u, 12u, 1000u + WorldBotHotZoneCooldownMs + 1u));
}

TEST(WorldBotHotZoneTrackerTest, PruneRemovesExpiredZones)
{
    ClearWorldBotHotZoneStateForTests();

    MarkWorldBotZoneHot(1u, 12u, 1000u);
    PruneWorldBotHotZones(1000u + WorldBotHotZoneCooldownMs + 1u);
    EXPECT_FALSE(IsWorldBotZoneHot(1u, 12u, 1000u + WorldBotHotZoneCooldownMs + 1u));
}

TEST(WorldBotHotZoneTrackerTest, ZonesAreTrackedIndependently)
{
    ClearWorldBotHotZoneStateForTests();

    MarkWorldBotZoneHot(1u, 12u, 1000u);
    MarkWorldBotZoneHot(1u, 34u, 2000u);

    EXPECT_FALSE(IsWorldBotZoneHot(1u, 12u, 1000u + WorldBotHotZoneCooldownMs + 1u));
    EXPECT_TRUE(IsWorldBotZoneHot(1u, 34u, 2000u + WorldBotHotZoneCooldownMs - 1u));
}

TEST(WorldBotHotZoneTrackerTest, ReMarkingZoneRefreshesCooldown)
{
    ClearWorldBotHotZoneStateForTests();

    MarkWorldBotZoneHot(1u, 12u, 1000u);
    MarkWorldBotZoneHot(1u, 12u, 2000u);

    EXPECT_TRUE(IsWorldBotZoneHot(1u, 12u, 2000u + WorldBotHotZoneCooldownMs - 1u));
    EXPECT_FALSE(IsWorldBotZoneHot(1u, 12u, 2000u + WorldBotHotZoneCooldownMs + 1u));
}

TEST(WorldBotHotZoneTrackerTest, SyntheticInterestMatchesConfiguredZone)
{
    ClearWorldBotHotZoneStateForTests();

    SetSyntheticWorldBotInterest(530u, 3703u);
    EXPECT_TRUE(HasSyntheticWorldBotInterest(530u, 3703u));
    EXPECT_FALSE(HasSyntheticWorldBotInterest(530u, 3519u));
    EXPECT_FALSE(HasSyntheticWorldBotInterest(571u, 3703u));
}

TEST(WorldBotHotZoneTrackerTest, SyntheticInterestCanBeMapWide)
{
    ClearWorldBotHotZoneStateForTests();

    SetSyntheticWorldBotInterest(530u, 0u);
    EXPECT_TRUE(HasSyntheticWorldBotInterest(530u, 3703u));
    EXPECT_TRUE(HasSyntheticWorldBotInterest(530u, 3519u));
}

TEST(WorldBotHotZoneTrackerTest, SyntheticInterestSupportsMapZero)
{
    ClearWorldBotHotZoneStateForTests();

    SetSyntheticWorldBotInterest(0u, 35u);
    EXPECT_TRUE(HasSyntheticWorldBotInterest(0u, 35u));
    EXPECT_FALSE(HasSyntheticWorldBotInterest(1u, 35u));
}

TEST(WorldBotHotZoneTrackerTest, CooldownOverrideShortensHotLifetime)
{
    ClearWorldBotHotZoneStateForTests();

    SetWorldBotHotZoneCooldownOverrideMs(100u);
    MarkWorldBotZoneHot(1u, 12u, 1000u);
    EXPECT_TRUE(IsWorldBotZoneHot(1u, 12u, 1099u));
    EXPECT_FALSE(IsWorldBotZoneHot(1u, 12u, 1101u));
}

} // namespace living_world::script
