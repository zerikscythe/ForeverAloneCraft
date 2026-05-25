#include "service/WorldBotAssignedGearService.h"

#include "integration/BotAssignedGearTemplateRepository.h"
#include "DataStores/DBCStores.h"
#include "Globals/ObjectMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"
#include "model/BotSpecKey.h"
#include "service/WorldBotAssignedGearFilters.h"
#include "service/WorldBotGearBand.h"
#include "service/WorldBotAssignedGearSummary.h"
#include "service/WorldBotPreparationService.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <random>
#include <unordered_set>

namespace living_world
{
namespace service
{
namespace
{
struct SlotRule
{
    std::uint8_t slot = 0;
    std::array<std::uint32_t, 4> inventoryTypes{};
    std::size_t inventoryTypeCount = 0;
};

struct AssignedGearQualityOdds
{
    int rareOrBetterChance = 30;
    int epicChance = 0;
    int luckyEpicSlotChance = 0;
};

std::vector<model::WorldBotAssignedGearEntry> FinalizeAssignedGearEntries(
    std::vector<model::WorldBotAssignedGearEntry> entries)
{
    std::vector<model::WorldBotAssignedGearEntry> finalized;
    finalized.reserve(entries.size());

    for (model::WorldBotAssignedGearEntry& entry : entries)
    {
        if (entry.itemId == 0)
            continue;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
        if (!itemTemplate)
            continue;

        entry.itemLevel = itemTemplate->ItemLevel;
        entry.quality = static_cast<std::uint8_t>(itemTemplate->Quality);
        finalized.push_back(std::move(entry));
    }

    return finalized;
}

constexpr std::array<SlotRule, 17> SlotRules = {{
    { EQUIPMENT_SLOT_HEAD,      { INVTYPE_HEAD }, 1 },
    { EQUIPMENT_SLOT_NECK,      { INVTYPE_NECK }, 1 },
    { EQUIPMENT_SLOT_SHOULDERS, { INVTYPE_SHOULDERS }, 1 },
    { EQUIPMENT_SLOT_BACK,      { INVTYPE_CLOAK }, 1 },
    { EQUIPMENT_SLOT_CHEST,     { INVTYPE_CHEST, INVTYPE_ROBE }, 2 },
    { EQUIPMENT_SLOT_WRISTS,    { INVTYPE_WRISTS }, 1 },
    { EQUIPMENT_SLOT_HANDS,     { INVTYPE_HANDS }, 1 },
    { EQUIPMENT_SLOT_WAIST,     { INVTYPE_WAIST }, 1 },
    { EQUIPMENT_SLOT_LEGS,      { INVTYPE_LEGS }, 1 },
    { EQUIPMENT_SLOT_FEET,      { INVTYPE_FEET }, 1 },
    { EQUIPMENT_SLOT_FINGER1,   { INVTYPE_FINGER }, 1 },
    { EQUIPMENT_SLOT_FINGER2,   { INVTYPE_FINGER }, 1 },
    { EQUIPMENT_SLOT_TRINKET1,  { INVTYPE_TRINKET }, 1 },
    { EQUIPMENT_SLOT_TRINKET2,  { INVTYPE_TRINKET }, 1 },
    { EQUIPMENT_SLOT_MAINHAND,  { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_2HWEAPON }, 3 },
    { EQUIPMENT_SLOT_OFFHAND,   { INVTYPE_SHIELD, INVTYPE_HOLDABLE, INVTYPE_WEAPONOFFHAND, INVTYPE_WEAPON }, 4 },
    { EQUIPMENT_SLOT_RANGED,    { INVTYPE_RANGED, INVTYPE_RANGEDRIGHT, INVTYPE_THROWN, INVTYPE_RELIC }, 4 }
}};

AssignedGearQualityOdds GetAssignedGearQualityOdds(
    std::uint8_t level,
    std::uint8_t endgameStage)
{
    if (level >= 80)
    {
        switch (endgameStage)
        {
            case 0: return { 100, 15, 20 };
            case 1: return { 100, 25, 30 };
            case 2: return { 100, 40, 45 };
            case 3: return { 100, 60, 60 };
            case 4: return { 100, 80, 80 };
            default: return { 100, 15, 20 };
        }
    }

    if (level >= 70)
        return { 95, 8, 10 };
    if (level >= 60)
        return { 85, 2, 5 };
    if (level >= 50)
        return { 75, 0, 0 };
    if (level >= 40)
        return { 60, 0, 0 };
    if (level >= 30)
        return { 25, 0, 0 };
    if (level >= 20)
        return { 10, 0, 0 };

    return { 0, 0, 0 };
}

bool IsAllowedForClass(ItemTemplate const* itemTemplate, std::uint8_t classId)
{
    if (!itemTemplate || classId == 0)
        return false;

    if (itemTemplate->AllowableClass == std::numeric_limits<std::uint32_t>::max())
        return true;

    std::uint32_t const classMask = (1u << (classId - 1u));
    return (itemTemplate->AllowableClass & classMask) != 0;
}

bool IsAllowedForRace(ItemTemplate const* itemTemplate, std::uint8_t raceId)
{
    if (!itemTemplate || raceId == 0)
        return false;

    if (itemTemplate->AllowableRace == std::numeric_limits<std::uint32_t>::max())
        return true;

    std::uint32_t const raceMask = (1u << (raceId - 1u));
    return (itemTemplate->AllowableRace & raceMask) != 0;
}

bool HasUnsupportedWorldBotRequirement(ItemTemplate const* itemTemplate)
{
    if (!itemTemplate)
        return true;

    return itemTemplate->RequiredSkill != 0
        || itemTemplate->RequiredSkillRank != 0
        || itemTemplate->RequiredSpell != 0
        || itemTemplate->RequiredHonorRank != 0
        || itemTemplate->RequiredCityRank != 0
        || itemTemplate->RequiredReputationFaction != 0
        || itemTemplate->RequiredReputationRank != 0;
}

std::optional<std::uint32_t> GetPreferredArmorSubclass(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        default:
            return std::nullopt;
    }
}

bool IsAllowedArmorForSlot(
    ItemTemplate const* itemTemplate,
    std::uint8_t classId,
    std::uint8_t slot)
{
    if (!itemTemplate)
        return false;

    if (IsAssignedGearJewelryLikeSlot(slot))
        return true;

    if (itemTemplate->Class != ITEM_CLASS_ARMOR)
        return false;

    if (itemTemplate->InventoryType == INVTYPE_SHIELD
        || itemTemplate->InventoryType == INVTYPE_HOLDABLE)
    {
        return true;
    }

    if (itemTemplate->InventoryType == INVTYPE_RELIC)
    {
        switch (itemTemplate->SubClass)
        {
            case ITEM_SUBCLASS_ARMOR_LIBRAM:
                return classId == CLASS_PALADIN;
            case ITEM_SUBCLASS_ARMOR_IDOL:
                return classId == CLASS_DRUID;
            case ITEM_SUBCLASS_ARMOR_TOTEM:
                return classId == CLASS_SHAMAN;
            case ITEM_SUBCLASS_ARMOR_SIGIL:
                return classId == CLASS_DEATH_KNIGHT;
            default:
                return false;
        }
    }

    std::optional<std::uint32_t> preferredArmor = GetPreferredArmorSubclass(classId);
    if (!preferredArmor)
        return true;

    return itemTemplate->SubClass == *preferredArmor;
}

bool IsAllowedWeaponSubclass(std::uint8_t classId, std::uint32_t subClass)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return subClass == ITEM_SUBCLASS_WEAPON_AXE
                || subClass == ITEM_SUBCLASS_WEAPON_AXE2
                || subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_MACE2
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD2
                || subClass == ITEM_SUBCLASS_WEAPON_POLEARM
                || subClass == ITEM_SUBCLASS_WEAPON_STAFF
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_FIST
                || subClass == ITEM_SUBCLASS_WEAPON_BOW
                || subClass == ITEM_SUBCLASS_WEAPON_GUN
                || subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW
                || subClass == ITEM_SUBCLASS_WEAPON_THROWN;
        case CLASS_PALADIN:
            return subClass == ITEM_SUBCLASS_WEAPON_AXE
                || subClass == ITEM_SUBCLASS_WEAPON_AXE2
                || subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_MACE2
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD2
                || subClass == ITEM_SUBCLASS_WEAPON_POLEARM;
        case CLASS_HUNTER:
            return subClass == ITEM_SUBCLASS_WEAPON_AXE
                || subClass == ITEM_SUBCLASS_WEAPON_AXE2
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD2
                || subClass == ITEM_SUBCLASS_WEAPON_POLEARM
                || subClass == ITEM_SUBCLASS_WEAPON_STAFF
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_FIST
                || subClass == ITEM_SUBCLASS_WEAPON_BOW
                || subClass == ITEM_SUBCLASS_WEAPON_GUN
                || subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW;
        case CLASS_ROGUE:
            return subClass == ITEM_SUBCLASS_WEAPON_AXE
                || subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_FIST
                || subClass == ITEM_SUBCLASS_WEAPON_THROWN
                || subClass == ITEM_SUBCLASS_WEAPON_BOW
                || subClass == ITEM_SUBCLASS_WEAPON_GUN
                || subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW;
        case CLASS_PRIEST:
            return subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_STAFF
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_WAND;
        case CLASS_DEATH_KNIGHT:
            return subClass == ITEM_SUBCLASS_WEAPON_AXE
                || subClass == ITEM_SUBCLASS_WEAPON_AXE2
                || subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_MACE2
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD2
                || subClass == ITEM_SUBCLASS_WEAPON_POLEARM;
        case CLASS_SHAMAN:
            return subClass == ITEM_SUBCLASS_WEAPON_AXE
                || subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_STAFF
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_FIST;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return subClass == ITEM_SUBCLASS_WEAPON_SWORD
                || subClass == ITEM_SUBCLASS_WEAPON_STAFF
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_WAND;
        case CLASS_DRUID:
            return subClass == ITEM_SUBCLASS_WEAPON_MACE
                || subClass == ITEM_SUBCLASS_WEAPON_STAFF
                || subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_POLEARM;
        default:
            return false;
    }
}

