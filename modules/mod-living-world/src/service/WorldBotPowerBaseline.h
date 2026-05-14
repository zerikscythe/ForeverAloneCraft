#pragma once

#include "SharedDefines.h"

#include <cstdint>

namespace living_world
{
namespace service
{
inline Powers ResolveWorldBotPowerType(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return POWER_RAGE;
        case CLASS_ROGUE:
            return POWER_ENERGY;
        case CLASS_DEATH_KNIGHT:
            return POWER_RUNIC_POWER;
        default:
            return POWER_MANA;
    }
}

inline std::uint32_t ResolveWorldBotMaxPower(Powers powerType, std::uint32_t manaCap)
{
    switch (powerType)
    {
        case POWER_MANA:
            return manaCap;
        case POWER_ENERGY:
            return 100;
        case POWER_RAGE:
        case POWER_RUNIC_POWER:
            return 1000;
        default:
            return manaCap;
    }
}

inline std::uint32_t ResolveWorldBotSpawnPower(Powers powerType, std::uint32_t maxPower)
{
    if (maxPower == 0)
        return 0;

    switch (powerType)
    {
        case POWER_MANA:
        case POWER_ENERGY:
            return maxPower;
        case POWER_RAGE:
        case POWER_RUNIC_POWER:
            return 0;
        default:
            return maxPower;
    }
}
} // namespace service
} // namespace living_world
