#include "service/WorldBotPhysicalDamageBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
float ResolveCreatureBaselineDamage(
    float weaponSeedDamage,
    float attackPower,
    std::uint32_t attackTimeMs)
{
    float const attackSpeedSeconds = attackTimeMs > 0
        ? static_cast<float>(attackTimeMs) / 1000.0f
        : 1.0f;
    return (weaponSeedDamage + attackPower / 14.0f) * attackSpeedSeconds;
}
} // namespace

TEST(WorldBotPhysicalDamageBaselineTest, PreservesTwoSecondPlayerBaselineUnderCreatureFormula)
{
    WorldBotPhysicalDamageBaseline const baseline =
        BuildWorldBotPhysicalDamageBaseline(2000, 2000, 2000);

    float constexpr AttackPower = 210.0f;
    float constexpr ExpectedMinDamage = 1.0f + (AttackPower / 14.0f) * 2.0f;
    float constexpr ExpectedMaxDamage = 2.0f + (AttackPower / 14.0f) * 2.0f;

    EXPECT_FLOAT_EQ(ResolveCreatureBaselineDamage(baseline.mainHandMinDamage, AttackPower, 2000), ExpectedMinDamage);
    EXPECT_FLOAT_EQ(ResolveCreatureBaselineDamage(baseline.mainHandMaxDamage, AttackPower, 2000), ExpectedMaxDamage);
    EXPECT_FLOAT_EQ(ResolveCreatureBaselineDamage(baseline.rangedMinDamage, AttackPower, 2000), ExpectedMinDamage);
    EXPECT_FLOAT_EQ(ResolveCreatureBaselineDamage(baseline.rangedMaxDamage, AttackPower, 2000), ExpectedMaxDamage);
}

TEST(WorldBotPhysicalDamageBaselineTest, ScalesWeaponSeedByAttackSpeed)
{
    WorldBotPhysicalDamageBaseline const baseline =
        BuildWorldBotPhysicalDamageBaseline(1000, 1500, 2500);

    EXPECT_FLOAT_EQ(baseline.mainHandMinDamage, 1.0f);
    EXPECT_FLOAT_EQ(baseline.mainHandMaxDamage, 2.0f);
    EXPECT_FLOAT_EQ(baseline.offHandMinDamage, 1.0f / 1.5f);
    EXPECT_FLOAT_EQ(baseline.offHandMaxDamage, 2.0f / 1.5f);
    EXPECT_FLOAT_EQ(baseline.rangedMinDamage, 1.0f / 2.5f);
    EXPECT_FLOAT_EQ(baseline.rangedMaxDamage, 2.0f / 2.5f);
}

TEST(WorldBotPhysicalDamageBaselineTest, FallsBackToOneSecondWhenAttackTimeIsMissing)
{
    WorldBotPhysicalDamageBaseline const baseline =
        BuildWorldBotPhysicalDamageBaseline(0, 0, 0);

    EXPECT_FLOAT_EQ(baseline.mainHandMinDamage, 1.0f);
    EXPECT_FLOAT_EQ(baseline.mainHandMaxDamage, 2.0f);
    EXPECT_FLOAT_EQ(baseline.offHandMinDamage, 1.0f);
    EXPECT_FLOAT_EQ(baseline.offHandMaxDamage, 2.0f);
    EXPECT_FLOAT_EQ(baseline.rangedMinDamage, 1.0f);
    EXPECT_FLOAT_EQ(baseline.rangedMaxDamage, 2.0f);
}
} // namespace service
} // namespace living_world