bool IsInventoryTypeMatch(SlotRule const& rule, std::uint32_t inventoryType)
{
    for (std::size_t i = 0; i < rule.inventoryTypeCount; ++i)
    {
        if (rule.inventoryTypes[i] == inventoryType)
            return true;
    }

    return false;
}

float GetStatWeight(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& roleKey,
    std::uint32_t statType)
{
    if (roleKey == "TANK")
    {
        switch (statType)
        {
            case ITEM_MOD_STAMINA: return 4.5f;
            case ITEM_MOD_STRENGTH: return 3.0f;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
            case ITEM_MOD_DODGE_RATING:
            case ITEM_MOD_PARRY_RATING:
            case ITEM_MOD_BLOCK_RATING:
            case ITEM_MOD_BLOCK_VALUE:
                return 3.0f;
            case ITEM_MOD_HIT_RATING:
            case ITEM_MOD_EXPERTISE_RATING:
                return 1.5f;
            default:
                return 0.0f;
        }
    }

    if (roleKey == "HEAL")
    {
        switch (statType)
        {
            case ITEM_MOD_INTELLECT: return 4.0f;
            case ITEM_MOD_SPIRIT: return 3.0f;
            case ITEM_MOD_STAMINA: return 2.0f;
            case ITEM_MOD_SPELL_POWER: return 5.0f;
            case ITEM_MOD_MANA_REGENERATION: return 3.0f;
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_HASTE_SPELL_RATING:
            case ITEM_MOD_CRIT_RATING:
            case ITEM_MOD_HASTE_RATING:
                return 1.8f;
            default:
                return 0.0f;
        }
    }

    if (classId == CLASS_HUNTER
        || classId == CLASS_ROGUE
        || (classId == CLASS_SHAMAN && specKey == "Enhancement")
        || (classId == CLASS_DRUID && specKey == "Feral"))
    {
        switch (statType)
        {
            case ITEM_MOD_AGILITY: return 5.0f;
            case ITEM_MOD_STAMINA: return 2.5f;
            case ITEM_MOD_ATTACK_POWER:
            case ITEM_MOD_RANGED_ATTACK_POWER:
                return 3.0f;
            case ITEM_MOD_CRIT_RATING:
            case ITEM_MOD_HIT_RATING:
            case ITEM_MOD_HASTE_RATING:
            case ITEM_MOD_EXPERTISE_RATING:
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                return 2.0f;
            default:
                return 0.0f;
        }
    }

    if (classId == CLASS_MAGE
        || classId == CLASS_WARLOCK
        || specKey == "Shadow"
        || specKey == "Balance"
        || specKey == "Elemental")
    {
        switch (statType)
        {
            case ITEM_MOD_INTELLECT: return 4.0f;
            case ITEM_MOD_STAMINA: return 2.0f;
            case ITEM_MOD_SPIRIT: return 1.0f;
            case ITEM_MOD_SPELL_POWER: return 5.0f;
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_HASTE_SPELL_RATING:
            case ITEM_MOD_HIT_SPELL_RATING:
            case ITEM_MOD_CRIT_RATING:
            case ITEM_MOD_HASTE_RATING:
            case ITEM_MOD_HIT_RATING:
                return 2.0f;
            default:
                return 0.0f;
        }
    }

    switch (statType)
    {
        case ITEM_MOD_STRENGTH: return 4.0f;
        case ITEM_MOD_STAMINA: return 2.5f;
        case ITEM_MOD_ATTACK_POWER: return 3.0f;
        case ITEM_MOD_CRIT_RATING:
        case ITEM_MOD_HIT_RATING:
        case ITEM_MOD_HASTE_RATING:
        case ITEM_MOD_EXPERTISE_RATING:
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            return 2.0f;
        case ITEM_MOD_AGILITY: return 1.5f;
        default:
            return 0.0f;
    }
}

