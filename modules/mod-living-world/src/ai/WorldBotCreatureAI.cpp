#include "ai/WorldBotCreatureAI.h"

#include "Creature.h"
#include "CreatureAIImpl.h"
#include "CellImpl.h"
#include "DataStores/DBCStores.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlBotCombatDefaultProfileRepository.h"
#include "integration/SqlBotCombatProfileRepository.h"
#include "integration/SqlBotCombatProfileSelectionRepository.h"
#include "service/BotCombatDoctrineResolver.h"
#include "service/BotCombatRuntimeEvaluator.h"
#include "service/BotContextService.h"
#include "service/SimpleBotCombatSpecRoleResolver.h"
#include "model/BotSpecKey.h"

#include <algorithm>

namespace living_world
{
namespace ai
{

namespace
{
constexpr float GatherSearchRadius = 200.0f;
constexpr float GatherInteractRange = 6.0f;
constexpr float GatherAnchorReturnDistance = 60.0f;

integration::SqlBotIdentityRepository& GetIdentityRepo()
{
    static integration::SqlBotIdentityRepository repo;
    return repo;
}

service::BotContextService& GetCombatContextService()
{
    static service::BotContextService service;
    return service;
}

service::BotCombatDoctrineResolver& GetDoctrineResolver()
{
    static integration::SqlAccountAltRuntimeRepository runtimeRepository;
    static integration::SqlBotCombatProfileRepository profileRepository;
    static integration::SqlBotCombatProfileSelectionRepository selectionRepository;
    static integration::SqlBotCombatDefaultProfileRepository defaultProfileRepository;
    static service::SimpleBotCombatSpecRoleResolver specRoleResolver;
    static service::BotCombatDoctrineResolver doctrineResolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        GetCombatContextService());
    return doctrineResolver;
}

service::BotCombatProfilePreparationService& GetProfilePreparationService()
{
    static service::BotCombatProfilePreparationService preparationService(
        GetDoctrineResolver());
    return preparationService;
}

service::BotCombatRuntimeEvaluator& GetRuntimeEvaluator()
{
    static service::BotCombatRuntimeEvaluator evaluator;
    return evaluator;
}

std::string DescribeResumeState(integration::BotIdentityRecord const& identity)
{
    if (identity.lastSeenZoneId != 0)
    {
        return "resume_from_zone=" + std::to_string(identity.lastSeenZoneId)
            + " session_count=" + std::to_string(identity.sessionCount);
    }

    return "fresh_spawn session_count=" + std::to_string(identity.sessionCount);
}

std::string DescribeNextTask(service::AmbientSession const& session, std::size_t currentStep)
{
    for (std::size_t i = currentStep; i < session.steps.size(); ++i)
    {
        service::AmbientStep const& step = session.steps[i];
        if (step.type != service::AmbientStepType::Travel)
            return step.label;
    }

    return "session_complete";
}

std::string BuildPositionDetail(
    Unit const* bot,
    std::string const& detail)
{
    if (!bot)
        return detail;

    return detail
        + " | zone=" + std::to_string(bot->GetZoneId())
        + " pos=("
        + std::to_string(bot->GetPositionX()) + ","
        + std::to_string(bot->GetPositionY()) + ","
        + std::to_string(bot->GetPositionZ()) + ")";
}

std::string DescribeSessionOrigin(service::AmbientSession const& session)
{
    std::string const sourceKind = session.sourceKind.empty()
        ? "unknown"
        : session.sourceKind;
    std::string const sourceKey = session.sourceKey.empty()
        ? session.activityKey
        : session.sourceKey;

    return "source_kind='" + sourceKind
        + "' source_key='" + sourceKey
        + "' session='" + session.displayName + "'";
}

std::string DescribeTravelRecovery(
    service::AmbientStep const& step,
    char const* reason)
{
    return std::string(reason)
        + " -> " + step.label
        + " pos=("
        + std::to_string(step.x) + ","
        + std::to_string(step.y) + ","
        + std::to_string(step.z) + ")";
}

std::string ResolveRoleKey(std::uint8_t classId, std::string const& specKey)
{
    if (classId == CLASS_PRIEST)
    {
        if (specKey == "Holy" || specKey == "Discipline")
            return "HEAL";
        return "DPS";
    }

    if (classId == CLASS_PALADIN)
    {
        if (specKey == "Holy")
            return "HEAL";
        if (specKey == "Protection")
            return "TANK";
        return "DPS";
    }

    if (classId == CLASS_DRUID)
    {
        if (specKey == "Restoration")
            return "HEAL";
        if (specKey == "Feral")
            return "TANK";
        return "DPS";
    }

    if (classId == CLASS_SHAMAN)
    {
        if (specKey == "Restoration")
            return "HEAL";
        return "DPS";
    }

    if (classId == CLASS_WARRIOR)
        return specKey == "Protection" ? "TANK" : "DPS";

    if (classId == CLASS_DEATH_KNIGHT)
        return specKey == "Blood" ? "TANK" : "DPS";

    return "DPS";
}

bool IsSpellUsableForLevel(std::uint32_t spellId, std::uint8_t level)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    std::uint32_t const requiredLevel = std::max(spellInfo->SpellLevel, spellInfo->BaseLevel);
    return requiredLevel == 0 || requiredLevel <= level;
}

void AddKnownSpellForAction(
    std::unordered_set<std::uint32_t>& knownSpells,
    model::BotCombatActionDefinition const& action,
    std::uint8_t level)
{
    if (action.actionType != model::BotCombatActionType::Spell || action.spellBaseId == 0)
        return;

    switch (action.rankMode)
    {
        case model::BotCombatRankMode::ExactSpellId:
            if (IsSpellUsableForLevel(action.spellBaseId, level))
                knownSpells.insert(action.spellBaseId);
            return;

        case model::BotCombatRankMode::SpecificRank:
        {
            if (action.rankValue == 0)
                return;

            std::uint32_t candidate = sSpellMgr->GetFirstSpellInChain(action.spellBaseId);
            if (!candidate)
                candidate = action.spellBaseId;

            for (std::uint8_t rank = 1; candidate; ++rank)
            {
                if (rank == action.rankValue)
                {
                    if (IsSpellUsableForLevel(candidate, level))
                        knownSpells.insert(candidate);
                    return;
                }

                candidate = sSpellMgr->GetNextSpellInChain(candidate);
            }

            return;
        }

        case model::BotCombatRankMode::BestKnown:
        {
            std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(action.spellBaseId);
            if (!candidate)
                candidate = action.spellBaseId;

            while (candidate)
            {
                if (IsSpellUsableForLevel(candidate, level))
                {
                    knownSpells.insert(candidate);
                    return;
                }

                candidate = sSpellMgr->GetPrevSpellInChain(candidate);
            }

            return;
        }
    }
}

std::unordered_set<std::uint32_t> BuildWorldBotKnownSpells(
    Creature* creature,
    service::BotCombatDoctrineResolution const& resolution,
    std::uint8_t level)
{
    std::unordered_set<std::uint32_t> knownSpells;
    if (!creature)
        return knownSpells;

    for (std::uint8_t i = 0; i < MAX_CREATURE_SPELLS; ++i)
    {
        if (creature->m_spells[i] != 0 && IsSpellUsableForLevel(creature->m_spells[i], level))
            knownSpells.insert(creature->m_spells[i]);
    }

    auto addEntries =
        [&](std::vector<model::BotCombatEntryDefinition> const& entries)
        {
            for (model::BotCombatEntryDefinition const& entry : entries)
            {
                AddKnownSpellForAction(knownSpells, entry.primaryAction, level);
                if (entry.secondaryAction)
                    AddKnownSpellForAction(knownSpells, *entry.secondaryAction, level);
            }
        };

    addEntries(resolution.profile.interruptEntries);
    addEntries(resolution.profile.rotationEntries);
    return knownSpells;
}

class NearestGatherNodeCheck
{
public:
    NearestGatherNodeCheck(WorldObject const& source, SkillType requiredSkill, float range)
        : _source(source), _requiredSkill(requiredSkill), _range(range)
    {
    }

