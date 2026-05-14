#include "script/WorldBotMaterializationIdentity.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace script
{

TEST(WorldBotMaterializationIdentityTest, UsesRefreshedIdentityWhenIdsMatch)
{
    integration::BotIdentityRecord cached;
    cached.id = 77;
    cached.level = 40;
    cached.specKey = "Retribution";
    cached.personalityKey = "uninterested";

    integration::BotIdentityRecord refreshed = cached;
    refreshed.level = 41;
    refreshed.specKey = "Holy";
    refreshed.personalityKey = "aggressive";

    integration::BotIdentityRecord const selected =
        SelectWorldBotMaterializationIdentity(cached, refreshed);

    EXPECT_EQ(selected.id, 77u);
    EXPECT_EQ(selected.level, 41u);
    EXPECT_EQ(selected.specKey, "Holy");
    EXPECT_EQ(selected.personalityKey, "aggressive");
}

TEST(WorldBotMaterializationIdentityTest, FallsBackToCachedIdentityWhenRefreshIsMissing)
{
    integration::BotIdentityRecord cached;
    cached.id = 55;
    cached.level = 32;
    cached.specKey = "Fury";
    cached.personalityKey = "opportunistic";

    integration::BotIdentityRecord const selected =
        SelectWorldBotMaterializationIdentity(cached, std::nullopt);

    EXPECT_EQ(selected.id, 55u);
    EXPECT_EQ(selected.level, 32u);
    EXPECT_EQ(selected.specKey, "Fury");
    EXPECT_EQ(selected.personalityKey, "opportunistic");
}

TEST(WorldBotMaterializationIdentityTest, FallsBackToCachedIdentityWhenRefreshIdDoesNotMatch)
{
    integration::BotIdentityRecord cached;
    cached.id = 11;
    cached.level = 22;
    cached.specKey = "Frost";

    integration::BotIdentityRecord refreshed;
    refreshed.id = 12;
    refreshed.level = 80;
    refreshed.specKey = "Arcane";

    integration::BotIdentityRecord const selected =
        SelectWorldBotMaterializationIdentity(cached, refreshed);

    EXPECT_EQ(selected.id, 11u);
    EXPECT_EQ(selected.level, 22u);
    EXPECT_EQ(selected.specKey, "Frost");
}

TEST(WorldBotMaterializationIdentityTest, FallsBackToCachedIdentityWhenRefreshIsRetired)
{
    integration::BotIdentityRecord cached;
    cached.id = 99;
    cached.level = 60;
    cached.specKey = "Shadow";

    integration::BotIdentityRecord refreshed = cached;
    refreshed.level = 61;
    refreshed.isRetired = true;

    integration::BotIdentityRecord const selected =
        SelectWorldBotMaterializationIdentity(cached, refreshed);

    EXPECT_EQ(selected.id, 99u);
    EXPECT_EQ(selected.level, 60u);
    EXPECT_EQ(selected.specKey, "Shadow");
}

} // namespace script
} // namespace living_world