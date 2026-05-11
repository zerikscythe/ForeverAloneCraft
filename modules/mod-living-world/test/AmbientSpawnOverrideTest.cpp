#include "script/AmbientSpawnOverride.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace script
{

TEST(AmbientSpawnOverrideTest, DisabledWhenForcedSpawnCountIsZero)
{
    AmbientSpawnOverrideConfig config;
    config.forcedSpawnCount = 0;
    config.forcedMapId = 530;

    EXPECT_FALSE(HasForcedSpawnOverride(config));
    EXPECT_FALSE(ShouldUseForcedSpawn(config, 0u, 0u));
}

TEST(AmbientSpawnOverrideTest, DisabledWhenForcedMapIdIsZero)
{
    AmbientSpawnOverrideConfig config;
    config.forcedSpawnCount = 3;
    config.forcedMapId = 0;

    EXPECT_FALSE(HasForcedSpawnOverride(config));
    EXPECT_FALSE(ShouldUseForcedSpawn(config, 0u, 0u));
}

TEST(AmbientSpawnOverrideTest, EnabledWhenCountAndMapAreConfigured)
{
    AmbientSpawnOverrideConfig config;
    config.forcedSpawnCount = 3;
    config.forcedMapId = 530;

    EXPECT_TRUE(HasForcedSpawnOverride(config));
}

TEST(AmbientSpawnOverrideTest, UsesForcedSpawnWhileBelowConfiguredActiveThreshold)
{
    AmbientSpawnOverrideConfig config;
    config.forcedSpawnCount = 3;
    config.forcedMapId = 530;

    EXPECT_TRUE(ShouldUseForcedSpawn(config, 0u, 0u));
    EXPECT_TRUE(ShouldUseForcedSpawn(config, 1u, 0u));
    EXPECT_TRUE(ShouldUseForcedSpawn(config, 1u, 1u));
}

TEST(AmbientSpawnOverrideTest, StopsUsingForcedSpawnAtConfiguredThreshold)
{
    AmbientSpawnOverrideConfig config;
    config.forcedSpawnCount = 3;
    config.forcedMapId = 530;

    EXPECT_FALSE(ShouldUseForcedSpawn(config, 3u, 0u));
    EXPECT_FALSE(ShouldUseForcedSpawn(config, 2u, 1u));
    EXPECT_FALSE(ShouldUseForcedSpawn(config, 1u, 2u));
}

} // namespace script
} // namespace living_world