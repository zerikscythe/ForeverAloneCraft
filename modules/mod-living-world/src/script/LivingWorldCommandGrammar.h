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

// "<position|name|party> attack" — lock bot onto owner's current target
// or the named target if provided.
struct BotAttackCommand
{
    BotRef botRef; // "party" resolves to all bots
    std::optional<std::string> targetName; // nullopt = owner's current target
};

// "<position|name|party> disengage" — stop combat and return to follow.
struct BotDisengageCommand
{
    BotRef botRef; // "party" resolves to all bots
};

// "<position|name|party> train" — while at a class trainer, teaches every
// available spell up to the bot's level and deducts the gold cost from the
// owner. Use "party" to train all active bots in one command.
struct BotTrainCommand
{
    BotRef botRef;
};

// "<position|name|party> retreat" — all bots stop combat, follow the owner,
// and only cast instant heals for 30 seconds. Issuing the command again
// during the countdown cancels the retreat early.
struct BotRetreatCommand
{
    BotRef botRef;
};

// "<position|name|party> follow" — cancel combat and return to follow immediately.
struct BotFollowCommand
{
    BotRef botRef;
};

// "<position|name|party> refreshments" — each bot consumes food if HP < 60%
// and/or drink if mana < 60%.
struct BotRefreshmentsCommand
{
    BotRef botRef;
};

// "<position|name|party> buff" — force each bot to re-apply its out-of-combat
// maintenance buffs regardless of combat state.
struct BotBuffCommand
{
    BotRef botRef;
};

// "bags" — request bot inventory data sent back to the requesting player's
// client as an addon message (LWBOT prefix).
struct BotBagsCommand
{
    BotRef botRef;
};

// "retrieve" — move a specific item (identified by its GUID low) from the
// bot's inventory or equipped slots into the owner's inventory.
struct BotRetrieveCommand
{
    BotRef botRef;
    std::uint32_t itemGuidLow;
    std::optional<std::uint32_t> itemCount;
};

// "equip" — equip an item (by GUID low) from the bot's bags onto the bot.
// If the destination slot is occupied, the existing item is moved to bags first.
struct BotEquipCommand
{
    BotRef botRef;
    std::uint32_t itemGuidLow;
};

// "unequip" — move an equipped item (by GUID low) from the bot's body into
// the bot's bags.
struct BotUnequipCommand
{
    BotRef botRef;
    std::uint32_t itemGuidLow;
};

using ParsedCommand = std::variant<
    CommandParseError,
    RosterListCommand,
    RosterRequestCommand,
    RosterDismissCommand,
    BotProfileSetCommand,
    BotCastCommand,
    BotAttackCommand,
    BotDisengageCommand,
    BotTrainCommand,
    BotRetreatCommand,
    BotFollowCommand,
    BotRefreshmentsCommand,
    BotBuffCommand,
    BotBagsCommand,
    BotRetrieveCommand,
    BotEquipCommand,
    BotUnequipCommand>;

// Parse a raw command argument string (everything after `.lwbot `). The
// input is expected to be trimmed of the command prefix but may still
// contain leading/trailing whitespace and multiple spaces between tokens.
ParsedCommand ParseLivingWorldCommand(std::string_view arguments);
} // namespace script
} // namespace living_world
