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
//   .lwbot combat strict|smart
//   .lwbot <position|name> profile <1-10>
//   .lwbot <position|name> cast <Ability Name> [on <target>]
//   .lwbot <position|name> mode assist|passive|hold|stay|guard
//   .lwbot <position|name> stay
//
// The parser intentionally produces a structured command object rather
// than executing anything. Parse errors are returned as a dedicated
// variant alternative so callers can render a precise error message
// without exception handling.

#include "model/BotCombatMode.h"
#include "model/BotCombatControlMode.h"

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

// "<position|name|party> yoink" — teleport a bot or the whole active bot
// party to the owner's current position. Intended as a recovery command for
// stuck/fallen bots.
struct BotYoinkCommand
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

// "bagmove" — move an item (by GUID low) into a target bag container on the
// bot. bagIndex 0 means backpack, 1-4 are equipped bag slots.
struct BotBagMoveCommand
{
    BotRef botRef;
    std::uint32_t itemGuidLow;
    std::uint8_t bagIndex;
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

// "questactions" — query the owner's current target (quest giver NPC) for
// bot-specific quest pick-up and turn-in opportunities.
struct QuestActionsCommand
{
};

// "<position|name> pickup <questId>" — have a bot accept a specific quest
// from the owner's targeted quest giver.
struct BotQuestPickupCommand
{
    BotRef botRef;
    std::uint32_t questId = 0;
};

// "<position|name> turnin <questId>" — have a bot turn in a completed quest
// to the owner's targeted quest giver.
struct BotQuestTurninCommand
{
    BotRef botRef;
    std::uint32_t questId = 0;
};

// "trainactions" — query the owner's current target (trainer NPC) for
// bot-specific spell training opportunities.
struct TrainActionsCommand
{
};

// "<position|name> trainspell <trainerSpellId>" — have a bot learn a single
// spell from the owner's targeted trainer NPC.
struct BotTrainSpellCommand
{
    BotRef botRef;
    std::uint32_t trainerSpellId = 0;
};

// "<position|name> trainall" — have a bot learn all available spells from the
// owner's targeted trainer NPC.
struct BotTrainAllCommand
{
    BotRef botRef;
};

// "quests" - request the current pending bot quest reward choices plus the
// owner's current smart/manual reward mode.
struct QuestRewardsCommand
{
};

// "questmode <smart|manual>" - change the default reward mode used when the
// owner turns in a quest for active bots.
struct QuestRewardModeSetCommand
{
    bool smartMode = true;
};

// "<position|name> reward <questId> <choiceNumber>" - manually reward a
// completed bot quest using the selected 1-based reward choice number.
struct BotRewardChoiceCommand
{
    BotRef botRef;
    std::uint32_t questId = 0;
    std::uint8_t choiceNumber = 0;
};

// "<position|name> addtalent <Talent Name> <points>" — add one or more
// talent points to a named talent on a bot. talentName is the spell name
// of the talent's first rank (multi-word names are supported). points is
// how many ranks to add (1-5). All prerequisite and available-point checks
// are enforced server-side in the handler.
struct BotAddTalentCommand
{
    BotRef botRef;
    std::string talentName;
    std::uint8_t points = 1;
};

// "<position|name> resettalents" — refund all talent points on the bot's
// active spec, restoring free talent points to their level-appropriate cap.
struct BotResetTalentsCommand
{
    BotRef botRef;
};

// "<position|name> applytalent [reset]"
// Spend all available free talent points using the bot's preferred template.
// reset=true: reset all talents first, then fill the full build.
struct BotApplyTalentTemplateCommand
{
    BotRef botRef;
    bool resetFirst = false;
};

// "<position|name> favoritetalent [<specKey>|auto]"
// No arg or "auto" → report the current preferred template (or auto-detected spec).
// <specKey>         → pin the preferred template to the named spec.
struct BotTalentFavoriteCommand
{
    BotRef botRef;
    std::optional<std::string> specKey; // nullopt = query current setting
};

// "<position|name> mode assist|passive|hold|guard"
//
// Changes the bot's combat stance immediately. Stored in BotPlayerRegistry
// and consulted at the top of every CompanionAI tick.
struct BotModeSetCommand
{
    BotRef botRef;
    model::BotCombatMode mode = model::BotCombatMode::Assist;
};

// "combat strict|smart"
//
// Changes the owner-wide combat control layer for companion/account bots.
// Strict preserves old obedient assist behavior. Smart allows doctrine-aware
// target arbitration when there is no explicit player command override.
struct BotCombatControlModeSetCommand
{
    model::BotCombatControlMode mode = model::BotCombatControlMode::Strict;
};

// "<position|name> info" — request the bot's current config state pushed back
// as a LWBT:BINFO system message so the Bot-Tune addon can render it.
struct BotInfoCommand
{
    BotRef botRef;
};

// "raid request <class> <spec_role> <level> <min_ilvl>"
// Pull a matching server-pool bot, spawn at player, add to raid group.
// class    : warrior | paladin | hunter | rogue | priest | dk | shaman | mage | warlock | druid
// spec_role: tank | healer | dps
// level    : 1-80
// min_ilvl : 0-N (average item level floor)
struct RaidRequestCommand
{
    std::uint8_t  classId  = 0;
    std::string   specRole;       // "tank", "healer", "dps"
    std::uint8_t  minLevel = 1;
    std::uint16_t minIlvl  = 0;
};

// "raid dismiss <name|#>" — log out a raid pool bot and return it to the pool.
struct RaidDismissCommand
{
    BotRef botRef;
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
    BotYoinkCommand,
    BotRefreshmentsCommand,
    BotBuffCommand,
    BotBagsCommand,
    BotRetrieveCommand,
    BotBagMoveCommand,
    BotEquipCommand,
    BotUnequipCommand,
    QuestActionsCommand,
    BotQuestPickupCommand,
    BotQuestTurninCommand,
    TrainActionsCommand,
    BotTrainSpellCommand,
    BotTrainAllCommand,
    QuestRewardsCommand,
    QuestRewardModeSetCommand,
    BotRewardChoiceCommand,
    BotModeSetCommand,
    BotCombatControlModeSetCommand,
    BotInfoCommand,
    BotAddTalentCommand,
    BotResetTalentsCommand,
    BotApplyTalentTemplateCommand,
    BotTalentFavoriteCommand,
    RaidRequestCommand,
    RaidDismissCommand>;


// Parse a raw command argument string (everything after `.lwbot `). The
// input is expected to be trimmed of the command prefix but may still
// contain leading/trailing whitespace and multiple spaces between tokens.
ParsedCommand ParseLivingWorldCommand(std::string_view arguments);
} // namespace script
} // namespace living_world
