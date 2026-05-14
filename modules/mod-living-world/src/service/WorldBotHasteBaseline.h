#pragma once

#include "Unit.h"

#include <algorithm>

namespace living_world
{
namespace service
{
inline float NormalizeWorldBotHasteBonus(float hasteBonusPct)
{
    return std::max(hasteBonusPct, -99.0f);
}

inline void ApplyWorldBotHasteBonus(
    Unit& unit,
    float meleeHastePct,
    float rangedHastePct,
    float spellHastePct)
{
    meleeHastePct = NormalizeWorldBotHasteBonus(meleeHastePct);
    rangedHastePct = NormalizeWorldBotHasteBonus(rangedHastePct);
    spellHastePct = NormalizeWorldBotHasteBonus(spellHastePct);

    if (meleeHastePct != 0.0f)
    {
        unit.ApplyAttackTimePercentMod(BASE_ATTACK, meleeHastePct, true);
        unit.ApplyAttackTimePercentMod(OFF_ATTACK, meleeHastePct, true);
    }

    if (rangedHastePct != 0.0f)
        unit.ApplyAttackTimePercentMod(RANGED_ATTACK, rangedHastePct, true);

    if (spellHastePct != 0.0f)
        unit.ApplyCastTimePercentMod(spellHastePct, true);
}
} // namespace service
} // namespace living_world
