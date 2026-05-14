#include "service/WorldBotResilienceBaseline.h"

#include <gtest/gtest.h>

using living_world::service::ApplyWorldBotResilience;
using living_world::service::CalculateWorldBotResilienceDamageReduction;

TEST(WorldBotResilienceBaselineTest, ReducesCritChanceByRatingPercent)
{
    float critChance = 12.5f;

    ApplyWorldBotResilience(3.25f, &critChance, nullptr, false);

    EXPECT_FLOAT_EQ(critChance, 9.25f);
}

TEST(WorldBotResilienceBaselineTest, AppliesCritAndFlatDamageReductionSequentially)
{
    std::int32_t damage = 1000;

    ApplyWorldBotResilience(10.0f, nullptr, &damage, true);

    EXPECT_EQ(damage, 624);
}

TEST(WorldBotResilienceBaselineTest, AppliesNonCritDamageReductionOnly)
{
    std::int32_t damage = 1000;

    ApplyWorldBotResilience(10.0f, nullptr, &damage, false);

    EXPECT_EQ(damage, 800);
}

TEST(WorldBotResilienceBaselineTest, CapsCritDamageReduction)
{
    EXPECT_EQ(CalculateWorldBotResilienceDamageReduction(1000, 20.0f, 2.2f, 33.0f), 330);
}
