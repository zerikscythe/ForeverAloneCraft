#pragma once

#include "SharedDefines.h"

#include <cstdint>

namespace living_world
{
namespace service
{
inline void ApplyWorldBotSpellPowerBonus(
    SpellSchoolMask schoolMask,
    std::int32_t spellPower,
    std::int32_t& doneAdvertisedBenefit)
{
    if ((schoolMask & ~SPELL_SCHOOL_MASK_NORMAL) == 0)
        return;

    doneAdvertisedBenefit += spellPower;
}

inline void ApplyWorldBotHealingPowerBonus(
    std::int32_t healingPower,
    std::int32_t& advertisedBenefit)
{
    advertisedBenefit += healingPower;
}
} // namespace service
} // namespace living_world
