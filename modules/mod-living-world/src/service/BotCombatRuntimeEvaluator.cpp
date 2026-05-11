#include "service/BotCombatRuntimeEvaluator.h"

#include "CellImpl.h"
#include "Creature.h"
#include "DataStores/DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <algorithm>
#include <cctype>
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
// Returns the remaining cooldown in milliseconds for the given spell on any Unit.
// Dispatches to Player::GetSpellCooldownDelay or Creature::GetSpellCooldown.
std::uint32_t GetSpellCooldownRemainingMs(Unit* bot, std::uint32_t spellId)
{
    if (!bot || spellId == 0)
        return 0;
    if (Player* player = bot->ToPlayer())
        return player->GetSpellCooldownDelay(spellId);
    if (Creature* creature = bot->ToCreature())
        return creature->GetSpellCooldown(spellId);
    return 0;
}

// Returns the global cooldown remaining in ms for a given spell on any Unit.
// Creatures do not use the player GCD system; returns 0 for non-Player units.
std::uint32_t GetGlobalCooldownRemainingMs(Unit* bot, SpellInfo const* spellInfo)
{
    if (!bot || !spellInfo)
        return 0;
    if (Player* player = bot->ToPlayer())
        return player->GetGlobalCooldownMgr().GetGlobalCooldown(spellInfo);
    return 0;
}

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
    if (!condition.stringValue.empty())
    {
        std::uint32_t spellId = 0;
        auto const* begin = condition.stringValue.data();
        auto const* end = begin + condition.stringValue.size();
        auto const result = std::from_chars(begin, end, spellId);
        if (result.ec == std::errc{} && result.ptr == end)
            return spellId;
    }

    if (condition.numericValue > 0.0f)
        return static_cast<std::uint32_t>(condition.numericValue);

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

float NormalizeDisplayedPower(Powers powerType, std::int32_t rawValue)
{
    if (powerType == POWER_RAGE || powerType == POWER_RUNIC_POWER)
        return static_cast<float>(rawValue) / 10.0f;

    return static_cast<float>(rawValue);
}

float GetPower(Unit* unit, Powers powerType)
{
    if (!unit)
        return 0.0f;

    return NormalizeDisplayedPower(powerType, unit->GetPower(powerType));
}

float GetPowerPct(Unit* unit, Powers powerType)
{
    if (!unit)
        return 0.0f;

    std::uint32_t const maxPower = unit->GetMaxPower(powerType);
    if (maxPower == 0)
        return 100.0f;

    return 100.0f * static_cast<float>(unit->GetPower(powerType)) /
        static_cast<float>(maxPower);
}

std::string ToLowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool TryParseRuneType(
    std::string const& value,
    std::optional<RuneType>& runeType)
{
    std::string const lowered = ToLowerCopy(value);
    if (lowered.empty() || lowered == "any")
    {
        runeType = std::nullopt;
        return true;
    }

    if (lowered == "blood" || lowered == "b")
    {
        runeType = RUNE_BLOOD;
        return true;
    }

    if (lowered == "frost" || lowered == "f")
    {
        runeType = RUNE_FROST;
        return true;
    }

    if (lowered == "unholy" || lowered == "u")
    {
        runeType = RUNE_UNHOLY;
        return true;
    }

    if (lowered == "death" || lowered == "d")
    {
        runeType = RUNE_DEATH;
        return true;
    }

    return false;
}

bool RuneMatchesRequested(RuneType currentRune, RuneType requestedRune)
{
    if (requestedRune == RUNE_DEATH)
        return currentRune == RUNE_DEATH;

    return currentRune == requestedRune || currentRune == RUNE_DEATH;
}

std::uint32_t CountReadyRunes(Player* player, std::optional<RuneType> requestedRune)
{
    if (!player)
        return 0;

    std::uint32_t ready = 0;
    for (std::uint8_t i = 0; i < MAX_RUNES; ++i)
    {
        if (player->GetRuneCooldown(i) != 0)
            continue;

        RuneType const currentRune = player->GetCurrentRune(i);
        if (!requestedRune || RuneMatchesRequested(currentRune, *requestedRune))
            ++ready;
    }

    return ready;
}

