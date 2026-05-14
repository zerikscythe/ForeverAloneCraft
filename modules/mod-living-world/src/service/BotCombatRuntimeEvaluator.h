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
    bool allowHardCasts = true;
    std::unordered_set<std::uint32_t> const* usedSimulatedItemsThisCombat = nullptr;
    std::uint32_t rotationWaitMs = 500;
    model::BotCombatAoEMode defaultAoEMode = model::BotCombatAoEMode::Centroid;
    std::uint8_t defaultAoEMinTargets = 2;
    float defaultAoEScanRadius = 10.0f;
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
    model::BotCombatActionType actionType = model::BotCombatActionType::Spell;
    std::uint32_t spellId = 0;
    std::uint32_t itemId = 0;
    bool simulatedItemUse = false;
    Unit* target = nullptr;
    std::string targetKey;
    std::string entryLabel;
    bool isInterrupt = false;
    std::uint8_t actionSlot = 0;
    std::optional<model::BotCombatAoEMode> aoeMode;
    std::optional<std::uint8_t> aoeMinTargets;
    std::optional<float> aoeRadius;
    bool useDestination = false;
    float destinationX = 0.0f;
    float destinationY = 0.0f;
    float destinationZ = 0.0f;
    bool breaksCurrentCast = false;
};

struct BotCombatEvaluationResult
{
    BotCombatEvaluationDisposition disposition =
        BotCombatEvaluationDisposition::None;
    std::optional<BotCombatEvaluatedAction> action;
    std::uint32_t waitMs = 0;
    std::uint64_t traceEntryId = 0;
    std::uint64_t traceActionId = 0;
    std::uint32_t traceSpellId = 0;
    std::string traceEntryLabel;
    std::string traceTargetKey;
    std::string traceReason;
};

class BotCombatRuntimeEvaluator
{
public:
    [[nodiscard]] static std::uint32_t CountPartyMembersBelowHealthPct(
        Unit* bot,
        Player* owner,
        float thresholdPct);

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
        std::uint32_t spellId,
        bool allowHardCasts);

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
