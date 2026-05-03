#include "service/BotCombatProfilePreparationService.h"

#include "Log.h"
#include "Player.h"
#include "SpellMgr.h"

namespace living_world
{
namespace service
{
namespace
{
std::uint32_t FindBestKnownSpellInChain(Player* bot, std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (bot->HasSpell(candidate))
            return candidate;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return 0;
}
} // namespace

BotCombatProfilePreparationService::BotCombatProfilePreparationService(
    BotCombatDoctrineResolver const& doctrineResolver)
    : _doctrineResolver(doctrineResolver)
{
}

BotCombatPreparedProfile BotCombatProfilePreparationService::PrepareForBot(
    Player* bot,
    std::uint32_t ownerAccountId) const
{
    BotCombatPreparedProfile prepared;
    if (!bot)
        return prepared;

    prepared.resolution = _doctrineResolver.ResolveForBot(
        bot->GetGUID().GetCounter(),
        bot->getClass(),
        ownerAccountId);
    prepared.interruptEntries = FilterEntriesForKnownActions(
        bot,
        prepared.resolution.profile.interruptEntries);
    prepared.rotationEntries = FilterEntriesForKnownActions(
        bot,
        prepared.resolution.profile.rotationEntries);

    return prepared;
}

std::uint32_t BotCombatProfilePreparationService::ResolveKnownSpellForAction(
    Player* bot,
    model::BotCombatActionDefinition const& action)
{
    if (!bot || action.actionType != model::BotCombatActionType::Spell ||
        action.spellBaseId == 0)
        return 0;

    switch (action.rankMode)
    {
        case model::BotCombatRankMode::BestKnown:
        {
            std::uint32_t const spellId =
                FindBestKnownSpellInChain(bot, action.spellBaseId);
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] ProfileSpellResolve bot='{}' guid={} actionId={} rankMode=BestKnown baseSpellId={} resolvedSpellId={}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                action.actionId,
                action.spellBaseId,
                spellId);
            return spellId;
        }
        case model::BotCombatRankMode::ExactSpellId:
        {
            std::uint32_t const spellId =
                bot->HasSpell(action.spellBaseId) ? action.spellBaseId : 0;
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] ProfileSpellResolve bot='{}' guid={} actionId={} rankMode=ExactSpellId baseSpellId={} resolvedSpellId={}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                action.actionId,
                action.spellBaseId,
                spellId);
            return spellId;
        }
        case model::BotCombatRankMode::SpecificRank:
        {
            if (action.rankValue == 0)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] ProfileSpellResolve bot='{}' guid={} actionId={} rankMode=SpecificRank baseSpellId={} requestedRank={} resolvedSpellId=0 reason=zero_rank",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    action.actionId,
                    action.spellBaseId,
                    static_cast<std::uint32_t>(action.rankValue));
                return 0;
            }

            std::uint32_t candidate =
                sSpellMgr->GetFirstSpellInChain(action.spellBaseId);
            if (!candidate)
                candidate = action.spellBaseId;

            std::uint8_t rank = 1;
            while (candidate)
            {
                if (rank == action.rankValue)
                {
                    std::uint32_t const spellId =
                        bot->HasSpell(candidate) ? candidate : 0;
                    LOG_INFO(
                        "server.worldserver",
                        "[LivingWorldDebug] ProfileSpellResolve bot='{}' guid={} actionId={} rankMode=SpecificRank baseSpellId={} requestedRank={} resolvedSpellId={}",
                        bot->GetName(),
                        bot->GetGUID().GetCounter(),
                        action.actionId,
                        action.spellBaseId,
                        static_cast<std::uint32_t>(action.rankValue),
                        spellId);
                    return spellId;
                }
                candidate = sSpellMgr->GetNextSpellInChain(candidate);
                ++rank;
            }

            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] ProfileSpellResolve bot='{}' guid={} actionId={} rankMode=SpecificRank baseSpellId={} requestedRank={} resolvedSpellId=0 reason=rank_not_found",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                action.actionId,
                action.spellBaseId,
                static_cast<std::uint32_t>(action.rankValue));
            return 0;
        }
    }

    return 0;
}

bool BotCombatProfilePreparationService::IsActionUsableByBot(
    Player* bot,
    model::BotCombatActionDefinition const& action)
{
    switch (action.actionType)
    {
        case model::BotCombatActionType::Spell:
            return ResolveKnownSpellForAction(bot, action) != 0;
        case model::BotCombatActionType::Item:
            return action.itemId != 0;
    }

    return false;
}

std::optional<model::BotCombatEntryDefinition>
BotCombatProfilePreparationService::FilterEntryForKnownActions(
    Player* bot,
    model::BotCombatEntryDefinition entry)
{
    if (!entry.enabled)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] ProfileEntryFiltered bot='{}' guid={} entryId={} label='{}' reason=disabled",
            bot ? bot->GetName() : "",
            bot ? bot->GetGUID().GetCounter() : 0,
            entry.entryId,
            entry.label);
        return std::nullopt;
    }

    bool const primaryUsable = IsActionUsableByBot(bot, entry.primaryAction);
    bool const secondaryUsable =
        entry.secondaryAction && IsActionUsableByBot(bot, *entry.secondaryAction);

    if (!primaryUsable && !secondaryUsable)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] ProfileEntryFiltered bot='{}' guid={} entryId={} label='{}' reason=no_usable_actions primaryActionId={} secondaryActionId={}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            entry.entryId,
            entry.label,
            entry.primaryAction.actionId,
            entry.secondaryAction ? entry.secondaryAction->actionId : 0);
        return std::nullopt;
    }

    if (!primaryUsable && secondaryUsable)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] ProfileEntryFiltered bot='{}' guid={} entryId={} label='{}' action=promote_secondary primaryActionId={} secondaryActionId={}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            entry.entryId,
            entry.label,
            entry.primaryAction.actionId,
            entry.secondaryAction ? entry.secondaryAction->actionId : 0);
        entry.primaryAction = *entry.secondaryAction;
        entry.primaryAction.slot = 0;
        entry.secondaryAction.reset();
    }
    else if (entry.secondaryAction && !secondaryUsable)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] ProfileEntryFiltered bot='{}' guid={} entryId={} label='{}' action=drop_secondary secondaryActionId={}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            entry.entryId,
            entry.label,
            entry.secondaryAction->actionId);
        entry.secondaryAction.reset();
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] ProfileEntryAccepted bot='{}' guid={} entryId={} label='{}' primaryActionId={} secondaryActionId={} isInterrupt={} priority={}",
        bot->GetName(),
        bot->GetGUID().GetCounter(),
        entry.entryId,
        entry.label,
        entry.primaryAction.actionId,
        entry.secondaryAction ? entry.secondaryAction->actionId : 0,
        entry.isInterrupt,
        static_cast<std::uint32_t>(entry.priority));

    return entry;
}

std::vector<model::BotCombatEntryDefinition>
BotCombatProfilePreparationService::FilterEntriesForKnownActions(
    Player* bot,
    std::vector<model::BotCombatEntryDefinition> const& entries)
{
    std::vector<model::BotCombatEntryDefinition> filtered;
    filtered.reserve(entries.size());

    for (auto const& entry : entries)
    {
        if (auto resolved = FilterEntryForKnownActions(bot, entry))
            filtered.push_back(std::move(*resolved));
    }

    return filtered;
}
} // namespace service
} // namespace living_world
