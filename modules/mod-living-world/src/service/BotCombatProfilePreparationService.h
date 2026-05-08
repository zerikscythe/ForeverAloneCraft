#pragma once

#include "model/BotCombatProfile.h"
#include "service/BotCombatDoctrineResolver.h"

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

class Player;
class Unit;

namespace living_world
{
namespace service
{
struct BotCombatPreparedProfile
{
    BotCombatDoctrineResolution resolution;
    std::vector<model::BotCombatEntryDefinition> interruptEntries;
    std::vector<model::BotCombatEntryDefinition> rotationEntries;
    // The spell set used to prepare this profile. Stored here so the runtime
    // evaluator can reference it without re-querying.
    std::unordered_set<std::uint32_t> availableSpells;
};

class BotCombatProfilePreparationService
{
public:
    explicit BotCombatProfilePreparationService(
        BotCombatDoctrineResolver const& doctrineResolver);

    // Player session bots — builds spell set from player's learned spell map.
    [[nodiscard]] BotCombatPreparedProfile PrepareForPlayer(
        Player* bot,
        std::uint32_t ownerAccountId) const;

    // Creature bots — uses caller-provided spell set (from living_world_bot_spell_list).
    [[nodiscard]] BotCombatPreparedProfile PrepareForUnit(
        Unit* unit,
        std::unordered_set<std::uint32_t> const& knownSpells,
        std::uint32_t ownerAccountId) const;

    // Backwards-compat alias for Player session bots.
    [[nodiscard]] BotCombatPreparedProfile PrepareForBot(
        Player* bot,
        std::uint32_t ownerAccountId) const;

    static std::uint32_t ResolveKnownSpellForAction(
        std::unordered_set<std::uint32_t> const& knownSpells,
        model::BotCombatActionDefinition const& action);

private:
    static bool IsActionUsableByBot(
        std::unordered_set<std::uint32_t> const& knownSpells,
        model::BotCombatActionDefinition const& action);

    static std::optional<model::BotCombatEntryDefinition> FilterEntryForKnownActions(
        std::unordered_set<std::uint32_t> const& knownSpells,
        model::BotCombatEntryDefinition entry);

    static std::vector<model::BotCombatEntryDefinition> FilterEntriesForKnownActions(
        std::unordered_set<std::uint32_t> const& knownSpells,
        std::vector<model::BotCombatEntryDefinition> const& entries);

    BotCombatDoctrineResolver const& _doctrineResolver;
};
} // namespace service
} // namespace living_world
