#include "service/WorldBotPassiveSpellRules.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(WorldBotPassiveSpellRulesTest, AcceptsPlainPassiveSpell)
{
    EXPECT_TRUE(IsWorldBotPassiveSelfAuraCandidate(
        true,
        false,
        0,
        false));
}

TEST(WorldBotPassiveSpellRulesTest, AcceptsDoNotDisplayStanceSpellOnlyWhenAllowedOutOfForm)
{
    EXPECT_TRUE(IsWorldBotPassiveSelfAuraCandidate(
        false,
        true,
        1u,
        true));
    EXPECT_FALSE(IsWorldBotPassiveSelfAuraCandidate(
        false,
        true,
        1u,
        false));
}

TEST(WorldBotPassiveSpellRulesTest, RejectsActiveVisibleSpell)
{
    EXPECT_FALSE(IsWorldBotPassiveSelfAuraCandidate(
        false,
        false,
        0,
        false));
}
} // namespace service
} // namespace living_world