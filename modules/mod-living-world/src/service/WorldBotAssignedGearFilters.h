#pragma once

#include "ItemTemplate.h"
#include "Player.h"
#include "SharedDefines.h"

#include <cstdint>
#include <initializer_list>
#include <string>

namespace living_world
{
namespace service
{
enum class AssignedGearStatFamily
{
    None,
    Tank,
    Healer,
    Caster,
    Agility,
    Strength,
};

inline bool IsAssignedGearJewelryLikeSlot(std::uint8_t slot)
{
    return slot == EQUIPMENT_SLOT_NECK
        || slot == EQUIPMENT_SLOT_FINGER1
        || slot == EQUIPMENT_SLOT_FINGER2
        || slot == EQUIPMENT_SLOT_TRINKET1
        || slot == EQUIPMENT_SLOT_TRINKET2
        || slot == EQUIPMENT_SLOT_BACK;
}

inline bool ShouldApplyAssignedGearStatFamilyGate(
    std::uint8_t slot,
    ItemTemplate const* itemTemplate)
{
    if (!itemTemplate)
        return false;

    if (IsAssignedGearJewelryLikeSlot(slot))
        return true;

    return slot == EQUIPMENT_SLOT_OFFHAND
        && itemTemplate->InventoryType != INVTYPE_WEAPONOFFHAND;
}

inline AssignedGearStatFamily DetermineAssignedGearStatFamily(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& roleKey)
{
    if (roleKey == "TANK")
        return AssignedGearStatFamily::Tank;

    if (roleKey == "HEAL")
        return AssignedGearStatFamily::Healer;

    if (classId == CLASS_HUNTER
        || classId == CLASS_ROGUE
        || (classId == CLASS_SHAMAN && specKey == "Enhancement")
        || (classId == CLASS_DRUID && specKey == "Feral"))
    {
        return AssignedGearStatFamily::Agility;
    }

    if (classId == CLASS_MAGE
        || classId == CLASS_WARLOCK
        || specKey == "Shadow"
        || specKey == "Balance"
        || specKey == "Elemental")
    {
        return AssignedGearStatFamily::Caster;
    }

    return AssignedGearStatFamily::Strength;
}

inline bool AssignedGearHasAnyStat(
    ItemTemplate const* itemTemplate,
    std::initializer_list<std::uint32_t> statTypes)
{
    if (!itemTemplate)
        return false;

    for (std::uint32_t i = 0; i < itemTemplate->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        _ItemStat const& stat = itemTemplate->ItemStat[i];
        for (std::uint32_t statType : statTypes)
        {
            if (stat.ItemStatType == statType && stat.ItemStatValue > 0)
                return true;
        }
    }

    return false;
}

inline bool MatchesAssignedGearStatFamily(
    ItemTemplate const* itemTemplate,
    AssignedGearStatFamily statFamily)
{
    switch (statFamily)
    {
        case AssignedGearStatFamily::Tank:
            return AssignedGearHasAnyStat(itemTemplate,
                {
                    ITEM_MOD_STAMINA,
                    ITEM_MOD_STRENGTH,
                    ITEM_MOD_DEFENSE_SKILL_RATING,
                    ITEM_MOD_DODGE_RATING,
                    ITEM_MOD_PARRY_RATING,
                    ITEM_MOD_BLOCK_RATING,
                    ITEM_MOD_BLOCK_VALUE,
                    ITEM_MOD_HIT_RATING,
                    ITEM_MOD_EXPERTISE_RATING,
                });
        case AssignedGearStatFamily::Healer:
            return AssignedGearHasAnyStat(itemTemplate,
                {
                    ITEM_MOD_INTELLECT,
                    ITEM_MOD_SPIRIT,
                    ITEM_MOD_SPELL_POWER,
                    ITEM_MOD_MANA_REGENERATION,
                    ITEM_MOD_CRIT_SPELL_RATING,
                    ITEM_MOD_HASTE_SPELL_RATING,
                    ITEM_MOD_CRIT_RATING,
                    ITEM_MOD_HASTE_RATING,
                    ITEM_MOD_MANA,
                });
        case AssignedGearStatFamily::Caster:
            return AssignedGearHasAnyStat(itemTemplate,
                {
                    ITEM_MOD_INTELLECT,
                    ITEM_MOD_SPIRIT,
                    ITEM_MOD_SPELL_POWER,
                    ITEM_MOD_MANA_REGENERATION,
                    ITEM_MOD_CRIT_SPELL_RATING,
                    ITEM_MOD_HASTE_SPELL_RATING,
                    ITEM_MOD_HIT_SPELL_RATING,
                    ITEM_MOD_CRIT_RATING,
                    ITEM_MOD_HASTE_RATING,
                    ITEM_MOD_HIT_RATING,
                    ITEM_MOD_MANA,
                });
        case AssignedGearStatFamily::Agility:
            return AssignedGearHasAnyStat(itemTemplate,
                {
                    ITEM_MOD_AGILITY,
                    ITEM_MOD_ATTACK_POWER,
                    ITEM_MOD_RANGED_ATTACK_POWER,
                    ITEM_MOD_CRIT_RATING,
                    ITEM_MOD_HIT_RATING,
                    ITEM_MOD_HASTE_RATING,
                    ITEM_MOD_EXPERTISE_RATING,
                    ITEM_MOD_ARMOR_PENETRATION_RATING,
                });
        case AssignedGearStatFamily::Strength:
            return AssignedGearHasAnyStat(itemTemplate,
                {
                    ITEM_MOD_STRENGTH,
                    ITEM_MOD_ATTACK_POWER,
                    ITEM_MOD_CRIT_RATING,
                    ITEM_MOD_HIT_RATING,
                    ITEM_MOD_HASTE_RATING,
                    ITEM_MOD_EXPERTISE_RATING,
                    ITEM_MOD_ARMOR_PENETRATION_RATING,
                });
        case AssignedGearStatFamily::None:
        default:
            return true;
    }
}

inline bool SupportsAssignedGearShield(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& roleKey)
{
    if (roleKey == "TANK")
        return classId == CLASS_WARRIOR || classId == CLASS_PALADIN;

    if (roleKey == "HEAL")
        return classId == CLASS_PALADIN || classId == CLASS_SHAMAN;

    return classId == CLASS_SHAMAN && specKey == "Elemental";
}

inline bool SupportsAssignedGearWeaponOffhand(
    std::uint8_t classId,
    std::string const& specKey)
{
    switch (classId)
    {
        case CLASS_ROGUE:
        case CLASS_WARRIOR:
        case CLASS_HUNTER:
            return true;
        case CLASS_SHAMAN:
            return specKey == "Enhancement";
        case CLASS_DEATH_KNIGHT:
            return specKey == "Frost";
        default:
            return false;
    }
}

inline bool IsAssignedGearTwoHandMainhand(ItemTemplate const* itemTemplate)
{
    return itemTemplate
        && itemTemplate->Class == ITEM_CLASS_WEAPON
        && itemTemplate->InventoryType == INVTYPE_2HWEAPON;
}

inline bool IsAssignedGearStaff(ItemTemplate const* itemTemplate)
{
    return itemTemplate
        && itemTemplate->Class == ITEM_CLASS_WEAPON
        && itemTemplate->SubClass == ITEM_SUBCLASS_WEAPON_STAFF;
}

inline bool IsAssignedGearOffhandCompatible(
    ItemTemplate const* offhandTemplate,
    ItemTemplate const* mainhandTemplate,
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& roleKey)
{
    if (!offhandTemplate)
        return false;

    if (offhandTemplate->InventoryType == INVTYPE_SHIELD
        && !SupportsAssignedGearShield(classId, specKey, roleKey))
    {
        return false;
    }

    if (offhandTemplate->InventoryType == INVTYPE_WEAPONOFFHAND
        && !SupportsAssignedGearWeaponOffhand(classId, specKey))
    {
        return false;
    }

    if (mainhandTemplate
        && (IsAssignedGearTwoHandMainhand(mainhandTemplate)
            || IsAssignedGearStaff(mainhandTemplate)))
    {
        return false;
    }

    return true;
}

inline bool IsAssignedGearMainhandCompatible(
    ItemTemplate const* mainhandTemplate,
    ItemTemplate const* offhandTemplate,
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& roleKey)
{
    if (!mainhandTemplate)
        return false;

    if (!offhandTemplate)
        return true;

    if (!IsAssignedGearOffhandCompatible(offhandTemplate, mainhandTemplate, classId, specKey, roleKey))
        return false;

    if (IsAssignedGearTwoHandMainhand(mainhandTemplate)
        || IsAssignedGearStaff(mainhandTemplate))
    {
        return false;
    }

    return true;
}
} // namespace service
} // namespace living_world