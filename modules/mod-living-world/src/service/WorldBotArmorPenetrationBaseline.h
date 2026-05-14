#pragma once

#include <algorithm>

namespace living_world
{
namespace service
{
inline float BuildWorldBotArmorPenetrationCap(float armor, float victimLevel)
{
    if (armor <= 0.0f)
        return 0.0f;

    float maxArmorPen = victimLevel < 60.0f
        ? 400.0f + 85.0f * victimLevel
        : 400.0f + 85.0f * victimLevel + 4.5f * 85.0f * (victimLevel - 59.0f);

    return std::min((armor + maxArmorPen) / 3.0f, armor);
}

inline float ApplyWorldBotArmorPenetration(float armor, float victimLevel, float armorPenetrationPct)
{
    if (armor <= 0.0f || armorPenetrationPct <= 0.0f)
        return std::max(armor, 0.0f);

    float const maxArmorPen = BuildWorldBotArmorPenetrationCap(armor, victimLevel);
    float const ignoredArmor = std::min(maxArmorPen * armorPenetrationPct / 100.0f, maxArmorPen);
    return std::max(armor - ignoredArmor, 0.0f);
}
} // namespace service
} // namespace living_world
