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

inline bool CastEvaluatedAction(Unit* bot, BotCombatEvaluatedAction const& action)
{
    if (!bot)
        return false;

    if (action.actionType == model::BotCombatActionType::Item)
    {
        if (action.simulatedItemUse)
            return TryExecuteSimulatedCombatItem(bot, action);

        Player* player = bot->ToPlayer();
        if (!player || action.itemId == 0)
            return false;

        Item* item = player->GetItemByEntry(action.itemId);
        if (!item)
            return false;

        if (player->CanUseItem(item) != EQUIP_ERR_OK)
            return false;

        SpellCastTargets targets;
        targets.SetUnitTarget(action.target ? action.target : player);
        player->CastItemUseSpell(item, targets, 1, 0);
        return true;
    }

    if (action.useDestination)
    {
        bot->CastSpell(
            action.destinationX,
            action.destinationY,
            action.destinationZ,
            action.spellId,
            false);
        return true;
    }

    if (!action.target)
        return false;

    bot->CastSpell(action.target, action.spellId, false);
    return true;
}
} // namespace service
} // namespace living_world