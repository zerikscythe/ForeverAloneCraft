#include "script/LivingWorldCommandGrammar.h"

#include <cctype>
#include <charconv>
#include <limits>

namespace living_world
{
namespace script
{
namespace
{
CommandParseError MakeError(CommandParseErrorKind kind, std::string detail)
{
    CommandParseError error;
    error.kind = kind;
    error.detail = std::move(detail);
    return error;
}

// Trim ASCII whitespace from both ends. Chat input on 3.3.5a is byte
// oriented; multibyte-aware trimming would only add risk here.
std::string_view TrimWhitespace(std::string_view input)
{
    while (!input.empty() &&
           std::isspace(static_cast<unsigned char>(input.front())))
    {
        input.remove_prefix(1);
    }
    while (!input.empty() &&
           std::isspace(static_cast<unsigned char>(input.back())))
    {
        input.remove_suffix(1);
    }
    return input;
}

// Consume the next whitespace-delimited token. The remaining view is
// updated in place; returns an empty view when nothing is left.
std::string_view ConsumeToken(std::string_view& remaining)
{
    remaining = TrimWhitespace(remaining);
    if (remaining.empty())
    {
        return {};
    }

    std::size_t end = 0;
    while (end < remaining.size() &&
           !std::isspace(static_cast<unsigned char>(remaining[end])))
    {
        ++end;
    }

    std::string_view token = remaining.substr(0, end);
    remaining.remove_prefix(end);
    return token;
}

bool ParseUInt64(std::string_view token, std::uint64_t& out)
{
    if (token.empty())
    {
        return false;
    }
    std::uint64_t value = 0;
    auto result = std::from_chars(
        token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{} ||
        result.ptr != token.data() + token.size())
    {
        return false;
    }
    out = value;
    return true;
}

bool IsAlphaOnly(std::string_view token)
{
    if (token.empty())
        return false;
    for (char c : token)
        if (!std::isalpha(static_cast<unsigned char>(c)))
            return false;
    return true;
}

// First char upper, rest lower — mirrors WoW's enforced character name format.
std::string NormalizeCharacterName(std::string_view name)
{
    std::string out(name);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!out.empty())
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

// Parses a bot reference token into a BotRef (position or normalized name).
// Returns a CommandParseError variant on failure.
std::variant<BotRef, CommandParseError> ParseBotRef(std::string_view token)
{
    if (token.empty())
        return MakeError(CommandParseErrorKind::MissingArgument, "bot position or name required");

    if (std::isdigit(static_cast<unsigned char>(token.front())))
    {
        std::uint64_t position = 0;
        if (!ParseUInt64(token, position) ||
            position == 0 ||
            position > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "bot position must be a positive integer");
        }
        return BotRef { static_cast<std::uint32_t>(position) };
    }

    if (IsAlphaOnly(token))
        return BotRef { NormalizeCharacterName(token) };

    return MakeError(
        CommandParseErrorKind::InvalidArgument,
        "bot must be specified by roster position (number) or character name");
}

ParsedCommand ParseRosterVerb(
    std::string_view verb, std::string_view remaining)
{
    if (verb == "list")
        return RosterListCommand{};

    if (verb == "request" || verb == "dismiss")
    {
        std::string_view refToken = ConsumeToken(remaining);
        auto botRefResult = ParseBotRef(refToken);

        if (CommandParseError const* err = std::get_if<CommandParseError>(&botRefResult))
            return *err;

        BotRef botRef = std::get<BotRef>(std::move(botRefResult));

        if (verb == "request")
        {
            RosterRequestCommand cmd;
            cmd.botRef = std::move(botRef);
            return cmd;
        }

        RosterDismissCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    return MakeError(
        CommandParseErrorKind::UnknownVerb,
        std::string("unknown roster verb: ") + std::string(verb));
}

// Dispatches on the second token after a resolved bot reference:
//   "profile <1-10>"                          → BotProfileSetCommand
//   "cast <Ability Name> [on <target>]"       → BotCastCommand
//   "attack [<target>]"                       → BotAttackCommand
//   "disengage"                               → BotDisengageCommand
//   "reward <questId> <choiceNumber>"         -> BotRewardChoiceCommand
ParsedCommand ParseBotActionCommand(BotRef botRef, std::string_view remaining)
{
    std::string_view secondToken = ConsumeToken(remaining);

    if (secondToken.empty())
    {
        return MakeError(
            CommandParseErrorKind::UnknownVerb,
            "expected a verb like 'profile', 'cast', 'attack', 'pickup', 'turnin', etc. after the bot reference");
    }

    // "profile <slot>" path — unchanged.
    if (secondToken == "profile")
    {
        std::string_view slotToken = ConsumeToken(remaining);
        if (slotToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "profile slot required (1-10)");
        }

        std::uint64_t slot = 0;
        if (!ParseUInt64(slotToken, slot) || slot < 1 || slot > 10)
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "profile slot must be 1-10");
        }

        BotProfileSetCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.profileSlot = static_cast<std::uint8_t>(slot);
        return cmd;
    }

    // "cast <Ability Name> [on <target>]" path.
    if (secondToken == "cast")
    {
        // Collect ability name tokens until "on" (case-insensitive) or end.
        std::string spellName;
        std::optional<std::string> targetName;

        while (true)
        {
            std::string_view tok = ConsumeToken(remaining);
            if (tok.empty())
                break;

            // Case-insensitive "on" check — the target delimiter.
            bool isOn = tok.size() == 2 &&
                std::tolower(static_cast<unsigned char>(tok[0])) == 'o' &&
                std::tolower(static_cast<unsigned char>(tok[1])) == 'n';
            if (isOn)
            {
                std::string_view targetTok = ConsumeToken(remaining);
                if (targetTok.empty())
                {
                    return MakeError(
                        CommandParseErrorKind::MissingArgument,
                        "target required after 'on'");
                }
                targetName = NormalizeCharacterName(targetTok);
                break;
            }

            if (!spellName.empty())
                spellName += ' ';
            spellName += tok;
        }

        if (spellName.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "spell name required after 'cast'");
        }

        BotCastCommand cmd;
        cmd.botRef     = std::move(botRef);
        cmd.spellName  = std::move(spellName);
        cmd.targetName = std::move(targetName);
        return cmd;
    }

    // "attack [<target>]" path.
    if (secondToken == "attack")
    {
        BotAttackCommand cmd;
        cmd.botRef = std::move(botRef);
        std::string_view targetTok = ConsumeToken(remaining);
        if (!targetTok.empty())
            cmd.targetName = NormalizeCharacterName(targetTok);
        return cmd;
    }

    // "disengage" path.
    if (secondToken == "disengage")
    {
        BotDisengageCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "train" path.
    if (secondToken == "train")
    {
        BotTrainCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "retreat" path.
    if (secondToken == "retreat")
    {
        BotRetreatCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "follow" path.
    if (secondToken == "follow")
    {
        BotFollowCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "yoink" path.
    if (secondToken == "yoink")
    {
        BotYoinkCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "refreshments" path.
    if (secondToken == "refreshments")
    {
        BotRefreshmentsCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "buff" path.
    if (secondToken == "buff")
    {
        BotBuffCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "bags" path — no arguments needed.
    if (secondToken == "bags")
    {
        BotBagsCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    // "retrieve" path — requires item guid low as numeric argument.
    if (secondToken == "retrieve")
    {
        std::string_view thirdToken = ConsumeToken(remaining);
        if (thirdToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "retrieve requires an item guid");
        }

        std::uint64_t guidRaw = 0;
        if (!ParseUInt64(thirdToken, guidRaw) ||
            guidRaw > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "retrieve requires a numeric item guid");
        }

        BotRetrieveCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.itemGuidLow = static_cast<std::uint32_t>(guidRaw);
        std::string_view fourthToken = ConsumeToken(remaining);
        if (!fourthToken.empty())
        {
            std::uint64_t countRaw = 0;
            if (!ParseUInt64(fourthToken, countRaw) ||
                countRaw == 0 ||
                countRaw > std::numeric_limits<std::uint32_t>::max())
            {
                return MakeError(
                    CommandParseErrorKind::InvalidArgument,
                    "retrieve count must be a positive integer");
            }

            cmd.itemCount = static_cast<std::uint32_t>(countRaw);
        }
        return cmd;
    }

    // "equip"/"unequip" path — requires item guid low as numeric argument.
    if (secondToken == "equip" || secondToken == "unequip")
    {
        std::string_view thirdToken = ConsumeToken(remaining);
        if (thirdToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                secondToken == "equip"
                    ? "equip requires an item guid"
                    : "unequip requires an item guid");
        }

        std::uint64_t guidRaw = 0;
        if (!ParseUInt64(thirdToken, guidRaw) ||
            guidRaw > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                secondToken == "equip"
                    ? "equip requires a numeric item guid"
                    : "unequip requires a numeric item guid");
        }

        if (secondToken == "equip")
        {
            BotEquipCommand cmd;
            cmd.botRef      = std::move(botRef);
            cmd.itemGuidLow = static_cast<std::uint32_t>(guidRaw);
            return cmd;
        }

        BotUnequipCommand cmd;
        cmd.botRef      = std::move(botRef);
        cmd.itemGuidLow = static_cast<std::uint32_t>(guidRaw);
        return cmd;
    }

    if (secondToken == "pickup")
    {
        std::string_view questToken = ConsumeToken(remaining);
        if (questToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "pickup requires a quest id");
        }

        std::uint64_t questIdRaw = 0;
        if (!ParseUInt64(questToken, questIdRaw) ||
            questIdRaw > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "pickup requires a numeric quest id");
        }

        BotQuestPickupCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.questId = static_cast<std::uint32_t>(questIdRaw);
        return cmd;
    }

    if (secondToken == "turnin")
    {
        std::string_view questToken = ConsumeToken(remaining);
        if (questToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "turnin requires a quest id");
        }

        std::uint64_t questIdRaw = 0;
        if (!ParseUInt64(questToken, questIdRaw) ||
            questIdRaw > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "turnin requires a numeric quest id");
        }

        BotQuestTurninCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.questId = static_cast<std::uint32_t>(questIdRaw);
        return cmd;
    }

    if (secondToken == "trainspell")
    {
        std::string_view spellToken = ConsumeToken(remaining);
        if (spellToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "trainspell requires a trainer spell id");
        }

        std::uint64_t spellIdRaw = 0;
        if (!ParseUInt64(spellToken, spellIdRaw) ||
            spellIdRaw > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "trainspell requires a numeric trainer spell id");
        }

        BotTrainSpellCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.trainerSpellId = static_cast<std::uint32_t>(spellIdRaw);
        return cmd;
    }

    if (secondToken == "trainall")
    {
        BotTrainAllCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    if (secondToken == "reward")
    {
        std::string_view questToken = ConsumeToken(remaining);
        if (questToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "reward requires a quest id");
        }

        std::uint64_t questIdRaw = 0;
        if (!ParseUInt64(questToken, questIdRaw) ||
            questIdRaw > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "reward requires a numeric quest id");
        }

        std::string_view choiceToken = ConsumeToken(remaining);
        if (choiceToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "reward requires a choice number");
        }

        std::uint64_t choiceRaw = 0;
        if (!ParseUInt64(choiceToken, choiceRaw) ||
            choiceRaw == 0 ||
            choiceRaw > std::numeric_limits<std::uint8_t>::max())
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "reward choice number must be a positive integer");
        }

        BotRewardChoiceCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.questId = static_cast<std::uint32_t>(questIdRaw);
        cmd.choiceNumber = static_cast<std::uint8_t>(choiceRaw);
        return cmd;
    }

    // "mode assist|passive|hold|guard" path.
    if (secondToken == "mode")
    {
        std::string_view modeTok = ConsumeToken(remaining);
        if (modeTok.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "mode required: assist, passive, hold, or guard");
        }

        model::BotCombatMode mode;
        if (modeTok == "assist")
            mode = model::BotCombatMode::Assist;
        else if (modeTok == "passive")
            mode = model::BotCombatMode::Passive;
        else if (modeTok == "hold")
            mode = model::BotCombatMode::Hold;
        else if (modeTok == "guard")
            mode = model::BotCombatMode::Guard;
        else
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                std::string("unknown mode '") + std::string(modeTok) +
                    "'; expected assist, passive, hold, or guard");

        BotModeSetCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.mode   = mode;
        return cmd;
    }

    if (secondToken == "info")
    {
        BotInfoCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    return MakeError(
        CommandParseErrorKind::UnknownVerb,
        std::string("expected 'profile', 'cast', 'attack', 'disengage', 'train', 'retreat', "
                    "'follow', 'refreshments', 'buff', 'bags', 'retrieve', 'equip', 'unequip', "
                    "'pickup', 'turnin', 'trainspell', 'trainall', 'reward', 'mode', or 'info', got: ") +
            std::string(secondToken));
}
} // namespace

