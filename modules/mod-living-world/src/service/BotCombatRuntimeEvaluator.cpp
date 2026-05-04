#include "service/BotCombatRuntimeEvaluator.h"

#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{
bool CompareNumeric(
    model::BotCombatConditionOperator op,
    float left,
    float right)
{
    switch (op)
    {
        case model::BotCombatConditionOperator::Equal:
            return std::fabs(left - right) < 0.01f;
        case model::BotCombatConditionOperator::NotEqual:
            return std::fabs(left - right) >= 0.01f;
        case model::BotCombatConditionOperator::LessThan:
            return left < right;
        case model::BotCombatConditionOperator::LessThanOrEqual:
            return left <= right;
        case model::BotCombatConditionOperator::GreaterThan:
            return left > right;
        case model::BotCombatConditionOperator::GreaterThanOrEqual:
            return left >= right;
        case model::BotCombatConditionOperator::Has:
        case model::BotCombatConditionOperator::NotHas:
        case model::BotCombatConditionOperator::Exists:
            return false;
    }

    return false;
}

std::optional<std::uint32_t> ParseConditionSpellId(
    model::BotCombatConditionDefinition const& condition)
{
    if (condition.numericValue > 0.0f)
        return static_cast<std::uint32_t>(condition.numericValue);

    if (condition.stringValue.empty())
        return std::nullopt;

    std::uint32_t spellId = 0;
    auto const* begin = condition.stringValue.data();
    auto const* end = begin + condition.stringValue.size();
    auto const result = std::from_chars(begin, end, spellId);
    if (result.ec == std::errc{} && result.ptr == end)
        return spellId;

    return std::nullopt;
}

float GetManaPct(Unit* unit)
{
    if (!unit)
        return 0.0f;

    if (unit->GetMaxPower(POWER_MANA) == 0)
        return 100.0f;

    return 100.0f * static_cast<float>(unit->GetPower(POWER_MANA)) /
        static_cast<float>(unit->GetMaxPower(POWER_MANA));
}

Player* FindLowestHealthPartyTarget(Player* bot, Player* owner)
{
    Player* lowest = nullptr;
    auto consider = [&](Player* candidate)
    {
        if (!candidate || !candidate->IsAlive() || !candidate->IsInWorld())
            return;
        if (!lowest || candidate->GetHealthPct() < lowest->GetHealthPct())
            lowest = candidate;
    };

    consider(owner);
    consider(bot);

    if (owner)
    {
        if (Group const* group = owner->GetGroup())
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                consider(ObjectAccessor::FindConnectedPlayer(slot.guid));
            }
        }
    }

    return lowest;
}

Unit* FindTrashTarget(BotCombatRuntimeContext const& context)
{
    if (!context.owner)
        return nullptr;

    for (Unit* attacker : context.owner->getAttackers())
    {
        if (!attacker || !attacker->IsAlive())
            continue;
        if (attacker == context.primaryTarget)
            continue;
        if (context.owner->IsFriendlyTo(attacker))
            continue;
        return attacker;
    }

    return nullptr;
}