bool HasRequiredRunes(Player* player, std::uint32_t runeCostId)
{
    if (!player || runeCostId == 0)
        return true;

    SpellRuneCostEntry const* runeCostData = sSpellRuneCostStore.LookupEntry(runeCostId);
    if (!runeCostData || runeCostData->NoRuneCost())
        return true;

    int32 runeCost[NUM_RUNE_TYPES] = { 0, 0, 0, 0 };
    for (std::uint32_t i = 0; i < RUNE_DEATH; ++i)
        runeCost[i] = runeCostData->RuneCost[i];

    for (std::uint8_t i = 0; i < MAX_RUNES; ++i)
    {
        RuneType const rune = player->GetCurrentRune(i);
        if (player->GetRuneCooldown(i) == 0 && runeCost[rune] > 0)
            --runeCost[rune];
    }

    int32 deathRuneDeficit = 0;
    for (std::uint32_t i = 0; i < RUNE_DEATH; ++i)
    {
        if (runeCost[i] > 0)
            deathRuneDeficit += runeCost[i];
    }

    if (deathRuneDeficit == 0)
        return true;

    for (std::uint8_t i = 0; i < MAX_RUNES; ++i)
    {
        if (player->GetRuneCooldown(i) == 0 && player->GetCurrentRune(i) == RUNE_DEATH)
            --deathRuneDeficit;
    }

    return deathRuneDeficit <= 0;
}

float GetAuraRemainingSecs(Unit* unit, std::uint32_t spellId)
{
    if (!unit || spellId == 0)
        return 0.0f;

    Aura const* aura = unit->GetAura(spellId);
    if (!aura)
        return 0.0f;

    return static_cast<float>(aura->GetDuration()) / 1000.0f;
}

std::uint32_t CountNearbyEnemies(
    BotCombatRuntimeContext const& context,
    Unit* subject,
    float radius)
{
    if (!subject || radius <= 0.0f)
        return 0;

    Unit* hostilityReference = context.bot ? context.bot : subject;
    std::vector<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(subject, hostilityReference, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(subject, targets, check);
    Cell::VisitObjects(subject, searcher, radius);
    return static_cast<std::uint32_t>(targets.size());
}

std::uint32_t CountPartyMembersBelowHealthPctImpl(
    Unit* bot,
    Player* owner,
    float thresholdPct)
{
    std::uint32_t count = 0;

    auto consider = [&](Player* candidate)
    {
        if (!candidate || !candidate->IsAlive())
            return;

        if (candidate->GetHealthPct() <= thresholdPct)
            ++count;
    };

    consider(owner);
    consider(bot ? bot->ToPlayer() : nullptr);

    if (owner)
    {
        if (Group const* group = owner->GetGroup())
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
                if (!member || member == owner || member == bot)
                    continue;

                consider(member);
            }
        }
    }

    return count;
}

Player* FindLowestHealthPartyTarget(Unit* bot, Player* owner)
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
    consider(bot->ToPlayer());

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

