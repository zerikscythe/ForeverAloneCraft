#include "service/SimpleBotCombatSpecRoleResolver.h"

#include "SharedDefines.h"

namespace living_world
{
namespace service
{
namespace
{
std::pair<std::string, std::string> GetGuessedSpecAndRole(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return { "Arms", "DPS" };
        case CLASS_PALADIN:
            return { "Retribution", "DPS" };
        case CLASS_HUNTER:
            return { "BeastMastery", "DPS" };
        case CLASS_ROGUE:
            return { "Combat", "DPS" };
        case CLASS_PRIEST:
            return { "Shadow", "DPS" };
        case CLASS_DEATH_KNIGHT:
            return { "Unholy", "DPS" };
        case CLASS_SHAMAN:
            return { "Elemental", "DPS" };
        case CLASS_MAGE:
            return { "Frost", "DPS" };
        case CLASS_WARLOCK:
            return { "Affliction", "DPS" };
        case CLASS_DRUID:
            return { "Balance", "DPS" };
        default:
            return { "", "" };
    }
}
} // namespace

BotCombatSpecRoleResolutionResult SimpleBotCombatSpecRoleResolver::Resolve(
    BotCombatSpecRoleResolutionRequest const& request) const
{
    BotCombatSpecRoleResolutionResult result;
    auto const [guessedSpec, guessedRole] =
        GetGuessedSpecAndRole(request.classId);

    result.guessedSpecKey = guessedSpec;
    result.guessedRoleKey = guessedRole;
    result.effectiveSpecKey =
        request.specOverrideKey && !request.specOverrideKey->empty()
            ? *request.specOverrideKey
            : guessedSpec;
    result.effectiveRoleKey =
        request.roleOverrideKey && !request.roleOverrideKey->empty()
            ? *request.roleOverrideKey
            : guessedRole;
    return result;
}
} // namespace service
} // namespace living_world
