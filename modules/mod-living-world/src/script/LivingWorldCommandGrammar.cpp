#include "script/LivingWorldCommandGrammar.h"

#include <cctype>
#include <charconv>
#include <limits>
#include <vector>

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
    {
        RosterListCommand cmd;
        return cmd;
    }

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

std::string ToLowerCopy(std::string_view value)
{
    std::string out(value);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool TryParseLevelRangeToken(
    std::string_view token,
    std::uint8_t& minLevel,
    std::uint8_t& maxLevel)
{
    if (token.empty())
        return false;

    std::size_t const colon = token.find(':');
    if (colon == std::string_view::npos)
    {
        std::uint64_t levelRaw = 0;
        if (!ParseUInt64(token, levelRaw) || levelRaw < 1 || levelRaw > 80)
            return false;
        minLevel = static_cast<std::uint8_t>(levelRaw);
        maxLevel = static_cast<std::uint8_t>(levelRaw);
        return true;
    }

    std::string_view left = token.substr(0, colon);
    std::string_view right = token.substr(colon + 1);
    std::uint64_t minRaw = 0;
    std::uint64_t maxRaw = 0;
    if (left.empty() || right.empty() ||
        !ParseUInt64(left, minRaw) ||
        !ParseUInt64(right, maxRaw) ||
        minRaw < 1 || minRaw > 80 ||
        maxRaw < 1 || maxRaw > 80 ||
        minRaw > maxRaw)
    {
        return false;
    }

    minLevel = static_cast<std::uint8_t>(minRaw);
    maxLevel = static_cast<std::uint8_t>(maxRaw);
    return true;
}

ParsedCommand ParseListScope(std::string_view remaining)
{
    RosterListCommand cmd;

    std::string_view scopeToken = ConsumeToken(remaining);
    if (scopeToken.empty())
        return cmd;

    std::string const scope = ToLowerCopy(scopeToken);
    if (scope == "party")
        cmd.scope = RosterListCommand::Scope::Party;
    else if (scope == "raid")
        cmd.scope = RosterListCommand::Scope::Raid;
    else if (scope == "zone")
        cmd.scope = RosterListCommand::Scope::Zone;
    else if (scope == "world")
        cmd.scope = RosterListCommand::Scope::World;
    else
    {
        return MakeError(
            CommandParseErrorKind::InvalidArgument,
            "list scope must be party, raid, zone, or world");
    }

    while (true)
    {
        std::string_view token = ConsumeToken(remaining);
        if (token.empty())
            break;

        std::string const lowered = ToLowerCopy(token);
        std::uint8_t rangeMin = 0;
        std::uint8_t rangeMax = 0;
        if (TryParseLevelRangeToken(token, rangeMin, rangeMax))
        {
            cmd.minLevel = rangeMin;
            cmd.maxLevel = rangeMax;
            continue;
        }

        auto const classTokenToId = [](std::string_view name) -> std::uint8_t
        {
            if (name == "warrior")                       return 1;
            if (name == "paladin")                       return 2;
            if (name == "hunter")                        return 3;
            if (name == "rogue")                         return 4;
            if (name == "priest")                        return 5;
            if (name == "dk" || name == "deathknight")   return 6;
            if (name == "shaman")                        return 7;
            if (name == "mage")                          return 8;
            if (name == "warlock")                       return 9;
            if (name == "druid")                         return 11;
            return 0;
        };

        std::uint8_t const classId = classTokenToId(lowered);
        if (classId != 0)
        {
            cmd.classId = classId;
            continue;
        }

        cmd.specKey = lowered;
    }

    return cmd;
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

    // "stay" path -> alias for mode hold.
    if (secondToken == "stay")
    {
        BotModeSetCommand cmd;
        cmd.botRef = std::move(botRef);
        cmd.mode = model::BotCombatMode::Hold;
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

    // "mode assist|passive|hold|stay|guard" path.
    if (secondToken == "mode")
    {
        std::string_view modeTok = ConsumeToken(remaining);
        if (modeTok.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "mode required: assist, passive, hold, stay, or guard");
        }

        model::BotCombatMode mode;
        if (modeTok == "assist")
            mode = model::BotCombatMode::Assist;
        else if (modeTok == "passive")
            mode = model::BotCombatMode::Passive;
        else if (modeTok == "hold" || modeTok == "stay")
            mode = model::BotCombatMode::Hold;
        else if (modeTok == "guard")
            mode = model::BotCombatMode::Guard;
        else
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                std::string("unknown mode '") + std::string(modeTok) +
                    "'; expected assist, passive, hold, stay, or guard");

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

    if (secondToken == "addtalent")
    {
        // Consume all remaining tokens. The last token is the point count;
        // everything before it is the talent name (multi-word supported).
        std::string_view next = ConsumeToken(remaining);
        if (next.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "addtalent requires a talent name and point count");
        }

        // Collect tokens; the final one must be a number.
        std::vector<std::string_view> parts;
        while (!next.empty())
        {
            parts.push_back(next);
            next = ConsumeToken(remaining);
        }

        if (parts.size() < 2)
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "addtalent requires both a talent name and a point count");
        }

        // Last part is the point count.
        std::uint64_t pointsRaw = 0;
        if (!ParseUInt64(parts.back(), pointsRaw) || pointsRaw == 0 ||
            pointsRaw > 5)
        {
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "addtalent point count must be 1-5");
        }

        // All other parts form the talent name.
        std::string talentName;
        for (std::size_t i = 0; i + 1 < parts.size(); ++i)
        {
            if (i > 0)
                talentName += ' ';
            talentName += std::string(parts[i]);
        }

        BotAddTalentCommand cmd;
        cmd.botRef     = std::move(botRef);
        cmd.talentName = std::move(talentName);
        cmd.points     = static_cast<std::uint8_t>(pointsRaw);
        return cmd;
    }

    if (secondToken == "resettalents")
    {
        BotResetTalentsCommand cmd;
        cmd.botRef = std::move(botRef);
        return cmd;
    }

    if (secondToken == "applytalent")
    {
        BotApplyTalentTemplateCommand cmd;
        cmd.botRef = std::move(botRef);
        std::string_view opt = ConsumeToken(remaining);
        cmd.resetFirst = (opt == "reset");
        return cmd;
    }

    if (secondToken == "favoritetalent")
    {
        BotTalentFavoriteCommand cmd;
        cmd.botRef = std::move(botRef);
        std::string_view opt = ConsumeToken(remaining);
        if (!opt.empty() && opt != "auto")
            cmd.specKey = std::string(opt);
        return cmd;
    }

    return MakeError(
        CommandParseErrorKind::UnknownVerb,
        std::string("expected 'profile', 'cast', 'attack', 'disengage', 'train', 'retreat', "
                    "'follow', 'stay', 'refreshments', 'buff', 'bags', 'retrieve', 'equip', 'unequip', "
                    "'pickup', 'turnin', 'trainspell', 'trainall', 'reward', 'mode', 'info', "
                    "'addtalent', 'resettalents', 'applytalent', or 'favoritetalent', got: ") +
            std::string(secondToken));
}
} // namespace

