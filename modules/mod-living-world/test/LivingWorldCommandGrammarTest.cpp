#include "script/LivingWorldCommandGrammar.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace script
{
TEST(LivingWorldCommandGrammarTest, EmptyInputProducesEmptyError)
{
    ParsedCommand cmd = ParseLivingWorldCommand("");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::Empty);
}

TEST(LivingWorldCommandGrammarTest, UnknownSubsystemIsRejected)
{
    // Mixed alphanumeric is neither a position nor a name — unknown subsystem.
    ParsedCommand cmd = ParseLivingWorldCommand("abc123 list");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::UnknownSubsystem);
}

TEST(LivingWorldCommandGrammarTest, RosterListParses)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster list");

    EXPECT_NE(std::get_if<RosterListCommand>(&cmd), nullptr);
}

TEST(LivingWorldCommandGrammarTest, ShortListParsesWithoutSubsystem)
{
    ParsedCommand cmd = ParseLivingWorldCommand("list");

    EXPECT_NE(std::get_if<RosterListCommand>(&cmd), nullptr);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestParsesPosition)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request 42");

    auto const* request = std::get_if<RosterRequestCommand>(&cmd);
    ASSERT_NE(request, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&request->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 42u);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestParsesName)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request thrall");

    auto const* request = std::get_if<RosterRequestCommand>(&cmd);
    ASSERT_NE(request, nullptr);
    auto const* name = std::get_if<std::string>(&request->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Thrall");
}

TEST(LivingWorldCommandGrammarTest, ShortRequestParsesPositionWithoutSubsystem)
{
    ParsedCommand cmd = ParseLivingWorldCommand("request 42");

    auto const* request = std::get_if<RosterRequestCommand>(&cmd);
    ASSERT_NE(request, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&request->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 42u);
}

TEST(LivingWorldCommandGrammarTest, RosterDismissParsesPosistion)
{
    ParsedCommand cmd = ParseLivingWorldCommand("  roster   dismiss   7 ");

    auto const* dismiss = std::get_if<RosterDismissCommand>(&cmd);
    ASSERT_NE(dismiss, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&dismiss->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 7u);
}

TEST(LivingWorldCommandGrammarTest, ShortDismissParsesPositionWithoutSubsystem)
{
    ParsedCommand cmd = ParseLivingWorldCommand("dismiss 7");

    auto const* dismiss = std::get_if<RosterDismissCommand>(&cmd);
    ASSERT_NE(dismiss, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&dismiss->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 7u);
}

TEST(LivingWorldCommandGrammarTest, RosterDismissParsesName)
{
    ParsedCommand cmd = ParseLivingWorldCommand("dismiss Arthas");

    auto const* dismiss = std::get_if<RosterDismissCommand>(&cmd);
    ASSERT_NE(dismiss, nullptr);
    auto const* name = std::get_if<std::string>(&dismiss->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Arthas");
}

TEST(LivingWorldCommandGrammarTest, RosterRequestRejectsMissingRef)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestRejectsPositionZero)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request 0");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestRejectsMixedAlphanumericRef)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request abc123");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, UnknownRosterVerbRejected)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster teleport 3");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::UnknownVerb);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetParsesPositionAndSlot)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 profile 3");

    auto const* set = std::get_if<BotProfileSetCommand>(&cmd);
    ASSERT_NE(set, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&set->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 2u);
    EXPECT_EQ(set->profileSlot, 3u);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetParsesNameAndSlot)
{
    ParsedCommand cmd = ParseLivingWorldCommand("thrall profile 5");

    auto const* set = std::get_if<BotProfileSetCommand>(&cmd);
    ASSERT_NE(set, nullptr);
    auto const* name = std::get_if<std::string>(&set->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Thrall");
    EXPECT_EQ(set->profileSlot, 5u);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetNormalizesNameCase)
{
    ParsedCommand cmd = ParseLivingWorldCommand("ARTHAS profile 1");

    auto const* set = std::get_if<BotProfileSetCommand>(&cmd);
    ASSERT_NE(set, nullptr);
    auto const* name = std::get_if<std::string>(&set->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Arthas");
}

TEST(LivingWorldCommandGrammarTest, ProfileSetAcceptsSlotBoundaries)
{
    ParsedCommand cmd1 = ParseLivingWorldCommand("1 profile 1");
    ParsedCommand cmd10 = ParseLivingWorldCommand("1 profile 10");

    EXPECT_NE(std::get_if<BotProfileSetCommand>(&cmd1), nullptr);
    EXPECT_NE(std::get_if<BotProfileSetCommand>(&cmd10), nullptr);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetRejectsSlotZero)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 profile 0");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetRejectsSlotElevenAndAbove)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 profile 11");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetRejectsMissingSlot)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 profile");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetRejectsPositionZero)
{
    ParsedCommand cmd = ParseLivingWorldCommand("0 profile 1");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, BotActionRejectsUnknownVerbAfterPosition)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 dance");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::UnknownVerb);
}

