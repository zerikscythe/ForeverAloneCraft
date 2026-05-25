#pragma once

#include "service/BotCombatRuntimeEvaluator.h"
#include "service/BotCombatSimulatedItemUse.h"

#include "Creature.h"
#include "Player.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"

namespace living_world
{
namespace service
{
struct BotCombatActionDispatchResult
{
    bool dispatched = false;
    std::string reason;
    std::uint32_t resolvedSpellId = 0;
};

namespace
{
inline bool TryExecuteSimulatedCombatItem(Unit* bot, BotCombatEvaluatedAction const& action)
{
    if (!bot || action.itemId == 0)
        return false;

    Creature* creature = bot->ToCreature();
    if (!creature)
        return false;

    std::optional<SimulatedCombatItemDefinition> definition =
        GetSimulatedCombatItemDefinition(action.itemId);
    if (!definition)
        return false;

    if (creature->HasSpellCooldown(definition->useSpellId))
        return false;

    Unit* target = action.target ? action.target : bot;
    bot->CastSpell(target, definition->useSpellId, true);
    creature->AddSpellCooldown(definition->useSpellId, action.itemId, definition->cooldownMs);
    return true;
}
} // namespace

inline BotCombatActionDispatchResult DispatchEvaluatedAction(Unit* bot, BotCombatEvaluatedAction const& action)
{
    if (!bot)
        return { false, "missing_bot", 0 };

    if (action.actionType == model::BotCombatActionType::Item)
    {
        if (action.simulatedItemUse)
        {
            std::optional<SimulatedCombatItemDefinition> definition =
                GetSimulatedCombatItemDefinition(action.itemId);
            if (!definition)
                return { false, "missing_simulated_item_definition", 0 };

            bool const dispatched = TryExecuteSimulatedCombatItem(bot, action);
            return {
                dispatched,
                dispatched ? "simulated_item_dispatched" : "simulated_item_rejected",
                definition->useSpellId
            };
        }

        Player* player = bot->ToPlayer();
        if (!player || action.itemId == 0)
            return { false, "missing_player_or_item", 0 };

        Item* item = nullptr;
        if (action.equippedSlot != 255)
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, action.equippedSlot);
        else
            item = player->GetItemByEntry(action.itemId);
        if (!item)
            return { false, "item_not_found", 0 };

        if (player->CanUseItem(item) != EQUIP_ERR_OK)
            return { false, "item_cannot_be_used", 0 };

        SpellCastTargets targets;
        targets.SetUnitTarget(action.target ? action.target : player);
        player->CastItemUseSpell(item, targets, 1, 0);
        return { true, "item_use_dispatched", 0 };
    }

    if (action.useDestination)
    {
        bot->CastSpell(
            action.destinationX,
            action.destinationY,
            action.destinationZ,
            action.spellId,
            false);
        return { true, "ground_target_spell_dispatched", action.spellId };
    }

    if (!action.target)
        return { false, "missing_action_target", action.spellId };

    bot->CastSpell(action.target, action.spellId, false);
    return { true, "unit_target_spell_dispatched", action.spellId };
}

inline bool CastEvaluatedAction(Unit* bot, BotCombatEvaluatedAction const& action)
{
    return DispatchEvaluatedAction(bot, action).dispatched;
}
} // namespace service
} // namespace living_world