float ScoreRogueWeaponPreference(
    std::string const& canonicalSpecKey,
    std::uint8_t slot,
    ItemTemplate const* itemTemplate)
{
    if (!itemTemplate || itemTemplate->Class != ITEM_CLASS_WEAPON)
        return 0.0f;

    float bonus = 0.0f;
    std::uint32_t const subClass = itemTemplate->SubClass;
    std::uint32_t const delay = itemTemplate->Delay;

    if (canonicalSpecKey == "Assassination")
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
        {
            if (subClass == ITEM_SUBCLASS_WEAPON_THROWN)
                bonus += 180.0f;
            else if (subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW)
                bonus += 70.0f;
            else
                bonus += 20.0f;
            return bonus;
        }

        if (subClass == ITEM_SUBCLASS_WEAPON_DAGGER)
            bonus += 260.0f;
        else
            bonus -= 320.0f;

        if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            if (delay >= 1700)
                bonus += 50.0f;
        }
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
        {
            if (delay > 0 && delay <= 1600)
                bonus += 80.0f;
        }

        return bonus;
    }

    if (canonicalSpecKey == "Combat")
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
        {
            if (subClass == ITEM_SUBCLASS_WEAPON_GUN)
                bonus += 180.0f;
            else if (subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW)
                bonus += 120.0f;
            else
                bonus += 40.0f;
            return bonus;
        }

        if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            if (subClass == ITEM_SUBCLASS_WEAPON_FIST)
                bonus += 260.0f;
            else if (subClass == ITEM_SUBCLASS_WEAPON_DAGGER)
                bonus += 180.0f;
            else
                bonus += 25.0f;

            if (delay >= 2400)
                bonus += 140.0f;
            else if (delay >= 2000)
                bonus += 70.0f;
            else
                bonus -= 80.0f;
        }
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
        {
            if (subClass == ITEM_SUBCLASS_WEAPON_DAGGER
                || subClass == ITEM_SUBCLASS_WEAPON_FIST)
            {
                bonus += 220.0f;
            }
            else
            {
                bonus += 40.0f;
            }

            if (delay > 0 && delay <= 1600)
                bonus += 120.0f;
            else if (delay <= 1900)
                bonus += 50.0f;
            else
                bonus -= 60.0f;
        }

        return bonus;
    }

    if (canonicalSpecKey == "Subtlety")
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
        {
            if (subClass == ITEM_SUBCLASS_WEAPON_BOW)
                bonus += 180.0f;
            else if (subClass == ITEM_SUBCLASS_WEAPON_THROWN)
                bonus += 100.0f;
            else
                bonus += 30.0f;
            return bonus;
        }

        if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            if (subClass == ITEM_SUBCLASS_WEAPON_DAGGER)
                bonus += 140.0f;
            else if (subClass == ITEM_SUBCLASS_WEAPON_FIST
                || subClass == ITEM_SUBCLASS_WEAPON_SWORD)
            {
                bonus += 80.0f;
            }
        }
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
        {
            if (delay > 0 && delay <= 1800)
                bonus += 70.0f;
        }
    }

    return bonus;
}

