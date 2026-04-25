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

TEST(LivingWorldCommandGrammarTest, ProfileSetRejectsWrongVerbAfterPosition)
{
    ParsedCommand cmd = ParseLivingWorldCommand("2 spell 3");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::UnknownVerb);
}

TEST(LivingWorldCommandGrammarTest, ProfileSetRejectsMissingVerbAfterName)
{
    ParsedCommand cmd = ParseLivingWorldCommand("Thrall");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::UnknownVerb);
}
} // namespace script
} // namespace living_world