Spell* GetCurrentNonMeleeSpell(Unit* unit)
{
    if (!unit)
        return nullptr;

    if (Spell* spell = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        return spell;

    if (Spell* spell = unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return spell;

    return nullptr;
}
} // namespace

BotCombatEvaluationResult BotCombatRuntimeEvaluator::EvaluateInterrupts(
    BotCombatPreparedProfile const& preparedProfile,
    BotCombatRuntimeContext const& context) const
{
    return EvaluateEntries(preparedProfile.interruptEntries, context);
}

BotCombatEvaluationResult BotCombatRuntimeEvaluator::EvaluateRotation(
    BotCombatPreparedProfile const& preparedProfile,
    BotCombatRuntimeContext const& context) const
{
    return EvaluateEntries(preparedProfile.rotationEntries, context);
}

BotCombatEvaluationResult BotCombatRuntimeEvaluator::EvaluateEntries(
    std::vector<model::BotCombatEntryDefinition> const& sourceEntries,
    BotCombatRuntimeContext const& context)
{
    std::vector<model::BotCombatEntryDefinition> entries = sourceEntries;
    std::stable_sort(
        entries.begin(),
        entries.end(),
        [](model::BotCombatEntryDefinition const& left,
           model::BotCombatEntryDefinition const& right)
        {
            return left.priority < right.priority;
        });

    for (model::BotCombatEntryDefinition const& entry : entries)
    {
        if (!EvaluateEntryConditions(entry, context))
            continue;

        if (auto evaluated = EvaluateAction(entry, entry.primaryAction, context))
        {
            BotCombatEvaluationResult result;
            result.disposition = BotCombatEvaluationDisposition::Cast;
            result.action = std::move(evaluated);
            return result;
        }

        std::uint32_t const currentCastPrimaryWaitMs =
            GetCurrentCastHoldWaitMs(entry, entry.primaryAction, context);
        if (currentCastPrimaryWaitMs > 0)
        {
            BotCombatEvaluationResult result;
            result.disposition = BotCombatEvaluationDisposition::Wait;
            result.waitMs = currentCastPrimaryWaitMs;
            return result;
        }

        std::uint32_t const primaryWaitMs = entry.isInterrupt
            ? 0
            : GetActionWaitMs(entry.primaryAction, context);
        if (primaryWaitMs > 0)
        {
            BotCombatEvaluationResult result;
            result.disposition = BotCombatEvaluationDisposition::Wait;
            result.waitMs = primaryWaitMs;
            return result;
        }

        if (entry.secondaryAction)
        {
            if (auto evaluated = EvaluateAction(entry, *entry.secondaryAction, context))
            {
                BotCombatEvaluationResult result;
                result.disposition = BotCombatEvaluationDisposition::Cast;
                result.action = std::move(evaluated);
                return result;
            }

            std::uint32_t const currentCastSecondaryWaitMs =
                GetCurrentCastHoldWaitMs(entry, *entry.secondaryAction, context);
            if (currentCastSecondaryWaitMs > 0)
            {
                BotCombatEvaluationResult result;
                result.disposition = BotCombatEvaluationDisposition::Wait;
                result.waitMs = currentCastSecondaryWaitMs;
                return result;
            }

            std::uint32_t const secondaryWaitMs = entry.isInterrupt
                ? 0
                : GetActionWaitMs(*entry.secondaryAction, context);
            if (secondaryWaitMs > 0)
            {
                BotCombatEvaluationResult result;
                result.disposition = BotCombatEvaluationDisposition::Wait;
                result.waitMs = secondaryWaitMs;
                return result;
            }
        }
    }

    return {};
}

bool BotCombatRuntimeEvaluator::EvaluateEntryConditions(
    model::BotCombatEntryDefinition const& entry,
    BotCombatRuntimeContext const& context)
{
    if (entry.conditions.empty())
        return true;

    if (entry.conditionLogic == model::BotCombatConditionLogic::All)
    {
        for (model::BotCombatConditionDefinition const& condition : entry.conditions)
        {
            if (!EvaluateCondition(condition, context))
                return false;
        }
        return true;
    }

    for (model::BotCombatConditionDefinition const& condition : entry.conditions)
    {
        if (EvaluateCondition(condition, context))
            return true;
    }

    return false;
}

bool BotCombatRuntimeEvaluator::EvaluateCondition(
    model::BotCombatConditionDefinition const& condition,
    BotCombatRuntimeContext const& context)
{
    if (condition.statKey == "exists")
    {
        bool const exists = ResolveConditionSubject(condition.subjectKey, context) != nullptr;
        if (condition.comparison == model::BotCombatConditionOperator::Exists)
            return exists;
        return CompareNumeric(
            condition.comparison,
            exists ? 1.0f : 0.0f,
            condition.numericValue);
    }

    Unit* subject = ResolveConditionSubject(condition.subjectKey, context);
    if (!subject)
        return false;

    if (condition.statKey == "hp_pct")
        return CompareNumeric(condition.comparison, subject->GetHealthPct(), condition.numericValue);

    if (condition.statKey == "mana_pct")
        return CompareNumeric(condition.comparison, GetManaPct(subject), condition.numericValue);

    if (condition.statKey == "distance")
    {
        if (!context.bot)
            return false;
        return CompareNumeric(
            condition.comparison,
            context.bot->GetDistance(subject),
            condition.numericValue);
    }

    if (condition.statKey == "aura" || condition.statKey == "has_aura")
    {
        std::optional<std::uint32_t> spellId = ParseConditionSpellId(condition);
        if (!spellId)
            return false;

        bool const hasAura = subject->HasAura(*spellId);
        switch (condition.comparison)
        {
            case model::BotCombatConditionOperator::Has:
                return hasAura;
            case model::BotCombatConditionOperator::NotHas:
                return !hasAura;
            case model::BotCombatConditionOperator::Equal:
            case model::BotCombatConditionOperator::NotEqual:
            case model::BotCombatConditionOperator::LessThan:
            case model::BotCombatConditionOperator::LessThanOrEqual:
            case model::BotCombatConditionOperator::GreaterThan:
            case model::BotCombatConditionOperator::GreaterThanOrEqual:
            case model::BotCombatConditionOperator::Exists:
                return CompareNumeric(
                    condition.comparison,
                    hasAura ? 1.0f : 0.0f,
                    condition.numericValue);
        }
    }

    if (condition.statKey == "combo_points")
        return CompareNumeric(
            condition.comparison,
            static_cast<float>(subject->GetComboPoints()),
            condition.numericValue);

    if (condition.statKey == "threat_pct")
    {
        if (!context.primaryTarget)
            return false;
        ThreatManager& mgr = context.primaryTarget->GetThreatMgr();
        float const subjectThreat = mgr.GetThreat(subject);
        float topThreat = 0.0f;
        for (ThreatReference const* ref : mgr.GetSortedThreatList())
        {
            if (ref->IsOnline())
            {
                topThreat = ref->GetThreat();
                break;
            }
        }
        float const pct = (topThreat > 0.0f) ? (subjectThreat / topThreat * 100.0f) : 0.0f;
        return CompareNumeric(condition.comparison, pct, condition.numericValue);
    }

    if (condition.statKey == "is_aggro_holder")
    {
        if (!context.primaryTarget)
            return false;
        bool const holds = (context.primaryTarget->GetThreatMgr().GetCurrentVictim() == subject);
        return CompareNumeric(condition.comparison, holds ? 1.0f : 0.0f, condition.numericValue);
    }

    if (condition.statKey == "aura_stacks")
    {
        if (condition.stringValue.empty())
            return false;
        uint32_t spellId = 0;
        try { spellId = static_cast<uint32_t>(std::stoul(condition.stringValue)); }
        catch (...) { return false; }
        uint32_t stacks = 0;
        if (Aura const* aura = subject->GetAura(spellId))
            stacks = aura->GetStackAmount();
        return CompareNumeric(
            condition.comparison,
            static_cast<float>(stacks),
            condition.numericValue);
    }

    return false;
}

std::optional<BotCombatEvaluatedAction> BotCombatRuntimeEvaluator::EvaluateAction(
    model::BotCombatEntryDefinition const& entry,
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context)
{
    if (!context.bot)
        return std::nullopt;

    if (action.actionType != model::BotCombatActionType::Spell)
        return std::nullopt;

    std::uint32_t const spellId =
        BotCombatProfilePreparationService::ResolveKnownSpellForAction(
            context.bot,
            action);
    if (spellId == 0)
        return std::nullopt;

    Unit* target = ResolveActionTarget(action.targetKey, context);
    if (!target)
        return std::nullopt;

    if (entry.isInterrupt && !HasInterruptibleEnemyCast(target))
        return std::nullopt;

    if (!CanExecuteSpell(context.bot, target, spellId))
        return std::nullopt;

    BotCombatEvaluatedAction evaluated;
    evaluated.entryId = entry.entryId;
    evaluated.actionId = action.actionId;
    evaluated.spellId = spellId;
    evaluated.target = target;
    evaluated.targetKey = action.targetKey;
    evaluated.breaksCurrentCast = entry.breaksCurrentCast;

    if (!CanBreakCurrentCast(context.bot, evaluated))
        return std::nullopt;

    return evaluated;
}

std::uint32_t BotCombatRuntimeEvaluator::GetActionWaitMs(
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context)
{
    if (!context.bot)
        return 0;

    if (action.actionType != model::BotCombatActionType::Spell)
        return 0;

    std::uint32_t const spellId =
        BotCombatProfilePreparationService::ResolveKnownSpellForAction(
            context.bot,
            action);
    if (spellId == 0)
        return 0;

    Unit* target = ResolveActionTarget(action.targetKey, context);
    if (!target)
        return 0;

    std::uint32_t const waitMs = GetSpellWaitMs(context.bot, target, spellId);
    if (waitMs == 0 || waitMs > context.rotationWaitMs)
        return 0;

    return waitMs;
}

std::uint32_t BotCombatRuntimeEvaluator::GetCurrentCastHoldWaitMs(
    model::BotCombatEntryDefinition const& entry,
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context)
{
    if (!context.bot || !context.bot->IsNonMeleeSpellCast(false) || entry.breaksCurrentCast)
        return 0;

    if (action.actionType != model::BotCombatActionType::Spell)
        return 0;

    std::uint32_t const spellId =
        BotCombatProfilePreparationService::ResolveKnownSpellForAction(
            context.bot,
            action);
    if (spellId == 0)
        return 0;

    Unit* target = ResolveActionTarget(action.targetKey, context);
    if (!target)
        return 0;

    if (entry.isInterrupt && !HasInterruptibleEnemyCast(target))
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    if (context.bot->HasSpellCooldown(spellId))
        return 0;

    if (context.bot->GetGlobalCooldownMgr().GetGlobalCooldown(spellInfo) > 0)
        return 0;

    float const maxRange = context.bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !context.bot->IsWithinCombatRange(target, maxRange))
        return 0;

    Spell* currentSpell = GetCurrentNonMeleeSpell(context.bot);
    if (!currentSpell)
        return context.rotationWaitMs;

    std::uint32_t const remainingCastMs = std::max(currentSpell->GetCastTimeRemaining(), 0);
    return std::min(remainingCastMs, context.rotationWaitMs);
}

