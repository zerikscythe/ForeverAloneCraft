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
    ParsedCommand cmd = ParseLivingWorldCommand("economy list");

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

TEST(LivingWorldCommandGrammarTest, RosterRequestParsesId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request 42");

    auto const* request = std::get_if<RosterRequestCommand>(&cmd);
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->rosterEntryId, 42u);
}

TEST(LivingWorldCommandGrammarTest, ShortRequestParsesIdWithoutSubsystem)
{
    ParsedCommand cmd = ParseLivingWorldCommand("request 42");

    auto const* request = std::get_if<RosterRequestCommand>(&cmd);
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->rosterEntryId, 42u);
}

TEST(LivingWorldCommandGrammarTest, RosterDismissParsesId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("  roster   dismiss   7 ");

    auto const* dismiss = std::get_if<RosterDismissCommand>(&cmd);
    ASSERT_NE(dismiss, nullptr);
    EXPECT_EQ(dismiss->rosterEntryId, 7u);
}

TEST(LivingWorldCommandGrammarTest, ShortDismissParsesIdWithoutSubsystem)
{
    ParsedCommand cmd = ParseLivingWorldCommand("dismiss 7");

    auto const* dismiss = std::get_if<RosterDismissCommand>(&cmd);
    ASSERT_NE(dismiss, nullptr);
    EXPECT_EQ(dismiss->rosterEntryId, 7u);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestRejectsMissingId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::MissingArgument);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestRejectsZeroId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request 0");

    auto const* error = std::get_if<CommandParseError>(&cmd);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->kind, CommandParseErrorKind::InvalidArgument);
}

TEST(LivingWorldCommandGrammarTest, RosterRequestRejectsNonNumericId)
{
    ParsedCommand cmd = ParseLivingWorldCommand("roster request abc");

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
} // namespace script
} // namespace living_world
