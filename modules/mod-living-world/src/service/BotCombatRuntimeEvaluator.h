#pragma once

#include "service/BotCombatProfilePreparationService.h"

#include <optional>
#include <string>
#include <unordered_set>

class Player;
class Unit;

namespace living_world
{
namespace service
{
struct BotCombatRuntimeContext
{
    Unit* bot = nullptr;      // Player* for session bots, Creature* for world bots
    Player* owner = nullptr;  // Always a real player or nullptr
    Unit* primaryTarget = nullptr;
    std::uint32_t rotationWaitMs = 500;
    // Spells available to this bot. For Player session bots this is built from
    // GetSpellMap() during PrepareForPlayer. For creature bots it is loaded from
    // living_world_bot_spell_list. Must not be empty when bot is a Creature.
    std::unordered_set<std::uint32_t> availableSpells;
};

enum class BotCombatEvaluationDisposition : std::uint8_t
{
    None = 0,
    Wait = 1,
    Cast = 2
};

struct BotCombatEvaluatedAction
{
    std::uint64_t entryId = 0;
    std::uint64_t actionId = 0;
    std::uint32_t spellId = 0;
    Unit* target = nullptr;
    std::string targetKey;
    bool breaksCurrentCast = false;
};

struct BotCombatEvaluationResult
{
    BotCombatEvaluationDisposition disposition =
        BotCombatEvaluationDisposition::None;
    std::optional<BotCombatEvaluatedAction> action;
    std::uint32_t waitMs = 0;
};

class BotCombatRuntimeEvaluator
{
public:
    [[nodiscard]] BotCombatEvaluationResult EvaluateInterrupts(
        BotCombatPreparedProfile const& preparedProfile,
        BotCombatRuntimeContext const& context) const;

    [[nodiscard]] BotCombatEvaluationResult EvaluateRotation(
        BotCombatPreparedProfile const& preparedProfile,
        BotCombatRuntimeContext const& context) const;

private:
    [[nodiscard]] static BotCombatEvaluationResult EvaluateEntries(
        std::vector<model::BotCombatEntryDefinition> const& entries,
        BotCombatRuntimeContext const& context);

    static bool EvaluateEntryConditions(
        model::BotCombatEntryDefinition const& entry,
        BotCombatRuntimeContext const& context);

    static bool EvaluateCondition(
        model::BotCombatConditionDefinition const& condition,
        BotCombatRuntimeContext const& context);

    static std::optional<BotCombatEvaluatedAction> EvaluateAction(
        model::BotCombatEntryDefinition const& entry,
        model::BotCombatActionDefinition const& action,
        BotCombatRuntimeContext const& context);

    static std::uint32_t GetCurrentCastHoldWaitMs(
        model::BotCombatEntryDefinition const& entry,
        model::BotCombatActionDefinition const& action,
        BotCombatRuntimeContext const& context);

    static std::uint32_t GetActionWaitMs(
        model::BotCombatActionDefinition const& action,
        BotCombatRuntimeContext const& context);

    static Unit* ResolveConditionSubject(
        std::string const& subjectKey,
        BotCombatRuntimeContext const& context);

    static Unit* ResolveActionTarget(
        std::string const& targetKey,
        BotCombatRuntimeContext const& context);

    static bool CanExecuteSpell(
        Unit* bot,
        Unit* target,
        std::uint32_t spellId);

    static std::uint32_t GetSpellWaitMs(
        Unit* bot,
        Unit* target,
        std::uint32_t spellId);

    static bool CanBreakCurrentCast(
        Unit* bot,
        BotCombatEvaluatedAction const& evaluatedAction);

    static bool HasInterruptibleEnemyCast(Unit* target);
};
} // namespace service
} // namespace living_world