float ScoreItemForIdentity(
    integration::BotIdentityRecord const& identity,
    std::string const& canonicalSpecKey,
    std::string const& roleKey,
    std::uint8_t slot,
    ItemTemplate const* itemTemplate)
{
    if (!itemTemplate)
        return 0.0f;

    float score = static_cast<float>(itemTemplate->ItemLevel) * 2.0f;

    for (std::uint32_t i = 0; i < itemTemplate->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        _ItemStat const& stat = itemTemplate->ItemStat[i];
        score += GetStatWeight(identity.classId, canonicalSpecKey, roleKey, stat.ItemStatType)
            * static_cast<float>(stat.ItemStatValue);
    }

    if (itemTemplate->Armor > 0)
        score += static_cast<float>(itemTemplate->Armor) * (roleKey == "TANK" ? 0.08f : 0.03f);

    if (itemTemplate->Class == ITEM_CLASS_WEAPON)
    {
        float const dps = itemTemplate->getDPS();
        if (roleKey == "HEAL")
            score += dps * 0.5f;
        else if (identity.classId == CLASS_HUNTER && itemTemplate->InventoryType == INVTYPE_RANGEDRIGHT)
            score += dps * 8.0f;
        else if (roleKey == "TANK")
            score += dps * 2.0f;
        else
            score += dps * 6.0f;

        if (identity.classId == CLASS_ROGUE)
            score += ScoreRogueWeaponPreference(canonicalSpecKey, slot, itemTemplate);
    }

    return score;
}

