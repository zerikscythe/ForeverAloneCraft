#pragma once

#include "SharedDefines.h"
#include "SpellInfo.h"

#include <cstdint>

namespace living_world
{
namespace service
{
inline bool IsWorldBotPassiveSelfAuraCandidate(
    bool isPassive,
    bool isDoNotDisplay,
    std::uint32_t stances,
    bool allowWhileNotShapeshifted)
{
    if (!isPassive && !(isDoNotDisplay && stances != 0))
        return false;

    if (stances == 0)
        return true;

    return allowWhileNotShapeshifted;
}

inline bool ShouldAutoCastWorldBotPassiveSpell(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    return IsWorldBotPassiveSelfAuraCandidate(
        spellInfo->IsPassive(),
        spellInfo->HasAttribute(SPELL_ATTR0_DO_NOT_DISPLAY),
        spellInfo->Stances,
        spellInfo->HasAttribute(SPELL_ATTR2_ALLOW_WHILE_NOT_SHAPESHIFTED));
}
} // namespace service
} // namespace living_world