ParsedCommand ParseLivingWorldCommand(std::string_view arguments)
{
    std::string_view remaining = TrimWhitespace(arguments);
    if (remaining.empty())
    {
        return MakeError(CommandParseErrorKind::Empty, "no command given");
    }

    std::string_view firstToken = ConsumeToken(remaining);
    if (firstToken == "roster")
    {
        std::string_view verb = ConsumeToken(remaining);
        if (verb.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "roster verb required (list/request/dismiss)");
        }

        return ParseRosterVerb(verb, remaining);
    }

    // Support the shorter `.lwbot list|request|dismiss` forms in addition to
    // the original `.lwbot roster ...` grammar. This matches how the command
    // is being used in game and keeps the parser addon-friendly by treating
    // `roster` as an optional first subsystem marker for now.
    if (firstToken == "list" ||
        firstToken == "request" ||
        firstToken == "dismiss")
    {
        return ParseRosterVerb(firstToken, remaining);
    }

    if (firstToken == "questactions")
    {
        return QuestActionsCommand{};
    }

    if (firstToken == "trainactions")
    {
        return TrainActionsCommand{};
    }

    if (firstToken == "quests")
    {
        return QuestRewardsCommand{};
    }

    if (firstToken == "questmode")
    {
        std::string_view modeToken = ConsumeToken(remaining);
        if (modeToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "questmode requires 'smart' or 'manual'");
        }

        if (modeToken == "smart")
        {
            QuestRewardModeSetCommand cmd;
            cmd.smartMode = true;
            return cmd;
        }

        if (modeToken == "manual")
        {
            QuestRewardModeSetCommand cmd;
            cmd.smartMode = false;
            return cmd;
        }

        return MakeError(
            CommandParseErrorKind::InvalidArgument,
            "questmode must be 'smart' or 'manual'");
    }

    // `.lwbot <position> profile <slot>` — digit-leading token is always a position.
    if (std::isdigit(static_cast<unsigned char>(firstToken.front())))
    {
        auto botRefResult = ParseBotRef(firstToken);
        if (CommandParseError* err = std::get_if<CommandParseError>(&botRefResult))
            return *err;
        return ParseBotActionCommand(
            std::get<BotRef>(std::move(botRefResult)), remaining);
    }

    // `.lwbot <name> profile <slot>` — all-alpha token is a character name.
    if (IsAlphaOnly(firstToken))
        return ParseBotActionCommand(
            BotRef { NormalizeCharacterName(firstToken) }, remaining);

    return MakeError(
        CommandParseErrorKind::UnknownSubsystem,
        std::string("unknown subsystem: ") + std::string(firstToken));
}
} // namespace script
} // namespace living_world