    bool operator()(GameObject* go)
    {
        if (!go || !go->GetGOInfo() || !go->isSpawned())
            return false;

        if (!_source.IsWithinDistInMap(go, _range))
            return false;

        std::uint32_t const lockId = go->GetGOInfo()->GetLockId();
        if (lockId == 0)
            return false;

        LockEntry const* lock = sLockStore.LookupEntry(lockId);
        if (!lock)
            return false;

        bool matchesSkill = false;
        for (std::size_t i = 0; i < MAX_LOCK_CASE; ++i)
        {
            if (lock->Type[i] != LOCK_KEY_SKILL)
                continue;

            if (SkillByLockType(static_cast<LockType>(lock->Index[i])) == _requiredSkill)
            {
                matchesSkill = true;
                break;
            }
        }

        if (!matchesSkill)
            return false;

        _range = _source.GetDistance(go);
        return true;
    }

private:
    WorldObject const& _source;
    SkillType _requiredSkill;
    float _range;
};
} // namespace

// ---------------------------------------------------------------------------
WorldBotCreatureAI::WorldBotCreatureAI(Creature* creature)
    : CreatureAI(creature)
{
}

void WorldBotCreatureAI::InitializeAI()
{
    // Identity and session are set by the spawn path via SetIdentityAndSession.
    // Nothing to do here — we wait until that call arrives.
    me->SetReactState(REACT_AGGRESSIVE);
    me->SetUInt32Value(UNIT_NPC_FLAGS, 0);
    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);
}

