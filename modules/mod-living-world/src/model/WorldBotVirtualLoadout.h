#pragma once

#include <cstdint>
#include <string>

namespace living_world
{
namespace model
{
// V1 virtual world-bot loadout package.
//
// This intentionally models only the subset of bonuses that can be applied
// safely and honestly to Creature-backed world bots today. Player-only gear
// semantics (spell power coefficient pipelines, resilience rating, proc auras,
// trinkets, gems, enchants, set bonuses) are deferred to later slices.
struct WorldBotVirtualLoadout
{
    std::uint64_t loadoutId = 0;
    std::uint8_t classId = 0;
    std::string specKey;
    std::string loadoutKey;
    std::uint8_t gearTier = 1;
    std::string displayName;
    std::string description;

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

    [[nodiscard]] bool HasAnyBonus() const
    {
        return bonusStrength != 0
            || bonusAgility != 0
            || bonusStamina != 0
            || bonusIntellect != 0
            || bonusSpirit != 0
            || bonusHealth != 0
            || bonusMana != 0
            || bonusArmor != 0
            || bonusAttackPower != 0
            || bonusRangedAttackPower != 0;
    }
};
} // namespace model
} // namespace living_world