#include "service/WorldBotSpellPowerBaseline.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotSpellPowerBaselineTest, AddsSpellPowerForMagicalSchools)
{
    std::int32_t bonus = 25;

    ApplyWorldBotSpellPowerBonus(SPELL_SCHOOL_MASK_FIRE, 40, bonus);

    EXPECT_EQ(bonus, 65);
}

TEST(WorldBotSpellPowerBaselineTest, SkipsNormalSchoolDamage)
{
    std::int32_t bonus = 25;

    ApplyWorldBotSpellPowerBonus(SPELL_SCHOOL_MASK_NORMAL, 40, bonus);

    EXPECT_EQ(bonus, 25);
}

TEST(WorldBotSpellPowerBaselineTest, AddsHealingPower)
{
    std::int32_t bonus = 10;

    ApplyWorldBotHealingPowerBonus(55, bonus);

    EXPECT_EQ(bonus, 65);
}
} // namespace service
} // namespace living_world