void WorldBotCreatureAI::SetIdentityAndSession(
    integration::BotIdentityRecord const& identity,
    service::AmbientSession          const& session,
    std::size_t currentStep,
    std::uint32_t stepElapsedMs,
    std::uint64_t worldOnlineMsSoFar,
    bool alreadyMarkedActive,
    bool resumedFromAbstract)
{
    _identity     = identity;
    _identity.specKey = model::CanonicalizeBotSpecKey(_identity.specKey);
    _session      = session;
    _currentStep  = currentStep;
    _activityTimer = stepElapsedMs;
    _traveling    = false;
    _sessionDone  = false;
    _sessionReady = true;
    _worldOnlineMs = worldOnlineMsSoFar;
    _combatSuspendedStep = false;
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);
    InvalidateCombatProfile();

    ApplyIdentityToCreature();

    if (!alreadyMarkedActive)
        GetIdentityRepo().MarkActive(_identity.id);

    if (!alreadyMarkedActive)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "session_start",
            DescribeSessionOrigin(_session)
                + " tasks=" + std::to_string(_session.tasks.size())
                + " steps=" + std::to_string(_session.steps.size()));
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        DescribeResumeState(_identity));

    if (resumedFromAbstract)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "status_change",
            "materialized_from_abstract step=" + std::to_string(_currentStep)
                + " step_elapsed_ms=" + std::to_string(_activityTimer)
                + " world_online_ms=" + std::to_string(_worldOnlineMs));
    }

    if (_identity.lastSeenZoneId != 0)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "status_change",
            "is alive - resuming tasks");
    }

    if (_currentStep >= _session.steps.size())
        CompletSession();
}

void WorldBotCreatureAI::ApplyIdentityToCreature()
{
    if (!me)
        return;

    me->SetName(_identity.name);
    me->SetLevel(_identity.level);
    me->SetDisplayId(_identity.displayId);
    me->SetFaction(_identity.faction == 2 ? FACTION_HORDE_GENERIC : FACTION_ALLIANCE_GENERIC);
    me->SetReactState(REACT_AGGRESSIVE);
    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);

    // Class-appropriate unit flags
    me->SetByteValue(UNIT_FIELD_BYTES_0, 3, _identity.classId);
    me->UpdateAllStats();
    me->SetFullHealth();
}

void WorldBotCreatureAI::UpdateAI(uint32 diff)
{
    if (!_sessionReady || _sessionDone)
        return;

    _worldOnlineMs += diff;

    _tickAccum += diff;
    if (_tickAccum < TickIntervalMs)
        return;
    _tickAccum -= TickIntervalMs;

    if (me->IsInCombat() || me->GetVictim())
    {
        TickCombat(TickIntervalMs);
        return;
    }

    if (_combatSuspendedStep)
        ResumeSuspendedStepAfterCombat();

    TickStep(TickIntervalMs);

    if ((_worldOnlineMs % PositionSnapshotIntervalMs) < TickIntervalMs)
    {
        RecordPositionSnapshot("position_tick", DescribeNextTask(_session, _currentStep));
    }
}

