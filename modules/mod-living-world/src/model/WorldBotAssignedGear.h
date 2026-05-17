#pragma once

#include <cstdint>
#include <string>

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
    std::string enchantments;
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
    std::int32_t bonusHolyResistance = 0;
    std::int32_t bonusFireResistance = 0;
    std::int32_t bonusNatureResistance = 0;
    std::int32_t bonusFrostResistance = 0;
    std::int32_t bonusShadowResistance = 0;
    std::int32_t bonusArcaneResistance = 0;
    std::int32_t bonusAttackPower = 0;
    std::int32_t bonusRangedAttackPower = 0;

    std::int32_t bonusDefenseSkillRating = 0;
    std::int32_t bonusDodgeRating = 0;
    std::int32_t bonusParryRating = 0;
    std::int32_t bonusBlockRating = 0;
    std::int32_t bonusBlockValue = 0;

    std::int32_t bonusMeleeHitRating = 0;
    std::int32_t bonusRangedHitRating = 0;
    std::int32_t bonusSpellHitRating = 0;
    std::int32_t bonusMeleeCritRating = 0;
    std::int32_t bonusRangedCritRating = 0;
    std::int32_t bonusSpellCritRating = 0;
    std::int32_t bonusMeleeHasteRating = 0;
    std::int32_t bonusRangedHasteRating = 0;
    std::int32_t bonusSpellHasteRating = 0;
    std::int32_t bonusExpertiseRating = 0;
    std::int32_t bonusArmorPenetrationRating = 0;

    std::int32_t bonusHitTakenRating = 0;
    std::int32_t bonusCritTakenRating = 0;
    std::int32_t bonusResilienceRating = 0;

    std::int32_t bonusSpellPower = 0;
    std::int32_t bonusHealingPower = 0;
    std::int32_t bonusManaRegen = 0;
    std::int32_t bonusHealthRegen = 0;
    std::int32_t bonusSpellPenetration = 0;
};
} // namespace model
} // namespace living_world
