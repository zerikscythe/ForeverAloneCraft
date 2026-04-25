#pragma once

// Pure parser for the `.lwbot ...` command surface. The AzerothCore
// CommandScript that actually registers chat commands is a future slice -
// it will call into this parser, then hand the resulting command to the
// service layer. Keeping parsing out of the script file means we can unit
// test the grammar without the server and re-use the same grammar from a
// future in-game addon message channel.
//
// Supported forms in this slice:
//   .lwbot roster list
//   .lwbot roster request <rosterEntryId>
//   .lwbot roster dismiss <rosterEntryId>
//   .lwbot <position|name> profile <1-10>
//
// The parser intentionally produces a structured command object rather
// than executing anything. Parse errors are returned as a dedicated
// variant alternative so callers can render a precise error message
// without exception handling.

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace living_world
{
namespace script
{
// "list" — ask the service what roster entries are available to the
// requesting player. No arguments.
struct RosterListCommand
{
};

// "request" — ask the service to dispatch a roster request by ID.
struct RosterRequestCommand
{
    std::uint64_t rosterEntryId = 0;
};

// "dismiss" — future commit path for tearing down an active body.
struct RosterDismissCommand
{
    std::uint64_t rosterEntryId = 0;
};

// "<position|name> profile <slot>" — switch a bot's active moveset profile.
// botRef holds either a 1-N roster position (uint32) or a normalized character
// name (string, first char upper rest lower). profileSlot is 1-10.
struct BotProfileSetCommand
{
    std::variant<std::uint32_t, std::string> botRef;
    std::uint8_t profileSlot = 1;
};

// Reasons parsing can fail. Kept coarse so the UX can render one message
// per reason; fine-grained diagnostics go in the trailing `detail` field.
enum class CommandParseErrorKind : std::uint8_t
{
    Empty,
    UnknownSubsystem,
    UnknownVerb,
    MissingArgument,
    InvalidArgument
};

struct CommandParseError
{
    CommandParseErrorKind kind = CommandParseErrorKind::Empty;
    std::string detail;
};

using ParsedCommand = std::variant<
    CommandParseError,
    RosterListCommand,
    RosterRequestCommand,
    RosterDismissCommand,
    BotProfileSetCommand>;

// Parse a raw command argument string (everything after `.lwbot `). The
// input is expected to be trimmed of the command prefix but may still
// contain leading/trailing whitespace and multiple spaces between tokens.
ParsedCommand ParseLivingWorldCommand(std::string_view arguments);
} // namespace script
} // namespace living_world