void WorldBotCreatureAI::RecordPositionSnapshot(char const* eventType, std::string const& detail) const
{
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        eventType,
        BuildPositionDetail(me, detail));
}

bool WorldBotCreatureAI::BuildRuntimeSnapshot(RuntimeSnapshot& out) const
{
    if (!_sessionReady || _sessionDone || !me)
        return false;

    out.identity = _identity;
    out.session = _session;
    out.worldOnlineMs = _worldOnlineMs;
    out.progress.currentStep = _currentStep;
    out.progress.stepStartMapId = static_cast<std::uint16_t>(me->GetMapId());
    out.progress.stepStartX = me->GetPositionX();
    out.progress.stepStartY = me->GetPositionY();
    out.progress.stepStartZ = me->GetPositionZ();
    out.progress.stepElapsedMs = 0;

    if (_currentStep >= _session.steps.size())
        return true;

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type != service::AmbientStepType::Travel)
        out.progress.stepElapsedMs = _activityTimer;

    return true;
}

void WorldBotCreatureAI::JustEngagedWith(Unit* who)
{
    SuspendCurrentStepForCombat(who);
}

void WorldBotCreatureAI::JustReachedHome()
{
    if (_combatSuspendedStep && !me->IsInCombat() && !me->GetVictim())
        ResumeSuspendedStepAfterCombat();
}

std::string WorldBotCreatureAI::DescribeCurrentStep() const
{
    if (_currentStep >= _session.steps.size())
        return "session_complete";

    return _session.steps[_currentStep].label;
}

void WorldBotCreatureAI::InvalidateCombatProfile()
{
    _combatPreparedProfile = {};
    _combatProfilePrepared = false;
}

void WorldBotCreatureAI::ResetGatherState()
{
    _gatherTargetGuid.Clear();
    _gatherMovingToNode = false;
    _gatherCompletedCycles = 0;
}

void WorldBotCreatureAI::EnsureCombatProfile()
{
    if (_combatProfilePrepared || !_sessionReady || !me)
        return;

    std::string const roleKey = ResolveRoleKey(_identity.classId, _identity.specKey);
    service::BotCombatDoctrineResolution const resolution =
        GetDoctrineResolver().ResolveForWorldBot(
            me->GetGUID().GetCounter(),
            _identity.classId,
            _identity.specKey,
            roleKey,
            "PvE");

    std::unordered_set<std::uint32_t> const knownSpells =
        BuildWorldBotKnownSpells(me, resolution, _identity.level);

    _combatPreparedProfile = GetProfilePreparationService().PrepareForWorldBot(
        me,
        knownSpells,
        resolution.effectiveSpecKey.empty() ? _identity.specKey : resolution.effectiveSpecKey,
        resolution.effectiveRoleKey.empty() ? roleKey : resolution.effectiveRoleKey,
        "PvE");
    _combatProfilePrepared = true;
}

void WorldBotCreatureAI::SuspendCurrentStepForCombat(Unit* target)
{
    if (_combatSuspendedStep || !_sessionReady || _sessionDone)
        return;

    _combatSuspendedStep = true;
    if (_traveling || _gatherMovingToNode)
    {
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        _traveling = false;
        _gatherMovingToNode = false;
        ResetTravelWatchdog(_travelWatchdog);
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_enter",
        "step='" + DescribeCurrentStep()
            + "' target_guid=" + std::to_string(target ? target->GetGUID().GetCounter() : 0));
}

void WorldBotCreatureAI::ResumeSuspendedStepAfterCombat()
{
    if (!_combatSuspendedStep)
        return;

    _combatSuspendedStep = false;
    _traveling = false;
    ResetTravelWatchdog(_travelWatchdog);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_exit",
        "resume_step='" + DescribeCurrentStep() + "'");
}

