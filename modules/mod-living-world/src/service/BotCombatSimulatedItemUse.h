#pragma once

#include "Creature.h"
#include "Globals/ObjectMgr.h"
#include "ItemTemplate.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace living_world
{
namespace service
{
struct SimulatedCombatItemDefinition
{
    std::uint32_t itemId = 0;
    std::uint32_t useSpellId = 0;
    std::uint32_t cooldownMs = 0;
    bool oncePerCombat = false;
};

inline std::optional<SimulatedCombatItemDefinition> GetSimulatedCombatItemDefinition(std::uint32_t itemId)
{
    if (itemId == 0)
        return std::nullopt;

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
        return std::nullopt;

    // World bots have no real bags, so their doctrine-backed item actions need a
    // bagless fallback even when the underlying consumable is not flagged
    // conjured in item_template. Keep this narrow to self-use consumables.
    if (itemTemplate->Class != ITEM_CLASS_CONSUMABLE)
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
            true
        };
    }

    return std::nullopt;
}

inline bool CanUseSimulatedCombatItem(
    Unit* bot,
    Unit* target,
    std::uint32_t itemId,
    std::unordered_set<std::uint32_t> const* usedItemsThisCombat = nullptr)
{
    if (!bot || !target || target != bot)
        return false;

    Creature* creature = bot->ToCreature();
    if (!creature)
        return false;

    std::optional<SimulatedCombatItemDefinition> definition =
        GetSimulatedCombatItemDefinition(itemId);
    if (!definition)
        return false;

    if (definition->oncePerCombat && bot->IsInCombat() && usedItemsThisCombat
        && usedItemsThisCombat->count(itemId) > 0)
    {
        return false;
    }

    if (creature->HasSpellCooldown(definition->useSpellId))
        return false;

    return true;
}
} // namespace service
} // namespace living_world