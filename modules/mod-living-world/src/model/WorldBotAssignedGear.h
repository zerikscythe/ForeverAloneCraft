#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{
struct WorldBotAssignedGearEntry
{
    std::uint8_t slot = 0;
    std::uint32_t itemId = 0;
    std::uint32_t itemLevel = 0;
    std::uint8_t quality = 0;
};

struct WorldBotAssignedGearSummary
{
    std::int32_t bonusStrength = 0;
    std::int32_t bonusAgility = 0;
    std::int32_t bonusStamina = 0;
    std::int32_t bonusIntellect = 0;
    std::int32_t bonusSpirit = 0;
    std::int32_t bonusHealth = 0;
    std::int32_t bonusMana = 0;
    std::int32_t bonusArmor = 0;
    std::int32_t bonusAttackPower = 0;
    std::int32_t bonusRangedAttackPower = 0;
};
} // namespace model
} // namespace living_world