GameObject* WorldBotCreatureAI::ResolveGatherTarget() const
{
    if (!_gatherTargetGuid || !me)
        return nullptr;

    return ObjectAccessor::GetGameObject(*me, _gatherTargetGuid);
}

bool WorldBotCreatureAI::IsGatherNodeForStep(
    GameObject const* go,
    service::AmbientStep const& step) const
{
    if (!go || !go->GetGOInfo() || !go->isSpawned())
        return false;

    SkillType requiredSkill = SKILL_NONE;
    switch (step.type)
    {
        case service::AmbientStepType::GatherHerb:
            requiredSkill = SKILL_HERBALISM;
            break;
        case service::AmbientStepType::GatherOre:
            requiredSkill = SKILL_MINING;
            break;
        default:
            return false;
    }

    std::uint32_t const lockId = go->GetGOInfo()->GetLockId();
    if (lockId == 0)
        return false;

    LockEntry const* lock = sLockStore.LookupEntry(lockId);
    if (!lock)
        return false;

    for (std::size_t i = 0; i < MAX_LOCK_CASE; ++i)
    {
        if (lock->Type[i] != LOCK_KEY_SKILL)
            continue;

        if (SkillByLockType(static_cast<LockType>(lock->Index[i])) == requiredSkill)
            return true;
    }

    return false;
}

GameObject* WorldBotCreatureAI::FindNearestGatherNode(service::AmbientStep const& step) const
{
    if (!me)
        return nullptr;

    SkillType requiredSkill = SKILL_NONE;
    switch (step.type)
    {
        case service::AmbientStepType::GatherHerb:
            requiredSkill = SKILL_HERBALISM;
            break;
        case service::AmbientStepType::GatherOre:
            requiredSkill = SKILL_MINING;
            break;
        default:
            return nullptr;
    }

    GameObject* result = nullptr;
    float bestDistance = GatherSearchRadius;

    auto workerFn =
        [&](GameObject* go)
        {
            if (!go || !go->GetGOInfo() || !go->isSpawned())
                return;

            if (!IsGatherNodeForStep(go, step))
                return;

            float const dist = me->GetDistance(go);
            if (dist > bestDistance)
                return;

            bestDistance = dist;
            result = go;
        };

    Acore::GameObjectWorker<decltype(workerFn)> worker(me, workerFn);
    Cell::VisitObjects(me, worker, GatherSearchRadius);
    return result;
}

void WorldBotCreatureAI::TickGatherStep(service::AmbientStep const& step)
{
    if (!me)
        return;

    if (_activityTimer == 0)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activity_start",
            step.label + " subject_kind='" + step.subjectKind + "'");
    }

    _activityTimer += TickIntervalMs;
    std::uint32_t const durationMs = std::max<std::uint32_t>(1000u, step.durationSec * 1000u);
    std::uint8_t const requiredCycles = std::max<std::uint8_t>(1u, step.cycleCount);

    if (_gatherCompletedCycles >= requiredCycles || _activityTimer >= durationMs)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activity_complete",
            step.label + " cycles=" + std::to_string(_gatherCompletedCycles)
                + "/" + std::to_string(requiredCycles));
        _activityTimer = 0;
        AdvanceStep();
        return;
    }

    GameObject* target = ResolveGatherTarget();
    if (target && !IsGatherNodeForStep(target, step))
    {
        _gatherTargetGuid.Clear();
        _gatherMovingToNode = false;
        target = nullptr;
    }

    if (!target)
    {
        target = FindNearestGatherNode(step);
        if (target)
        {
            _gatherTargetGuid = target->GetGUID();
            _gatherMovingToNode = false;
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "gather_target_acquired",
                step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter()));
        }
    }

    if (!target)
    {
        if ((_activityTimer % 60000) < TickIntervalMs)
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "activity_tick",
                step.label + " waiting_for_node cycles=" + std::to_string(_gatherCompletedCycles));
        }

        if (!_gatherMovingToNode && me->GetDistance(step.x, step.y, step.z) > ArrivalThreshold)
        {
            me->GetMotionMaster()->MovePoint(static_cast<uint32>(_currentStep), step.x, step.y, step.z);
            _gatherMovingToNode = true;
        }
        return;
    }

    float const dist = me->GetDistance(target);
    if (dist > GatherInteractRange)
    {
        if (!_gatherMovingToNode)
        {
            me->GetMotionMaster()->MovePoint(
                static_cast<uint32>(_currentStep),
                target->GetPositionX(),
                target->GetPositionY(),
                target->GetPositionZ());
            _gatherMovingToNode = true;
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "gather_move_start",
                step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter()));
        }
        return;
    }

    me->StopMoving();
    me->GetMotionMaster()->Clear();
    _gatherMovingToNode = false;
    target->SetLootState(GO_JUST_DEACTIVATED, me);
    ++_gatherCompletedCycles;
    _gatherTargetGuid.Clear();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "gather_interact",
        step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " cycles=" + std::to_string(_gatherCompletedCycles)
            + "/" + std::to_string(requiredCycles));
}

