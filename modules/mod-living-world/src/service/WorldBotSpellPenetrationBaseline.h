#pragma once

#include "SharedDefines.h"

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline float ApplyWorldBotSpellPenetration(
    SpellSchoolMask schoolMask,
    float victimResistance,
    std::int32_t spellPenetration)
{
    if (schoolMask == SPELL_SCHOOL_MASK_NORMAL || spellPenetration <= 0)
        return std::max(victimResistance, 0.0f);

    return std::max(victimResistance - static_cast<float>(spellPenetration), 0.0f);
}
} // namespace service
} // namespace living_world