TEST(LivingWorldCommandGrammarTest, BotActionRejectsMissingVerbAfterName)
{
    ParsedCommand cmd = ParseLivingWorldCommand("Thrall");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::UnknownVerb);
}

// BotCastCommand tests

TEST(LivingWorldCommandGrammarTest, BotCastParsesPositionAndSingleWordSpell)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 cast Immolate");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&cast->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 2u);
    EXPECT_EQ(cast->spellName, "Immolate");
    EXPECT_FALSE(cast->targetName.has_value());
}

TEST(LivingWorldCommandGrammarTest, BotCastParsesNameAndSingleWordSpell)
{
    ParsedCommand cmd = ParseLivingWorldCommand("thrall cast Immolate");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    auto const* name = std::get_if<std::string>(&cast->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Thrall");
    EXPECT_EQ(cast->spellName, "Immolate");
    EXPECT_FALSE(cast->targetName.has_value());
}

TEST(LivingWorldCommandGrammarTest, QuestRewardsParses)
{
    ParsedCommand cmd = ParseLivingWorldCommand("quests");

    EXPECT_NE(std::get_if<QuestRewardsCommand>(&cmd), nullptr);
}

TEST(LivingWorldCommandGrammarTest, QuestRewardModeParsesSmart)
{
    ParsedCommand cmd = ParseLivingWorldCommand("questmode smart");

    auto const* mode = std::get_if<QuestRewardModeSetCommand>(&cmd);
    ASSERT_NE(mode, nullptr);
    EXPECT_TRUE(mode->smartMode);
}

TEST(LivingWorldCommandGrammarTest, QuestRewardModeParsesManual)
{
    ParsedCommand cmd = ParseLivingWorldCommand("questmode manual");

    auto const* mode = std::get_if<QuestRewardModeSetCommand>(&cmd);
    ASSERT_NE(mode, nullptr);
    EXPECT_FALSE(mode->smartMode);
}

TEST(LivingWorldCommandGrammarTest, BotRewardChoiceParses)
{
    ParsedCommand cmd = ParseLivingWorldCommand("thrall reward 8325 2");

    auto const* reward = std::get_if<BotRewardChoiceCommand>(&cmd);
    ASSERT_NE(reward, nullptr);
    auto const* name = std::get_if<std::string>(&reward->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Thrall");
    EXPECT_EQ(reward->questId, 8325u);
    EXPECT_EQ(reward->choiceNumber, 2u);
}

TEST(LivingWorldCommandGrammarTest, BotCastParsesMultiWordSpellName)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Death Strike");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    EXPECT_EQ(cast->spellName, "Death Strike");
    EXPECT_FALSE(cast->targetName.has_value());
}

TEST(LivingWorldCommandGrammarTest, BotCastParsesSpellWithOnTarget)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Holy Light on Arthas");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    EXPECT_EQ(cast->spellName, "Holy Light");
    ASSERT_TRUE(cast->targetName.has_value());
    EXPECT_EQ(*cast->targetName, "Arthas");
}

TEST(LivingWorldCommandGrammarTest, BotCastYourselfKeywordNormalized)
{
    // "yourself" → "Yourself": cast on the bot itself
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Immolate on yourself");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    ASSERT_TRUE(cast->targetName.has_value());
    EXPECT_EQ(*cast->targetName, "Yourself");
}

TEST(LivingWorldCommandGrammarTest, BotCastMeKeywordNormalized)
{
    // "me" → "Me": cast on the owner/player
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Holy Light on me");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    ASSERT_TRUE(cast->targetName.has_value());
    EXPECT_EQ(*cast->targetName, "Me");
}

TEST(LivingWorldCommandGrammarTest, BotCastMytargetKeywordNormalized)
{
    // "mytarget" → "Mytarget": cast on the owner's current target
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Immolate on mytarget");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    ASSERT_TRUE(cast->targetName.has_value());
    EXPECT_EQ(*cast->targetName, "Mytarget");
}

TEST(LivingWorldCommandGrammarTest, BotCastFocusKeywordNormalized)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Immolate on focus");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    ASSERT_TRUE(cast->targetName.has_value());
    EXPECT_EQ(*cast->targetName, "Focus");
}

TEST(LivingWorldCommandGrammarTest, BotCastNormalizesTargetNameCase)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Immolate on SYLVANAS");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    ASSERT_TRUE(cast->targetName.has_value());
    EXPECT_EQ(*cast->targetName, "Sylvanas");
}

TEST(LivingWorldCommandGrammarTest, BotCastPreservesSpellNameCase)
{
    // Spell names are case-preserved in the grammar (handler does case-insensitive match).
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast power word shield");

    auto const* cast = std::get_if<BotCastCommand>(&cmd);
    ASSERT_NE(cast, nullptr);
    EXPECT_EQ(cast->spellName, "power word shield");
}

