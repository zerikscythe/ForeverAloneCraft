#pragma once

#include "SharedDefines.h"

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
struct WorldBotAttackPowerBaseline
{
    std::int32_t meleeAttackPower = 0;
    std::int32_t rangedAttackPower = 0;
};

inline WorldBotAttackPowerBaseline BuildWorldBotAttackPowerBaseline(
    std::uint8_t classId,
    std::uint8_t level,
    std::int32_t strength,
    std::int32_t agility)
{
    WorldBotAttackPowerBaseline baseline;

    switch (classId)
    {
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_WARRIOR:
            baseline.meleeAttackPower = static_cast<std::int32_t>(level) * 3 + strength * 2 - 20;
            break;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
        case CLASS_ROGUE:
            baseline.meleeAttackPower = static_cast<std::int32_t>(level) * 2 + strength + agility - 20;
            break;
        case CLASS_DRUID:
            baseline.meleeAttackPower = strength * 2 - 20;
            break;
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            baseline.meleeAttackPower = strength - 10;
            break;
        default:
            break;
    }

    switch (classId)
    {
        case CLASS_HUNTER:
            baseline.rangedAttackPower = static_cast<std::int32_t>(level) * 2 + agility - 10;
            break;
        case CLASS_ROGUE:
        case CLASS_WARRIOR:
            baseline.rangedAttackPower = static_cast<std::int32_t>(level) + agility - 10;
            break;
        default:
            baseline.rangedAttackPower = agility - 10;
            break;
    }

    baseline.meleeAttackPower = std::max<std::int32_t>(baseline.meleeAttackPower, 0);
    baseline.rangedAttackPower = std::max<std::int32_t>(baseline.rangedAttackPower, 0);
    return baseline;
}
} // namespace service
} // namespace living_world