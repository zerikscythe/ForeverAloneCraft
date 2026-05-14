#include "service/WorldBotSpellPenetrationBaseline.h"

#include <gtest/gtest.h>

using living_world::service::ApplyWorldBotSpellPenetration;

TEST(WorldBotSpellPenetrationBaselineTest, ReducesMagicalResistance)
{
    EXPECT_EQ(ApplyWorldBotSpellPenetration(SPELL_SCHOOL_MASK_FIRE, 130.0f, 70), 60.0f);
}

TEST(WorldBotSpellPenetrationBaselineTest, FloorsResistanceAtZero)
{
    EXPECT_EQ(ApplyWorldBotSpellPenetration(SPELL_SCHOOL_MASK_FROST, 40.0f, 100), 0.0f);
}

TEST(WorldBotSpellPenetrationBaselineTest, SkipsPhysicalSchool)
{
    EXPECT_EQ(ApplyWorldBotSpellPenetration(SPELL_SCHOOL_MASK_NORMAL, 130.0f, 70), 130.0f);
}