void WorldBotCreatureAI::TickCombat(uint32 /*diff*/)
{
    if (!me)
        return;

    SuspendCurrentStepForCombat(me->GetVictim());

    if (!UpdateVictim())
        return;

    Unit* target = me->GetVictim();
    if (!target)
        return;

    EnsureCombatProfile();

    bool acted = false;
    if (!_combatPreparedProfile.interruptEntries.empty() || !_combatPreparedProfile.rotationEntries.empty())
    {
        service::BotCombatRuntimeContext context;
        context.bot = me;
        context.owner = nullptr;
        context.primaryTarget = target;
        context.rotationWaitMs = _combatPreparedProfile.resolution.profile.settings.rotationWaitMs;
        context.availableSpells = _combatPreparedProfile.availableSpells;

        auto const tryResult =
            [&](service::BotCombatEvaluationResult const& result) -> bool
            {
                if (result.disposition != service::BotCombatEvaluationDisposition::Cast || !result.action)
                    return result.disposition == service::BotCombatEvaluationDisposition::Wait;

                service::BotCombatEvaluatedAction const& action = *result.action;
                if (action.breaksCurrentCast && me->IsNonMeleeSpellCast(false))
                    me->InterruptNonMeleeSpells(false);

                me->CastSpell(action.target, action.spellId, false);
                return true;
            };

        acted = tryResult(GetRuntimeEvaluator().EvaluateInterrupts(_combatPreparedProfile, context));
        if (!acted)
            acted = tryResult(GetRuntimeEvaluator().EvaluateRotation(_combatPreparedProfile, context));
    }

    if (!acted && !me->IsWithinMeleeRange(target))
        me->GetMotionMaster()->MoveChase(target);

    DoMeleeAttackIfReady();
}