ItemTemplate const* FindChosenTemplateForSlot(
    std::vector<model::WorldBotAssignedGearEntry> const& selectedEntries,
    std::uint8_t slot)
{
    auto const it = std::find_if(
        selectedEntries.begin(),
        selectedEntries.end(),
        [&](model::WorldBotAssignedGearEntry const& entry)
        {
            return entry.slot == slot;
        });
    if (it == selectedEntries.end())
        return nullptr;

    return sObjectMgr->GetItemTemplate(it->itemId);
}

bool IsContextuallyCompatibleWithChosenGear(
    integration::BotIdentityRecord const& identity,
    std::string const& canonicalSpecKey,
    std::string const& roleKey,
    SlotRule const& rule,
    ItemTemplate const* itemTemplate,
    std::vector<model::WorldBotAssignedGearEntry> const& selectedEntries)
{
    if (!itemTemplate)
        return false;

    if (rule.slot == EQUIPMENT_SLOT_MAINHAND)
    {
        ItemTemplate const* chosenOffhand = FindChosenTemplateForSlot(selectedEntries, EQUIPMENT_SLOT_OFFHAND);
        return IsAssignedGearMainhandCompatible(
            itemTemplate,
            chosenOffhand,
            identity.classId,
            canonicalSpecKey,
            roleKey);
    }

    if (rule.slot == EQUIPMENT_SLOT_OFFHAND)
    {
        ItemTemplate const* chosenMainhand = FindChosenTemplateForSlot(selectedEntries, EQUIPMENT_SLOT_MAINHAND);
        return IsAssignedGearOffhandCompatible(
            itemTemplate,
            chosenMainhand,
            identity.classId,
            canonicalSpecKey,
            roleKey);
    }

    return true;
}

std::vector<ItemTemplate const*> CollectCandidates(
    integration::BotIdentityRecord const& identity,
    std::string const& canonicalSpecKey,
    std::string const& roleKey,
    SlotRule const& rule,
    std::uint8_t minQuality,
    std::uint8_t maxQuality,
    std::unordered_set<std::uint32_t> const& usedItemIds,
    std::vector<model::WorldBotAssignedGearEntry> const& selectedEntries)
{
    std::vector<ItemTemplate const*> candidates;
    auto const* store = sObjectMgr->GetItemTemplateStoreFast();
    if (!store)
        return candidates;

    AssignedGearStatFamily const statFamily =
        DetermineAssignedGearStatFamily(identity.classId, canonicalSpecKey, roleKey);

    for (ItemTemplate const* itemTemplate : *store)
    {
        if (!itemTemplate)
            continue;

        if (usedItemIds.count(itemTemplate->ItemId) > 0)
            continue;

        if (!IsInventoryTypeMatch(rule, itemTemplate->InventoryType))
            continue;

        if (!IsAllowedForClass(itemTemplate, identity.classId))
            continue;

        if (!IsAllowedForRace(itemTemplate, identity.raceId))
            continue;

        if (HasUnsupportedWorldBotRequirement(itemTemplate))
            continue;

        if (itemTemplate->RequiredLevel > identity.level)
            continue;

        if (itemTemplate->Quality < minQuality || itemTemplate->Quality > maxQuality)
            continue;

        if (itemTemplate->HasFlag(ITEM_FLAG_DEPRECATED))
            continue;

        if (itemTemplate->Class == ITEM_CLASS_ARMOR)
        {
            if (!IsAllowedArmorForSlot(itemTemplate, identity.classId, rule.slot))
                continue;
        }
        else if (itemTemplate->Class == ITEM_CLASS_WEAPON)
        {
            if (!IsAllowedWeaponSubclass(identity.classId, itemTemplate->SubClass))
                continue;
        }
        else if (itemTemplate->InventoryType != INVTYPE_TRINKET
            && itemTemplate->InventoryType != INVTYPE_FINGER
            && itemTemplate->InventoryType != INVTYPE_NECK
            && itemTemplate->InventoryType != INVTYPE_CLOAK
            && itemTemplate->InventoryType != INVTYPE_HOLDABLE
            && itemTemplate->InventoryType != INVTYPE_SHIELD
            && itemTemplate->InventoryType != INVTYPE_RELIC)
        {
            continue;
        }

        if (!IsContextuallyCompatibleWithChosenGear(
                identity,
                canonicalSpecKey,
                roleKey,
                rule,
                itemTemplate,
                selectedEntries))
        {
            continue;
        }

        if (ShouldApplyAssignedGearStatFamilyGate(rule.slot, itemTemplate)
            && !MatchesAssignedGearStatFamily(itemTemplate, statFamily))
        {
            continue;
        }

        candidates.push_back(itemTemplate);
    }

    std::uint8_t const bandStart = static_cast<std::uint8_t>(((std::max<std::uint8_t>(identity.level, 1u) - 1u) / 5u) * 5u + 1u);
    std::uint8_t const bandEnd = static_cast<std::uint8_t>(std::min<std::uint32_t>(bandStart + 4u, identity.level));

    std::vector<ItemTemplate const*> banded;
    for (ItemTemplate const* itemTemplate : candidates)
    {
        if (itemTemplate->RequiredLevel >= bandStart && itemTemplate->RequiredLevel <= bandEnd)
            banded.push_back(itemTemplate);
    }

    if (!banded.empty())
        candidates = std::move(banded);

    if (candidates.empty())
        return candidates;

    std::uint64_t totalItemLevel = 0;
    for (ItemTemplate const* itemTemplate : candidates)
        totalItemLevel += itemTemplate->ItemLevel;

    std::uint32_t const averageItemLevel = static_cast<std::uint32_t>(totalItemLevel / candidates.size());
    std::vector<ItemTemplate const*> narrowed;
    for (ItemTemplate const* itemTemplate : candidates)
    {
        if (itemTemplate->ItemLevel + 5u >= averageItemLevel
            && itemTemplate->ItemLevel <= averageItemLevel + 5u)
        {
            narrowed.push_back(itemTemplate);
        }
    }

    if (!narrowed.empty())
        candidates = std::move(narrowed);

    std::sort(
        candidates.begin(),
        candidates.end(),
        [&](ItemTemplate const* left, ItemTemplate const* right)
        {
            return ScoreItemForIdentity(identity, canonicalSpecKey, roleKey, rule.slot, left)
                > ScoreItemForIdentity(identity, canonicalSpecKey, roleKey, rule.slot, right);
        });

    if (candidates.size() > 12)
        candidates.resize(12);

    return candidates;
}