// Maps a lowercase class name token to the WoW class ID used in the DB.
// Returns 0 for unknown names so the caller can emit a clean error.
std::uint8_t ClassNameToId(std::string_view name)
{
    if (name == "warrior")                       return 1;
    if (name == "paladin")                       return 2;
    if (name == "hunter")                        return 3;
    if (name == "rogue")                         return 4;
    if (name == "priest")                        return 5;
    if (name == "dk" || name == "deathknight")   return 6;
    if (name == "shaman")                        return 7;
    if (name == "mage")                          return 8;
    if (name == "warlock")                       return 9;
    if (name == "druid")                         return 11;
    return 0;
}

ParsedCommand ParseRaidVerb(
    std::string_view verb, std::string_view remaining)
{
    if (verb == "request")
    {
        std::string_view classToken = ConsumeToken(remaining);
        if (classToken.empty())
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "class required (warrior/paladin/hunter/rogue/priest/dk/"
                "shaman/mage/warlock/druid)");

        std::uint8_t const classId = ClassNameToId(classToken);
        if (classId == 0)
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                std::string("unknown class: ") + std::string(classToken));

        std::string_view specToken = ConsumeToken(remaining);
        if (specToken.empty())
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "spec/role required (tank/healer/dps)");
        if (specToken != "tank" && specToken != "healer" && specToken != "dps")
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "spec must be tank, healer, or dps");

        std::string_view levelToken = ConsumeToken(remaining);
        std::uint64_t level = 0;
        if (levelToken.empty() || !ParseUInt64(levelToken, level)
            || level < 1 || level > 80)
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "level must be 1-80");

        std::string_view ilvlToken = ConsumeToken(remaining);
        std::uint64_t ilvl = 0;
        if (ilvlToken.empty() || !ParseUInt64(ilvlToken, ilvl))
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "min_ilvl must be a number");

        RaidRequestCommand cmd;
        cmd.classId  = classId;
        cmd.specRole = std::string(specToken);
        cmd.minLevel = static_cast<std::uint8_t>(level);
        cmd.minIlvl  = static_cast<std::uint16_t>(ilvl > 65535 ? 65535 : ilvl);
        return cmd;
    }

    if (verb == "dismiss")
    {
        std::string_view refToken = ConsumeToken(remaining);
        auto botRefResult = ParseBotRef(refToken);
        if (CommandParseError const* err =
                std::get_if<CommandParseError>(&botRefResult))
            return *err;
        RaidDismissCommand cmd;
        cmd.botRef = std::get<BotRef>(std::move(botRefResult));
        return cmd;
    }

    return MakeError(
        CommandParseErrorKind::UnknownVerb,
        std::string("unknown raid verb: ") + std::string(verb));
}

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

    if (firstToken == "raid")
    {
        std::string_view verb = ConsumeToken(remaining);
        if (verb.empty())
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "raid verb required (request/dismiss)");
        return ParseRaidVerb(verb, remaining);
    }

    // Support the shorter `.lwbot list|request|dismiss` forms in addition to
    // the original `.lwbot roster ...` grammar. This matches how the command
    // is being used in game and keeps the parser addon-friendly by treating
    // `roster` as an optional first subsystem marker for now.
    if (firstToken == "list")
    {
        return ParseListScope(remaining);
    }

    if (firstToken == "request" ||
        firstToken == "dismiss")
    {
        return ParseRosterVerb(firstToken, remaining);
    }

    if (firstToken == "questactions")
    {
        return QuestActionsCommand{};
    }

    if (firstToken == "admin")
    {
        std::string_view stateToken = ConsumeToken(remaining);
        if (stateToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "admin requires 'on' or 'off'");
        }

        if (stateToken == "on")
        {
            BotAdminModeSetCommand cmd;
            cmd.enabled = true;
            return cmd;
        }

        if (stateToken == "off")
        {
            BotAdminModeSetCommand cmd;
            cmd.enabled = false;
            return cmd;
        }

        return MakeError(
            CommandParseErrorKind::InvalidArgument,
            "admin must be 'on' or 'off'");
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

    if (firstToken == "combat")
    {
        std::string_view modeToken = ConsumeToken(remaining);
        if (modeToken.empty())
        {
            return MakeError(
                CommandParseErrorKind::MissingArgument,
                "combat requires 'strict' or 'smart'");
        }

        BotCombatControlModeSetCommand cmd;
        if (modeToken == "strict" || modeToken == "strick")
            cmd.mode = model::BotCombatControlMode::Strict;
        else if (modeToken == "smart")
            cmd.mode = model::BotCombatControlMode::Smart;
        else
            return MakeError(
                CommandParseErrorKind::InvalidArgument,
                "combat must be 'strict' or 'smart'");
        return cmd;
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