Unit* BotCombatRuntimeEvaluator::ResolveConditionSubject(
    std::string const& subjectKey,
    BotCombatRuntimeContext const& context)
{
    return ResolveActionTarget(subjectKey, context);
}

Unit* BotCombatRuntimeEvaluator::ResolveActionTarget(
    std::string const& targetKey,
    BotCombatRuntimeContext const& context)
{
    if (targetKey.empty() || targetKey == "enemy" || targetKey == "enemy_primary")
        return context.primaryTarget;

    if (targetKey == "self")
        return context.bot;

    if (targetKey == "owner")
        return context.owner;

    if (targetKey == "lowest_hp_party")
        return FindLowestHealthPartyTarget(context.bot, context.owner);

    if (targetKey == "enemy_trash")
        return FindTrashTarget(context);

    if (targetKey == "enemy_primary_victim")
        return context.primaryTarget ? context.primaryTarget->GetVictim() : nullptr;

    return nullptr;
}

bool BotCombatRuntimeEvaluator::CanExecuteSpell(
    Player* bot,
    Unit* target,
    std::uint32_t spellId)
{
    if (!bot || !target || spellId == 0)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (bot->HasSpellCooldown(spellId))
        return false;

    if (bot->GetGlobalCooldownMgr().GetGlobalCooldown(spellInfo) > 0)
        return false;

    float const maxRange = bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !bot->IsWithinCombatRange(target, maxRange))
        return false;

    return true;
}

