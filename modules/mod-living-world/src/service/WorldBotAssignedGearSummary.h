#pragma once

#include "ItemTemplate.h"
#include "SharedDefines.h"
#include "model/WorldBotAssignedGear.h"

#include <cstdint>

namespace living_world
{
namespace service
{
inline void AccumulateWorldBotAssignedGearResistance(
    model::WorldBotAssignedGearSummary& summary,
    SpellSchools school,
    std::int32_t resistance)
{
    if (resistance == 0)
        return;

    switch (school)
    {
        case SPELL_SCHOOL_HOLY:
            summary.bonusHolyResistance += resistance;
            break;
        case SPELL_SCHOOL_FIRE:
            summary.bonusFireResistance += resistance;
            break;
        case SPELL_SCHOOL_NATURE:
            summary.bonusNatureResistance += resistance;
            break;
        case SPELL_SCHOOL_FROST:
            summary.bonusFrostResistance += resistance;
            break;
        case SPELL_SCHOOL_SHADOW:
            summary.bonusShadowResistance += resistance;
            break;
        case SPELL_SCHOOL_ARCANE:
            summary.bonusArcaneResistance += resistance;
            break;
        default:
            break;
    }
}

inline void AccumulateWorldBotAssignedGearStat(
    model::WorldBotAssignedGearSummary& summary,
    std::uint32_t statType,
    std::int32_t statValue)
{
    switch (statType)
    {
        case ITEM_MOD_STRENGTH:
            summary.bonusStrength += statValue;
            break;
        case ITEM_MOD_AGILITY:
            summary.bonusAgility += statValue;
            break;
        case ITEM_MOD_STAMINA:
            summary.bonusStamina += statValue;
            break;
        case ITEM_MOD_INTELLECT:
            summary.bonusIntellect += statValue;
            break;
        case ITEM_MOD_SPIRIT:
            summary.bonusSpirit += statValue;
            break;
        case ITEM_MOD_HEALTH:
            summary.bonusHealth += statValue;
            break;
        case ITEM_MOD_MANA:
            summary.bonusMana += statValue;
            break;
        case ITEM_MOD_ATTACK_POWER:
            summary.bonusAttackPower += statValue;
            break;
        case ITEM_MOD_RANGED_ATTACK_POWER:
            summary.bonusRangedAttackPower += statValue;
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            summary.bonusDefenseSkillRating += statValue;
            break;
        case ITEM_MOD_DODGE_RATING:
            summary.bonusDodgeRating += statValue;
            break;
        case ITEM_MOD_PARRY_RATING:
            summary.bonusParryRating += statValue;
            break;
        case ITEM_MOD_BLOCK_RATING:
            summary.bonusBlockRating += statValue;
            break;
        case ITEM_MOD_BLOCK_VALUE:
            summary.bonusBlockValue += statValue;
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            summary.bonusMeleeHitRating += statValue;
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            summary.bonusRangedHitRating += statValue;
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            summary.bonusSpellHitRating += statValue;
            break;
        case ITEM_MOD_HIT_RATING:
            summary.bonusMeleeHitRating += statValue;
            summary.bonusRangedHitRating += statValue;
            summary.bonusSpellHitRating += statValue;
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            summary.bonusMeleeCritRating += statValue;
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            summary.bonusRangedCritRating += statValue;
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            summary.bonusSpellCritRating += statValue;
            break;
        case ITEM_MOD_CRIT_RATING:
            summary.bonusMeleeCritRating += statValue;
            summary.bonusRangedCritRating += statValue;
            summary.bonusSpellCritRating += statValue;
            break;
        case ITEM_MOD_HASTE_MELEE_RATING:
            summary.bonusMeleeHasteRating += statValue;
            break;
        case ITEM_MOD_HASTE_RANGED_RATING:
            summary.bonusRangedHasteRating += statValue;
            break;
        case ITEM_MOD_HASTE_SPELL_RATING:
            summary.bonusSpellHasteRating += statValue;
            break;
        case ITEM_MOD_HASTE_RATING:
            summary.bonusMeleeHasteRating += statValue;
            summary.bonusRangedHasteRating += statValue;
            summary.bonusSpellHasteRating += statValue;
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            summary.bonusExpertiseRating += statValue;
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            summary.bonusArmorPenetrationRating += statValue;
            break;
        case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
        case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
        case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
        case ITEM_MOD_HIT_TAKEN_RATING:
            summary.bonusHitTakenRating += statValue;
            break;
        case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
        case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
        case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
        case ITEM_MOD_CRIT_TAKEN_RATING:
            summary.bonusCritTakenRating += statValue;
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            summary.bonusResilienceRating += statValue;
            break;
        case ITEM_MOD_SPELL_POWER:
            summary.bonusSpellPower += statValue;
            summary.bonusHealingPower += statValue;
            break;
        case ITEM_MOD_SPELL_DAMAGE_DONE:
            summary.bonusSpellPower += statValue;
            break;
        case ITEM_MOD_SPELL_HEALING_DONE:
            summary.bonusHealingPower += statValue;
            break;
        case ITEM_MOD_MANA_REGENERATION:
            summary.bonusManaRegen += statValue;
            break;
        case ITEM_MOD_HEALTH_REGEN:
            summary.bonusHealthRegen += statValue;
            break;
        case ITEM_MOD_SPELL_PENETRATION:
            summary.bonusSpellPenetration += statValue;
            break;
        default:
            break;
    }
}
} // namespace service
} // namespace living_world
