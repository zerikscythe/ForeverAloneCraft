#pragma once

#include "Creature.h"
#include "Globals/ObjectMgr.h"
#include "ItemTemplate.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace living_world
{
namespace service
{
inline constexpr std::array<std::uint32_t, 7> GenericHealingPotionFamilyDescending = {
    33447, // Runic Healing Potion
    22829, // Super Healing Potion
    13446, // Major Healing Potion
    3928,  // Superior Healing Potion
    1710,  // Greater Healing Potion
    929,   // Healing Potion
    118    // Minor Healing Potion
};

inline constexpr std::array<std::uint32_t, 8> GenericManaPotionFamilyDescending = {
    33448, // Runic Mana Potion
    22832, // Super Mana Potion
    13444, // Major Mana Potion
    13443, // Superior Mana Potion
    6149,  // Greater Mana Potion
    3827,  // Mana Potion
    3385,  // Lesser Mana Potion
    2455   // Minor Mana Potion
};

inline constexpr std::array<std::uint32_t, 6> GenericManaGemFamilyDescending = {
    33312, // Mana Sapphire
    22044, // Mana Emerald
    8008,  // Mana Ruby
    8007,  // Mana Citrine
    5513,  // Mana Jade
    5514   // Mana Agate
};

inline constexpr std::array<std::uint32_t, 8> GenericHealthstoneFamilyDescending = {
    36894, // Fel Healthstone
    36891, // Demonic Healthstone
    22105, // Master Healthstone
    19013, // Major Healthstone
    19011, // Greater Healthstone
    19009, // Healthstone
    19007, // Lesser Healthstone
    19005  // Minor Healthstone
};

inline std::string NormalizeCombatItemSelector(std::string_view selector)
{
    std::string normalized(selector);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

inline bool IsGenericHealingPotionSelector(std::string_view selector)
{
    std::string const normalized = NormalizeCombatItemSelector(selector);
    return normalized == "hp"
        || normalized == "health"
        || normalized == "heal"
        || normalized == "healing";
}

inline bool IsGenericManaPotionSelector(std::string_view selector)
{
    std::string const normalized = NormalizeCombatItemSelector(selector);
    return normalized == "mp" || normalized == "mana";
}

inline bool IsTrinket1Selector(std::string_view selector)
{
    std::string const normalized = NormalizeCombatItemSelector(selector);
    return normalized == "trinket1" || normalized == "trinket_1";
}

inline bool IsTrinket2Selector(std::string_view selector)
{
    std::string const normalized = NormalizeCombatItemSelector(selector);
    return normalized == "trinket2" || normalized == "trinket_2";
}

inline bool IsHealthstoneSelector(std::string_view selector)
{
    std::string const normalized = NormalizeCombatItemSelector(selector);
    return normalized == "healthstone" || normalized == "hs";
}

inline bool IsManaGemSelector(std::string_view selector)
{
    std::string const normalized = NormalizeCombatItemSelector(selector);
    return normalized == "managem"
        || normalized == "mana_gem"
        || normalized == "mana-gem"
        || normalized == "mana gem";
}

template <std::size_t N>
inline std::uint32_t ResolveBestPotionFromFamilyForLevel(
    std::uint8_t level,
    std::array<std::uint32_t, N> const& familyDescending)
{
    for (std::uint32_t itemId : familyDescending)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate || !itemTemplate->IsPotion())
            continue;

        if (level >= itemTemplate->RequiredLevel)
            return itemId;
    }

    return 0;
}

inline std::uint32_t ResolveGenericHealingPotionItemIdForLevel(std::uint8_t level)
{
    return ResolveBestPotionFromFamilyForLevel(level, GenericHealingPotionFamilyDescending);
}

inline std::uint32_t ResolveGenericManaPotionItemIdForLevel(std::uint8_t level)
{
    return ResolveBestPotionFromFamilyForLevel(level, GenericManaPotionFamilyDescending);
}

inline std::uint32_t ResolveGenericManaGemItemIdForLevel(std::uint8_t level)
{
    return ResolveBestPotionFromFamilyForLevel(level, GenericManaGemFamilyDescending);
}

inline std::uint32_t ResolveGenericHealthstoneItemIdForLevel(std::uint8_t level)
{
    return ResolveBestPotionFromFamilyForLevel(level, GenericHealthstoneFamilyDescending);
}

inline std::uint32_t ResolveGenericPotionItemIdForLevel(
    std::uint8_t level,
    std::string_view selector)
{
    if (IsGenericHealingPotionSelector(selector))
        return ResolveGenericHealingPotionItemIdForLevel(level);

    if (IsGenericManaPotionSelector(selector))
        return ResolveGenericManaPotionItemIdForLevel(level);

    if (IsManaGemSelector(selector))
        return ResolveGenericManaGemItemIdForLevel(level);

    if (IsHealthstoneSelector(selector))
        return ResolveGenericHealthstoneItemIdForLevel(level);

    return 0;
}

struct SimulatedCombatItemDefinition
{
    std::uint32_t itemId = 0;
    std::uint32_t useSpellId = 0;
    std::uint32_t cooldownMs = 0;
    bool oncePerCombat = false;
    bool countsAsPotionUse = false;
    bool requiresEquipped = false;
};

inline std::optional<SimulatedCombatItemDefinition> GetSimulatedCombatItemDefinition(std::uint32_t itemId)
{
    if (itemId == 0)
        return std::nullopt;

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return std::nullopt;

    bool const isConsumable = itemTemplate->Class == ITEM_CLASS_CONSUMABLE;
    bool const isTrinket = itemTemplate->InventoryType == INVTYPE_TRINKET;

    // Generic world-bot item use should stay narrow enough to be predictable:
    // real consumables and equipped on-use trinkets are the first useful
    // families. We can widen this later if another item family proves clean.
    if (!isConsumable && !isTrinket)
        return std::nullopt;

    for (std::uint8_t i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = itemTemplate->Spells[i];
        if (spellData.SpellId <= 0)
            continue;

        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE
            && spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
        {
            continue;
        }

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(static_cast<std::uint32_t>(spellData.SpellId));
        if (!spellInfo)
            continue;

        std::uint32_t cooldownMs = 0;
        if (spellData.SpellCooldown > 0)
            cooldownMs = std::max(cooldownMs, static_cast<std::uint32_t>(spellData.SpellCooldown));
        if (spellData.SpellCategoryCooldown > 0)
            cooldownMs = std::max(cooldownMs, static_cast<std::uint32_t>(spellData.SpellCategoryCooldown));
        cooldownMs = std::max(cooldownMs, spellInfo->RecoveryTime);
        cooldownMs = std::max(cooldownMs, spellInfo->CategoryRecoveryTime);

        // World bots have no real bags, so conjured consumables are treated as
        // an out-of-band recreation path between combats rather than as true
        // persistent inventory. Keep the normal item cooldown when it exists,
        // but never allow an immediate back-to-back recreation window.
        cooldownMs = std::max<std::uint32_t>(cooldownMs, 60000u);

        return SimulatedCombatItemDefinition{
            itemId,
            spellInfo->Id,
            cooldownMs,
            true,
            itemTemplate->IsPotion(),
            isTrinket
        };
    }

    return std::nullopt;
}

inline bool CanUseSimulatedCombatItem(
    Unit* bot,
    Unit* target,
    std::uint32_t itemId,
    std::unordered_set<std::uint32_t> const* usedItemsThisCombat = nullptr,
    std::unordered_set<std::uint32_t> const* equippedWorldBotItemIds = nullptr,
    std::uint8_t const* simulatedPotionUsesThisSession = nullptr,
    std::uint8_t simulatedPotionUseLimit = 0,
    std::uint32_t syntheticGlobalCooldownRemainingMs = 0)
{
    if (!bot || !target)
        return false;

    Creature* creature = bot->ToCreature();
    if (!creature)
        return false;

    std::optional<SimulatedCombatItemDefinition> definition =
        GetSimulatedCombatItemDefinition(itemId);
    if (!definition)
        return false;

    if (definition->requiresEquipped)
    {
        if (!equippedWorldBotItemIds || equippedWorldBotItemIds->count(itemId) == 0)
            return false;
    }

    if (definition->countsAsPotionUse
        && simulatedPotionUsesThisSession
        && *simulatedPotionUsesThisSession >= simulatedPotionUseLimit)
    {
        return false;
    }

    if (definition->oncePerCombat && bot->IsInCombat() && usedItemsThisCombat
        && usedItemsThisCombat->count(itemId) > 0)
    {
        return false;
    }

    if (syntheticGlobalCooldownRemainingMs > 0)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(definition->useSpellId);
        if (spellInfo && spellInfo->StartRecoveryTime > 0)
            return false;
    }

    if (creature->HasSpellCooldown(definition->useSpellId))
        return false;

    return true;
}

inline bool DoesSimulatedCombatItemCountAsPotionUse(std::uint32_t itemId)
{
    std::optional<SimulatedCombatItemDefinition> definition =
        GetSimulatedCombatItemDefinition(itemId);
    return definition && definition->countsAsPotionUse;
}
} // namespace service
} // namespace living_world