std::optional<model::WorldBotAssignedGearEntry> ChooseItemForSlot(
    integration::BotIdentityRecord const& identity,
    std::string const& canonicalSpecKey,
    std::string const& roleKey,
    SlotRule const& rule,
    std::uint8_t minQuality,
    std::uint8_t maxQuality,
    std::unordered_set<std::uint32_t>& usedItemIds,
    std::vector<model::WorldBotAssignedGearEntry> const& selectedEntries,
    std::mt19937& rng)
{
    std::vector<ItemTemplate const*> candidates = CollectCandidates(
        identity,
        canonicalSpecKey,
        roleKey,
        rule,
        minQuality,
        maxQuality,
        usedItemIds,
        selectedEntries);

    if (candidates.empty() && minQuality == ITEM_QUALITY_EPIC)
    {
        candidates = CollectCandidates(
            identity,
            canonicalSpecKey,
            roleKey,
            rule,
            ITEM_QUALITY_RARE,
            ITEM_QUALITY_RARE,
            usedItemIds,
            selectedEntries);
    }

    if (candidates.empty() && minQuality >= ITEM_QUALITY_RARE)
    {
        candidates = CollectCandidates(
            identity,
            canonicalSpecKey,
            roleKey,
            rule,
            ITEM_QUALITY_UNCOMMON,
            ITEM_QUALITY_UNCOMMON,
            usedItemIds,
            selectedEntries);
    }

    if (candidates.empty())
        return std::nullopt;

    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1u);
    ItemTemplate const* picked = candidates[dist(rng)];
    if (!picked)
        return std::nullopt;

    usedItemIds.insert(picked->ItemId);

    model::WorldBotAssignedGearEntry entry;
    entry.slot = rule.slot;
    entry.itemId = picked->ItemId;
    entry.itemLevel = picked->ItemLevel;
    entry.quality = static_cast<std::uint8_t>(picked->Quality);
    return entry;
}

std::vector<std::uint32_t> ParseItemEnchantmentValues(std::string const& text)
{
    std::vector<std::uint32_t> values;
    std::size_t cursor = 0;

    while (cursor < text.size())
    {
        while (cursor < text.size()
            && std::isspace(static_cast<unsigned char>(text[cursor])))
        {
            ++cursor;
        }

        if (cursor >= text.size())
            break;

        char* end = nullptr;
        unsigned long const parsed = std::strtoul(text.c_str() + cursor, &end, 10);
        if (end == text.c_str() + cursor)
        {
            values.push_back(0u);
            ++cursor;
            continue;
        }

        values.push_back(static_cast<std::uint32_t>(parsed));
        cursor = static_cast<std::size_t>(end - text.c_str());
    }

    return values;
}

void AccumulateWorldBotEnchantmentEntry(
    model::WorldBotAssignedGearSummary& summary,
    SpellItemEnchantmentEntry const* enchantEntry)
{
    if (!enchantEntry)
        return;

    for (std::size_t effectIndex = 0;
         effectIndex < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS;
         ++effectIndex)
    {
        std::uint32_t const effectType = enchantEntry->type[effectIndex];
        std::int32_t const amount = static_cast<std::int32_t>(enchantEntry->amount[effectIndex]);
        std::uint32_t const effectArg = enchantEntry->spellid[effectIndex];

        switch (effectType)
        {
            case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                AccumulateWorldBotAssignedGearResistance(
                    summary,
                    static_cast<SpellSchools>(effectArg),
                    amount);
                break;
            case ITEM_ENCHANTMENT_TYPE_STAT:
                AccumulateWorldBotAssignedGearStat(summary, effectArg, amount);
                break;
            default:
                break;
        }
    }
}

