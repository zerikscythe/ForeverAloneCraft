#include "service/SimpleBotCombatSpecRoleResolver.h"

#include "SharedDefines.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(SimpleBotCombatSpecRoleResolverTest, UsesClassGuessWhenNoOverridesArePresent)
{
    SimpleBotCombatSpecRoleResolver resolver;

    BotCombatSpecRoleResolutionRequest request;
    request.classId = CLASS_MAGE;

    BotCombatSpecRoleResolutionResult const result = resolver.Resolve(request);
    EXPECT_EQ(result.guessedSpecKey, "Frost");
    EXPECT_EQ(result.guessedRoleKey, "DPS");
    EXPECT_EQ(result.effectiveSpecKey, "Frost");
    EXPECT_EQ(result.effectiveRoleKey, "DPS");
}

TEST(SimpleBotCombatSpecRoleResolverTest, AppliesSpecOverrideWithoutChangingGuessedValues)
{
    SimpleBotCombatSpecRoleResolver resolver;

    BotCombatSpecRoleResolutionRequest request;
    request.classId = CLASS_PALADIN;
    request.specOverrideKey = std::string("Holy");

    BotCombatSpecRoleResolutionResult const result = resolver.Resolve(request);
    EXPECT_EQ(result.guessedSpecKey, "Retribution");
    EXPECT_EQ(result.guessedRoleKey, "DPS");
    EXPECT_EQ(result.effectiveSpecKey, "Holy");
    EXPECT_EQ(result.effectiveRoleKey, "DPS");
}

TEST(SimpleBotCombatSpecRoleResolverTest, AppliesRoleOverrideWithoutChangingGuessedValues)
{
    SimpleBotCombatSpecRoleResolver resolver;

    BotCombatSpecRoleResolutionRequest request;
    request.classId = CLASS_WARRIOR;
    request.roleOverrideKey = std::string("TANK");

    BotCombatSpecRoleResolutionResult const result = resolver.Resolve(request);
    EXPECT_EQ(result.guessedSpecKey, "Arms");
    EXPECT_EQ(result.guessedRoleKey, "DPS");
    EXPECT_EQ(result.effectiveSpecKey, "Arms");
    EXPECT_EQ(result.effectiveRoleKey, "TANK");
}

TEST(SimpleBotCombatSpecRoleResolverTest, AppliesBothOverridesTogether)
{
    SimpleBotCombatSpecRoleResolver resolver;

    BotCombatSpecRoleResolutionRequest request;
    request.classId = CLASS_DRUID;
    request.specOverrideKey = std::string("Restoration");
    request.roleOverrideKey = std::string("HEAL");

    BotCombatSpecRoleResolutionResult const result = resolver.Resolve(request);
    EXPECT_EQ(result.guessedSpecKey, "Balance");
    EXPECT_EQ(result.guessedRoleKey, "DPS");
    EXPECT_EQ(result.effectiveSpecKey, "Restoration");
    EXPECT_EQ(result.effectiveRoleKey, "HEAL");
}

TEST(SimpleBotCombatSpecRoleResolverTest, UnknownClassReturnsEmptyDefaultsButPreservesOverrides)
{
    SimpleBotCombatSpecRoleResolver resolver;

    BotCombatSpecRoleResolutionRequest request;
    request.classId = 0;
    request.specOverrideKey = std::string("CustomSpec");
    request.roleOverrideKey = std::string("CustomRole");

    BotCombatSpecRoleResolutionResult const result = resolver.Resolve(request);
    EXPECT_EQ(result.guessedSpecKey, "");
    EXPECT_EQ(result.guessedRoleKey, "");
    EXPECT_EQ(result.effectiveSpecKey, "CustomSpec");
    EXPECT_EQ(result.effectiveRoleKey, "CustomRole");
}
} // namespace service
} // namespace living_world