void WorldBotCreatureAI::TickStep(uint32 /*diff*/)
{
    if (_currentStep >= _session.steps.size())
    {
        CompletSession();
        return;
    }

    service::AmbientStep const& step = _session.steps[_currentStep];

    if (step.type == service::AmbientStepType::Travel)
    {
        if (!_traveling)
        {
            // Same-map only for now — skip cross-map travel steps.
            if (step.mapId != me->GetMapId())
            {
                LOG_INFO("server.worldserver",
                    "[WorldBotAI] bot='{}' identity={} skipping cross-map travel step={} "
                    "step_map={} bot_map={}",
                    _identity.name, _identity.id,
                    _currentStep, step.mapId, me->GetMapId());
                AdvanceStep();
                return;
            }

            me->GetMotionMaster()->MovePoint(
                static_cast<uint32>(_currentStep),
                step.x, step.y, step.z);
            _traveling = true;

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_start", step.label);

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "status_change",
                "Travel started -> " + step.label);
        }
        else
        {
            // Check arrival
            float const dist = me->GetDistance(step.x, step.y, step.z);
            if (dist <= ArrivalThreshold)
            {
                _traveling = false;
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_arrive", step.label);

                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "status_change",
                    "destination reached -> beginning "
                        + DescribeNextTask(_session, _currentStep + 1));

                AdvanceStep();
                ResetTravelWatchdog(_travelWatchdog);
                return;
            }

            TravelWatchdogSignal const signal = UpdateTravelWatchdog(
                _travelWatchdog,
                me->GetPositionX(),
                me->GetPositionY(),
                me->GetPositionZ(),
                TickIntervalMs);
            if (signal == TravelWatchdogSignal::Stuck || signal == TravelWatchdogSignal::Timeout)
            {
                char const* eventType = signal == TravelWatchdogSignal::Timeout
                    ? "travel_timeout"
                    : "travel_stuck";
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    eventType,
                    DescribeTravelRecovery(step, eventType));
                me->NearTeleportTo(step.x, step.y, step.z, me->GetOrientation());
                _traveling = false;
                ResetTravelWatchdog(_travelWatchdog);
                AdvanceStep();
            }
        }
        return;
    }

    if (step.type == service::AmbientStepType::TaxiFlight)
    {
        if (_activityTimer == 0)
        {
            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_taxi_start", step.label);

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "status_change",
                "In transit -> " + step.label);
        }

        _activityTimer += TickIntervalMs;
        uint32 const durationMs = std::max<uint32>(1000u, step.durationSec * 1000u);
        if (_activityTimer >= durationMs)
        {
            if (step.mapId == me->GetMapId())
            {
                me->NearTeleportTo(step.x, step.y, step.z, me->GetOrientation());
            }

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_taxi_arrive", step.label);

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "status_change",
                "Taxi complete -> " + DescribeNextTask(_session, _currentStep + 1));

            _activityTimer = 0;
            AdvanceStep();
        }
        return;
    }

    if (step.type == service::AmbientStepType::GatherHerb
        || step.type == service::AmbientStepType::GatherOre)
    {
        TickGatherStep(step);
        return;
    }

    // Activity step — count down duration.
    if (_activityTimer == 0)
    {
        integration::BotActivityLog::Record(
            me, _identity.name, _identity.id,
            "status_change",
            "beginning task -> " + step.label);
    }

    _activityTimer += TickIntervalMs;
    uint32 const durationMs = step.durationSec * 1000u;

    // Heartbeat log every 60 seconds.
    if (_activityTimer % 60000 < TickIntervalMs)
    {
        integration::BotActivityLog::Record(
            me, _identity.name, _identity.id,
            "activity_tick", step.label);
    }

    if (_activityTimer >= durationMs)
    {
        integration::BotActivityLog::Record(
            me, _identity.name, _identity.id,
            "activity_complete", step.label);
        _activityTimer = 0;
        AdvanceStep();
    }
}

void WorldBotCreatureAI::AdvanceStep()
{
    ++_currentStep;
    _traveling     = false;
    _activityTimer = 0;
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);

    if (_currentStep >= _session.steps.size())
        CompletSession();
}

void WorldBotCreatureAI::CompletSession()
{
    if (_sessionDone)
        return;
    _sessionDone = true;

    std::uint32_t const zoneId = me->GetZoneId();

    integration::BotActivityLog::Record(
        me, _identity.name, _identity.id,
        "session_complete",
        "zone=" + std::to_string(zoneId) +
        " online_ms=" + std::to_string(_worldOnlineMs));

    GetIdentityRepo().CompleteWorldSession(_identity.id, zoneId, _worldOnlineMs);

    me->DespawnOrUnsummon(Milliseconds(1000));
}

void WorldBotCreatureAI::JustDied(Unit* /*killer*/)
{
    integration::BotActivityLog::Record(
        me, _identity.name, _identity.id,
        "status_change",
        "was attacked - Died. waiting to respawn.");

    // Guard: if session ended cleanly this is already done.
    // If the creature was forcibly removed (e.g. server shutdown), still release.
    if (!_sessionDone && _sessionReady)
    {
        GetIdentityRepo().CompleteWorldSession(
            _identity.id,
            me ? me->GetZoneId() : 0,
            _worldOnlineMs);
    }
}

} // namespace ai
} // namespace living_world

// ---------------------------------------------------------------------------
// CreatureScript registration
// ---------------------------------------------------------------------------

using namespace living_world::ai;

class WorldBotCreatureScript : public CreatureScript
{
public:
    WorldBotCreatureScript()
        : CreatureScript("worldbot_ai")
    {
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new WorldBotCreatureAI(creature);
    }
};

void AddSC_WorldBotCreatureAI()
{
    new WorldBotCreatureScript();
}