void AccumulateWorldBotEntryEnchantments(
    model::WorldBotAssignedGearSummary& summary,
    model::WorldBotAssignedGearEntry const& entry)
{
    if (entry.enchantments.empty())
        return;

    std::vector<std::uint32_t> const values = ParseItemEnchantmentValues(entry.enchantments);
    if (values.empty())
        return;

    for (std::size_t base = 0; base < values.size(); base += MAX_ENCHANTMENT_OFFSET)
    {
        std::uint32_t const enchantId = values[base];
        if (enchantId == 0)
            continue;

        AccumulateWorldBotEnchantmentEntry(
            summary,
            sSpellItemEnchantmentStore.LookupEntry(enchantId));
    }
}
} // namespace

WorldBotAssignedGearService::WorldBotAssignedGearService(
    integration::BotAssignedGearRepository const& assignedGearRepository,
    integration::BotAssignedGearTemplateRepository const& assignedGearTemplateRepository)
    : _assignedGearRepository(assignedGearRepository)
    , _assignedGearTemplateRepository(assignedGearTemplateRepository)
{
}

WorldBotAssignedGearResult WorldBotAssignedGearService::EnsureAssignedGear(
    integration::BotIdentityRecord& identity,
    std::string const& canonicalSpecKey,
    std::string const& roleKey) const
{
    WorldBotAssignedGearResult result;
    result.refreshBand = ComputeWorldBotGearRefreshBand(identity.level, identity.postMaxWorldOnlineMs);

    std::vector<model::WorldBotAssignedGearEntry> entries =
        _assignedGearRepository.LoadAssignments(identity.id);

    bool const needsRefresh = entries.empty()
        || identity.gearRefreshPending
        || identity.lastGearRefreshBand != result.refreshBand;

    if (needsRefresh)
    {
        static thread_local std::mt19937 rng(static_cast<std::uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));

        std::string const resolvedSpecKey = canonicalSpecKey.empty()
            ? model::CanonicalizeBotSpecKey(identity.specKey)
            : canonicalSpecKey;
        std::string const resolvedRoleKey = roleKey.empty()
            ? WorldBotPreparationService::ResolveRoleKey(identity.classId, resolvedSpecKey)
            : roleKey;
        std::uint8_t const endgameStage =
            ComputeWorldBotEndgameProgressionStage(identity.level, identity.postMaxWorldOnlineMs);
        if (identity.level >= 80)
        {
            std::vector<model::WorldBotAssignedGearEntry> curatedEntries =
                FinalizeAssignedGearEntries(_assignedGearTemplateRepository.LoadEndgameStageTemplate(
                    identity.classId,
                    resolvedSpecKey,
                    identity.loadoutKey,
                    endgameStage,
                    identity.raceId));
            if (!curatedEntries.empty())
            {
                _assignedGearRepository.ReplaceAssignments(identity.id, result.refreshBand, curatedEntries);
                identity.gearRefreshPending = false;
                identity.lastGearRefreshBand = result.refreshBand;
                result.entries = curatedEntries;
                result.summary = SummarizeAssignedGear(result.entries);
                result.refreshed = true;
                return result;
            }
        }

        AssignedGearQualityOdds const qualityOdds =
            GetAssignedGearQualityOdds(identity.level, endgameStage);

        std::unordered_set<std::uint32_t> usedItemIds;
        bool const luckyRoll =
            qualityOdds.luckyEpicSlotChance > 0
            && std::uniform_int_distribution<int>(1, 100)(rng) <= qualityOdds.luckyEpicSlotChance;
        std::optional<model::WorldBotAssignedGearEntry> luckyEpicEntry;
        if (luckyRoll)
        {
            std::vector<std::uint8_t> slotPool;
            slotPool.reserve(SlotRules.size());
            for (SlotRule const& rule : SlotRules)
                slotPool.push_back(rule.slot);

            std::shuffle(slotPool.begin(), slotPool.end(), rng);
            std::size_t const luckyAttempts = std::min<std::size_t>(3u, slotPool.size());
            for (std::size_t i = 0; i < luckyAttempts; ++i)
            {
                auto const slotIt = std::find_if(
                    SlotRules.begin(),
                    SlotRules.end(),
                    [&](SlotRule const& rule)
                    {
                        return rule.slot == slotPool[i];
                    });
                if (slotIt == SlotRules.end())
                    continue;

                std::optional<model::WorldBotAssignedGearEntry> chosenEpic = ChooseItemForSlot(
                    identity,
                    resolvedSpecKey,
                    resolvedRoleKey,
                    *slotIt,
                    ITEM_QUALITY_EPIC,
                    ITEM_QUALITY_EPIC,
                    usedItemIds,
                    {},
                    rng);
                if (chosenEpic)
                {
                    luckyEpicEntry = std::move(chosenEpic);
                    break;
                }
            }
        }

        std::vector<model::WorldBotAssignedGearEntry> generated;
        generated.reserve(SlotRules.size());
        std::vector<model::WorldBotAssignedGearEntry> plannedEntries;
        plannedEntries.reserve(SlotRules.size());
        if (luckyEpicEntry)
            plannedEntries.push_back(*luckyEpicEntry);

        for (SlotRule const& rule : SlotRules)
        {
            if (luckyEpicEntry && luckyEpicEntry->slot == rule.slot)
            {
                generated.push_back(*luckyEpicEntry);
                continue;
            }

            std::uint8_t minQuality = ITEM_QUALITY_UNCOMMON;
            std::uint8_t maxQuality = ITEM_QUALITY_UNCOMMON;
            int const qualityRoll = std::uniform_int_distribution<int>(1, 100)(rng);
            if (qualityOdds.epicChance > 0 && qualityRoll <= qualityOdds.epicChance)
            {
                minQuality = ITEM_QUALITY_EPIC;
                maxQuality = ITEM_QUALITY_EPIC;
            }
            else if (qualityRoll <= qualityOdds.rareOrBetterChance)
            {
                minQuality = ITEM_QUALITY_RARE;
                maxQuality = ITEM_QUALITY_RARE;
            }

            std::optional<model::WorldBotAssignedGearEntry> chosen = ChooseItemForSlot(
                identity,
                resolvedSpecKey,
                resolvedRoleKey,
                rule,
                minQuality,
                maxQuality,
                usedItemIds,
                plannedEntries,
                rng);
            if (!chosen)
                continue;

            generated.push_back(*chosen);
            plannedEntries.push_back(*chosen);
        }

        if (!generated.empty())
        {
            generated = FinalizeAssignedGearEntries(std::move(generated));
            _assignedGearRepository.ReplaceAssignments(identity.id, result.refreshBand, generated);
            identity.gearRefreshPending = false;
            identity.lastGearRefreshBand = result.refreshBand;
            entries = std::move(generated);
            result.refreshed = true;
        }
        else
        {
            LOG_WARN("server.worldserver",
                "[LivingWorld] WorldBotAssignedGear identity={} level={} spec='{}' could not generate any assignments",
                identity.id,
                static_cast<std::uint32_t>(identity.level),
                resolvedSpecKey);
        }
    }

    result.entries = std::move(entries);
    result.summary = SummarizeAssignedGear(result.entries);
    return result;
}

