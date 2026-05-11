#include "service/BotCombatProfilePreparationService.h"

#include "Log.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Unit.h"

namespace living_world
{
namespace service
{
namespace
{
std::uint32_t FindBestKnownSpellInChain(
    std::unordered_set<std::uint32_t> const& knownSpells,
    std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (knownSpells.count(candidate))
            return candidate;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return 0;
}

std::unordered_set<std::uint32_t> BuildSpellSetFromPlayer(Player* bot)
{
    std::unordered_set<std::uint32_t> spells;
    if (!bot)
        return spells;
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
        if (playerSpell && playerSpell->State != PLAYERSPELL_REMOVED && playerSpell->Active)
            spells.insert(spellId);
    return spells;
}
} // namespace

BotCombatProfilePreparationService::BotCombatProfilePreparationService(
    BotCombatDoctrineResolver const& doctrineResolver)
    : _doctrineResolver(doctrineResolver)
{
}

BotCombatPreparedProfile BotCombatProfilePreparationService::PrepareForPlayer(
    Player* bot,
    std::uint32_t ownerAccountId) const
{
    if (!bot)
        return {};
    return PrepareForUnit(bot, BuildSpellSetFromPlayer(bot), ownerAccountId);
}

BotCombatPreparedProfile BotCombatProfilePreparationService::PrepareForBot(
    Player* bot,
    std::uint32_t ownerAccountId) const
{
    return PrepareForPlayer(bot, ownerAccountId);
}

BotCombatPreparedProfile BotCombatProfilePreparationService::PrepareForUnit(
    Unit* unit,
    std::unordered_set<std::uint32_t> const& knownSpells,
    std::uint32_t ownerAccountId) const
{
    BotCombatPreparedProfile prepared;
    if (!unit)
        return prepared;

    prepared.resolution = _doctrineResolver.ResolveForBot(
        unit->GetGUID().GetCounter(),
        unit->getClass(),
        ownerAccountId);
    prepared.availableSpells = knownSpells;
    prepared.interruptEntries = FilterEntriesForKnownActions(
        knownSpells,
        prepared.resolution.profile.interruptEntries);
    prepared.rotationEntries = FilterEntriesForKnownActions(
        knownSpells,
        prepared.resolution.profile.rotationEntries);

    return prepared;
}

BotCombatPreparedProfile BotCombatProfilePreparationService::PrepareForWorldBot(
    Unit* unit,
    std::unordered_set<std::uint32_t> const& knownSpells,
    std::string const& specKey,
    std::string const& roleKey,
    std::string const& contextKey) const
{
    BotCombatPreparedProfile prepared;
    if (!unit)
        return prepared;

    prepared.resolution = _doctrineResolver.ResolveForWorldBot(
        unit->GetGUID().GetCounter(),
        unit->getClass(),
        specKey,
        roleKey,
        contextKey);
    prepared.availableSpells = knownSpells;
    prepared.interruptEntries = FilterEntriesForKnownActions(
        knownSpells,
        prepared.resolution.profile.interruptEntries);
    prepared.rotationEntries = FilterEntriesForKnownActions(
        knownSpells,
        prepared.resolution.profile.rotationEntries);

    return prepared;
}

std::uint32_t BotCombatProfilePreparationService::ResolveKnownSpellForAction(
    std::unordered_set<std::uint32_t> const& knownSpells,
    model::BotCombatActionDefinition const& action)
{
    if (action.actionType != model::BotCombatActionType::Spell || action.spellBaseId == 0)
        return 0;

    switch (action.rankMode)
    {
        case model::BotCombatRankMode::BestKnown:
            return FindBestKnownSpellInChain(knownSpells, action.spellBaseId);

        case model::BotCombatRankMode::ExactSpellId:
            return knownSpells.count(action.spellBaseId) ? action.spellBaseId : 0;

        case model::BotCombatRankMode::SpecificRank:
        {
            if (action.rankValue == 0)
                return 0;

            std::uint32_t candidate =
                sSpellMgr->GetFirstSpellInChain(action.spellBaseId);
            if (!candidate)
                candidate = action.spellBaseId;

            std::uint8_t rank = 1;
            while (candidate)
            {
                if (rank == action.rankValue)
                    return knownSpells.count(candidate) ? candidate : 0;
                candidate = sSpellMgr->GetNextSpellInChain(candidate);
                ++rank;
            }
            return 0;
        }
    }

    return 0;
}

bool BotCombatProfilePreparationService::IsActionUsableByBot(
    std::unordered_set<std::uint32_t> const& knownSpells,
    model::BotCombatActionDefinition const& action)
{
    switch (action.actionType)
    {
        case model::BotCombatActionType::Spell:
            return ResolveKnownSpellForAction(knownSpells, action) != 0;
        case model::BotCombatActionType::Item:
            return action.itemId != 0;
    }
    return false;
}

std::optional<model::BotCombatEntryDefinition>
BotCombatProfilePreparationService::FilterEntryForKnownActions(
    std::unordered_set<std::uint32_t> const& knownSpells,
    model::BotCombatEntryDefinition entry)
{
    if (!entry.enabled)
        return std::nullopt;

    bool const primaryUsable   = IsActionUsableByBot(knownSpells, entry.primaryAction);
    bool const secondaryUsable =
        entry.secondaryAction && IsActionUsableByBot(knownSpells, *entry.secondaryAction);

    if (!primaryUsable && !secondaryUsable)
        return std::nullopt;

    if (!primaryUsable && secondaryUsable)
    {
        entry.primaryAction = *entry.secondaryAction;
        entry.primaryAction.slot = 0;
        entry.secondaryAction.reset();
    }
    else if (entry.secondaryAction && !secondaryUsable)
    {
        entry.secondaryAction.reset();
    }

    return entry;
}

std::vector<model::BotCombatEntryDefinition>
BotCombatProfilePreparationService::FilterEntriesForKnownActions(
    std::unordered_set<std::uint32_t> const& knownSpells,
    std::vector<model::BotCombatEntryDefinition> const& entries)
{
    std::vector<model::BotCombatEntryDefinition> filtered;
    filtered.reserve(entries.size());
    for (auto const& entry : entries)
        if (auto resolved = FilterEntryForKnownActions(knownSpells, entry))
            filtered.push_back(std::move(*resolved));
    return filtered;
}
} // namespace service
} // namespace living_world
