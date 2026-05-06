#pragma once

#include "model/BotCombatProfile.h"
#include "service/BotCombatDoctrineResolver.h"

#include <cstdint>
#include <optional>
#include <vector>

class Player;

namespace living_world
{
namespace service
{
struct BotCombatPreparedProfile
{
    BotCombatDoctrineResolution resolution;
    std::vector<model::BotCombatEntryDefinition> interruptEntries;
    std::vector<model::BotCombatEntryDefinition> rotationEntries;
};

class BotCombatProfilePreparationService
{
public:
    explicit BotCombatProfilePreparationService(
        BotCombatDoctrineResolver const& doctrineResolver);

    [[nodiscard]] BotCombatPreparedProfile PrepareForBot(
        Player* bot,
        std::uint32_t ownerAccountId) const;

    static std::uint32_t ResolveKnownSpellForAction(
        Player* bot,
        model::BotCombatActionDefinition const& action);

private:
    static bool IsActionUsableByBot(
        Player* bot,
        model::BotCombatActionDefinition const& action);

    static std::optional<model::BotCombatEntryDefinition> FilterEntryForKnownActions(
        Player* bot,
        model::BotCombatEntryDefinition entry);

    static std::vector<model::BotCombatEntryDefinition> FilterEntriesForKnownActions(
        Player* bot,
        std::vector<model::BotCombatEntryDefinition> const& entries);

    BotCombatDoctrineResolver const& _doctrineResolver;
};
} // namespace service
} // namespace living_world