std::uint32_t BotCombatRuntimeEvaluator::CountPartyMembersBelowHealthPct(
    Unit* bot,
    Player* owner,
    float thresholdPct)
{
    return CountPartyMembersBelowHealthPctImpl(bot, owner, thresholdPct);
}

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

    if (condition.statKey == "power_pct")
        return CompareNumeric(
            condition.comparison,
            GetPowerPct(subject, subject->getPowerType()),
            condition.numericValue);

    if (condition.statKey == "power")
        return CompareNumeric(
            condition.comparison,
            GetPower(subject, subject->getPowerType()),
            condition.numericValue);

    if (condition.statKey == "runic_power_pct")
        return CompareNumeric(
            condition.comparison,
            GetPowerPct(subject, POWER_RUNIC_POWER),
            condition.numericValue);

    if (condition.statKey == "runic_power")
        return CompareNumeric(
            condition.comparison,
            GetPower(subject, POWER_RUNIC_POWER),
            condition.numericValue);

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

    if (condition.statKey == "aura_remaining_secs")
    {
        std::optional<std::uint32_t> spellId = ParseConditionSpellId(condition);
        if (!spellId)
            return false;

        return CompareNumeric(
            condition.comparison,
            GetAuraRemainingSecs(subject, *spellId),
            condition.numericValue);
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

    if (condition.statKey == "runes_ready" || condition.statKey == "runes_available")
    {
        Player* player = subject->ToPlayer();
        if (!player)
            return false;

        std::optional<RuneType> requestedRune;
        if (!TryParseRuneType(condition.stringValue, requestedRune))
            return false;

        return CompareNumeric(
            condition.comparison,
            static_cast<float>(CountReadyRunes(player, requestedRune)),
            condition.numericValue);
    }

    if (condition.statKey == "nearby_enemies")
    {
        float radius = 10.0f;
        if (!condition.stringValue.empty())
        {
            try
            {
                radius = std::stof(condition.stringValue);
            }
            catch (...)
            {
                return false;
            }
        }

        return CompareNumeric(
            condition.comparison,
            static_cast<float>(CountNearbyEnemies(context, subject, radius)),
            condition.numericValue);
    }

    if (condition.statKey == "party_members_below_hp_pct")
    {
        float thresholdPct = 0.0f;
        if (!condition.stringValue.empty())
        {
            try
            {
                thresholdPct = std::stof(condition.stringValue);
            }
            catch (...)
            {
                return false;
            }
        }
        else
        {
            thresholdPct = condition.numericValue;
        }

        std::uint32_t const memberCount =
            CountPartyMembersBelowHealthPctImpl(context.bot, context.owner, thresholdPct);

        return CompareNumeric(
            condition.comparison,
            static_cast<float>(memberCount),
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
            context.availableSpells,
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
            context.availableSpells,
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
            context.availableSpells,
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

    if (GetGlobalCooldownRemainingMs(context.bot, spellInfo) > 0)
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
    Unit* bot,
    Unit* target,
    std::uint32_t spellId)
{
    if (!bot || !target || spellId == 0)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (bot->isMoving() &&
        spellInfo->CalcCastTime(bot) > 0 &&
        (spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT))
    {
        return false;
    }

    if (Creature* creature = bot->ToCreature())
    {
        if (!creature->CanCastSpell(spellId))
            return false;
    }

    if (Player* player = bot->ToPlayer())
    {
        int32 const powerCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
        if (powerCost > 0)
        {
            if (spellInfo->PowerType == POWER_RUNE)
            {
                if (!HasRequiredRunes(player, spellInfo->RuneCostID))
                    return false;
            }
            else if (spellInfo->PowerType >= POWER_MANA && spellInfo->PowerType <= POWER_RUNIC_POWER)
            {
                Powers const powerType = static_cast<Powers>(spellInfo->PowerType);
                if (player->GetPower(powerType) < powerCost)
                    return false;
            }
        }
    }

    if (bot->HasSpellCooldown(spellId))
        return false;

    if (GetGlobalCooldownRemainingMs(bot, spellInfo) > 0)
        return false;

    float const maxRange = bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !bot->IsWithinCombatRange(target, maxRange))
        return false;

    return true;
}

std::uint32_t BotCombatRuntimeEvaluator::GetSpellWaitMs(
    Unit* bot,
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

    std::uint32_t const spellCooldownMs = GetSpellCooldownRemainingMs(bot, spellId);
    std::uint32_t const globalCooldownMs = GetGlobalCooldownRemainingMs(bot, spellInfo);
    std::uint32_t const waitMs = std::max(spellCooldownMs, globalCooldownMs);
    if (waitMs == 0)
        return 0;

    return waitMs;
}

bool BotCombatRuntimeEvaluator::CanBreakCurrentCast(
    Unit* bot,
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
