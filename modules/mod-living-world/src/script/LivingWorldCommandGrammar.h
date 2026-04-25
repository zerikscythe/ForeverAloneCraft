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
//   .lwbot <position|name> cast <Ability Name> [on <target>]
//
// The parser intentionally produces a structured command object rather
// than executing anything. Parse errors are returned as a dedicated
// variant alternative so callers can render a precise error message
// without exception handling.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace living_world
{
namespace script
{
// Identifies a bot by either its 1-N roster position or its normalized
// character name (first char upper, rest lower — WoW's enforced format).
using BotRef = std::variant<std::uint32_t, std::string>;

// "list" — ask the service what roster entries are available to the
// requesting player. No arguments.
struct RosterListCommand
{
};

// "request" — ask the service to dispatch a roster request.
struct RosterRequestCommand
{
    BotRef botRef;
};

// "dismiss" — tear down an active bot session.
struct RosterDismissCommand
{
    BotRef botRef;
};

// "<position|name> profile <slot>" — switch a bot's active moveset profile.
// profileSlot is 1-10.
struct BotProfileSetCommand
{
    BotRef botRef;
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

// "<position|name> cast <Ability Name> [on <target>]"
//
// Instructs an active bot to cast a named ability. The spell name is
// whitespace-joined from all tokens between "cast" and the optional "on"
// keyword, so multi-word names like "Death Strike" and "Holy Light" work
// naturally.
//
// targetName drives where the cast is aimed (after NormalizeCharacterName):
//   nullopt      → self-cast on the bot (no "on" clause)
//   "Yourself"   → explicit self-cast on the bot
//   "Me"         → the owner/player giving the command
//   "Mytarget"   → whatever the owner currently has targeted (any unit)
//   "Focus"      → the owner's focus target
//   anything else→ a character name looked up online
struct BotCastCommand
{
    BotRef botRef;
    std::string spellName;                 // as typed, case-preserved
    std::optional<std::string> targetName; // nullopt = self-cast
};

using ParsedCommand = std::variant<
    CommandParseError,
    RosterListCommand,
    RosterRequestCommand,
    RosterDismissCommand,
    BotProfileSetCommand,
    BotCastCommand>;

// Parse a raw command argument string (everything after `.lwbot `). The
// input is expected to be trimmed of the command prefix but may still
// contain leading/trailing whitespace and multiple spaces between tokens.
ParsedCommand ParseLivingWorldCommand(std::string_view arguments);
} // namespace script
} // namespace living_world