model::WorldBotAssignedGearSummary WorldBotAssignedGearService::SummarizeAssignedGear(
    std::vector<model::WorldBotAssignedGearEntry> const& entries)
{
    model::WorldBotAssignedGearSummary summary;

    for (model::WorldBotAssignedGearEntry const& entry : entries)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
        if (!itemTemplate)
            continue;

        summary.bonusArmor += static_cast<std::int32_t>(itemTemplate->Armor);
        summary.bonusBlockValue += static_cast<std::int32_t>(itemTemplate->Block);
        AccumulateWorldBotAssignedGearResistance(summary, SPELL_SCHOOL_HOLY, itemTemplate->HolyRes);
        AccumulateWorldBotAssignedGearResistance(summary, SPELL_SCHOOL_FIRE, itemTemplate->FireRes);
        AccumulateWorldBotAssignedGearResistance(summary, SPELL_SCHOOL_NATURE, itemTemplate->NatureRes);
        AccumulateWorldBotAssignedGearResistance(summary, SPELL_SCHOOL_FROST, itemTemplate->FrostRes);
        AccumulateWorldBotAssignedGearResistance(summary, SPELL_SCHOOL_SHADOW, itemTemplate->ShadowRes);
        AccumulateWorldBotAssignedGearResistance(summary, SPELL_SCHOOL_ARCANE, itemTemplate->ArcaneRes);

        for (std::uint32_t i = 0; i < itemTemplate->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        {
            _ItemStat const& stat = itemTemplate->ItemStat[i];
            AccumulateWorldBotAssignedGearStat(summary, stat.ItemStatType, stat.ItemStatValue);
        }

        AccumulateWorldBotEntryEnchantments(summary, entry);
    }

    return summary;
}
} // namespace service
} // namespace living_world