std::uint32_t BotCombatRuntimeEvaluator::GetSpellWaitMs(
    Player* bot,
    Unit* target,
    std::uint32_t spellId)
{
    if (!bot || !target || spellId == 0)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    float const maxRange = bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !bot->IsWithinCombatRange(target, maxRange))
        return 0;

    std::uint32_t const spellCooldownMs = bot->GetSpellCooldownDelay(spellId);
    std::uint32_t const globalCooldownMs =
        bot->GetGlobalCooldownMgr().GetGlobalCooldown(spellInfo);
    std::uint32_t const waitMs = std::max(spellCooldownMs, globalCooldownMs);
    if (waitMs == 0)
        return 0;

    return waitMs;
}

bool BotCombatRuntimeEvaluator::CanBreakCurrentCast(
    Player* bot,
    BotCombatEvaluatedAction const& evaluatedAction)
{
    if (!bot || !bot->IsNonMeleeSpellCast(false))
        return true;

    if (!evaluatedAction.breaksCurrentCast)
        return false;

    return true;
}

bool BotCombatRuntimeEvaluator::HasInterruptibleEnemyCast(Unit* target)
{
    Spell* currentSpell = GetCurrentNonMeleeSpell(target);
    if (!currentSpell)
        return false;

    return currentSpell->IsInterruptable();
}
} // namespace service
} // namespace living_world