TEST(LivingWorldCommandGrammarTest, BotCastRejectsMissingSpellName)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, BotCastRejectsMissingTargetAfterOn)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 cast Immolate on");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, BotEquipParsesPositionAndGuid)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 equip 12345");

    auto const* equip = std::get_if<BotEquipCommand>(&cmd);
    ASSERT_NE(equip, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&equip->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 2u);
    EXPECT_EQ(equip->itemGuidLow, 12345u);
}

TEST(LivingWorldCommandGrammarTest, BotRetrieveParsesOptionalCount)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 retrieve 12345 7");

    auto const* retrieve = std::get_if<BotRetrieveCommand>(&cmd);
    ASSERT_NE(retrieve, nullptr);
    auto const* position = std::get_if<std::uint32_t>(&retrieve->botRef);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(*position, 2u);
    EXPECT_EQ(retrieve->itemGuidLow, 12345u);
    ASSERT_TRUE(retrieve->itemCount.has_value());
    EXPECT_EQ(*retrieve->itemCount, 7u);
}

TEST(LivingWorldCommandGrammarTest, BotUnequipParsesNameAndGuid)
{
    ParsedCommand cmd = ParseLivingWorldCommand("thrall unequip 67890");

    auto const* unequip = std::get_if<BotUnequipCommand>(&cmd);
    ASSERT_NE(unequip, nullptr);
    auto const* name = std::get_if<std::string>(&unequip->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Thrall");
    EXPECT_EQ(unequip->itemGuidLow, 67890u);
}

TEST(LivingWorldCommandGrammarTest, BotUnequipRejectsMissingGuid)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 unequip");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, BotRetrieveRejectsZeroCount)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 retrieve 12345 0");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, BotUnequipRejectsNonNumericGuid)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 unequip abc");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}
TEST(LivingWorldCommandGrammarTest, QuestActionsParses)
{
    ParsedCommand cmd = ParseLivingWorldCommand("questactions");

    EXPECT_NE(std::get_if<QuestActionsCommand>(&cmd), nullptr);
}

TEST(LivingWorldCommandGrammarTest, TrainActionsParses)
{
    ParsedCommand cmd = ParseLivingWorldCommand("trainactions");

    EXPECT_NE(std::get_if<TrainActionsCommand>(&cmd), nullptr);
}

TEST(LivingWorldCommandGrammarTest, BotTrainSpellParsesNameAndSpellId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("Theron trainspell 1234");

    auto const* trainSpell = std::get_if<BotTrainSpellCommand>(&cmd);
    ASSERT_NE(trainSpell, nullptr);
    auto const* name = std::get_if<std::string>(&trainSpell->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Theron");
    EXPECT_EQ(trainSpell->trainerSpellId, 1234u);
}

TEST(LivingWorldCommandGrammarTest, BotTrainAllParsesPosition)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 trainall");

    auto const* trainAll = std::get_if<BotTrainAllCommand>(&cmd);
    ASSERT_NE(trainAll, nullptr);
    auto const* pos = std::get_if<std::uint32_t>(&trainAll->botRef);
    ASSERT_NE(pos, nullptr);
    EXPECT_EQ(*pos, 2u);
}

TEST(LivingWorldCommandGrammarTest, BotPickupParsesPositionAndQuestId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 pickup 783");

    auto const* pickup = std::get_if<BotQuestPickupCommand>(&cmd);
    ASSERT_NE(pickup, nullptr);
    auto const* pos = std::get_if<std::uint32_t>(&pickup->botRef);
    ASSERT_NE(pos, nullptr);
    EXPECT_EQ(*pos, 1u);
    EXPECT_EQ(pickup->questId, 783u);
}

TEST(LivingWorldCommandGrammarTest, BotPickupParsesNameAndQuestId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("Theron pickup 456");

    auto const* pickup = std::get_if<BotQuestPickupCommand>(&cmd);
    ASSERT_NE(pickup, nullptr);
    auto const* name = std::get_if<std::string>(&pickup->botRef);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "Theron");
    EXPECT_EQ(pickup->questId, 456u);
}

TEST(LivingWorldCommandGrammarTest, BotPickupRejectsMissingQuestId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 pickup");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, BotTurninParsesPositionAndQuestId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 turnin 100");

    auto const* turnin = std::get_if<BotQuestTurninCommand>(&cmd);
    ASSERT_NE(turnin, nullptr);
    auto const* pos = std::get_if<std::uint32_t>(&turnin->botRef);
    ASSERT_NE(pos, nullptr);
    EXPECT_EQ(*pos, 2u);
    EXPECT_EQ(turnin->questId, 100u);
}

TEST(LivingWorldCommandGrammarTest, BotTurninRejectsMissingQuestId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 turnin");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, BotTurninRejectsNonNumericQuestId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("1 turnin abc");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}
} // namespace script
} // namespace living_world
