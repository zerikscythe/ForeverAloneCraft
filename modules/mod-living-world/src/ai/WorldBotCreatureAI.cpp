#include "ai/WorldBotCreatureAI.h"

#include "Creature.h"
#include "CreatureAIImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ScriptMgr.h"

namespace living_world
{
namespace ai
{

namespace
{
integration::SqlBotIdentityRepository& GetIdentityRepo()
{
    static integration::SqlBotIdentityRepository repo;
    return repo;
}
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
    me->SetReactState(REACT_PASSIVE);
    me->SetUInt32Value(UNIT_NPC_FLAGS, 0);
    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);
}

void WorldBotCreatureAI::SetIdentityAndSession(
    integration::BotIdentityRecord const& identity,
    service::AmbientSession          const& session)
{
    _identity     = identity;
    _session      = session;
    _currentStep  = 0;
    _activityTimer = 0;
    _traveling    = false;
    _sessionDone  = false;
    _sessionReady = true;

    ApplyIdentityToCreature();

    GetIdentityRepo().MarkActive(_identity.id);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "session_start",
        "steps=" + std::to_string(_session.steps.size()));
}

void WorldBotCreatureAI::ApplyIdentityToCreature()
{
    if (!me)
        return;

    me->SetLevel(_identity.level);
    me->SetDisplayId(_identity.displayId);

    // Class-appropriate unit flags
    me->SetByteValue(UNIT_FIELD_BYTES_0, 3, _identity.classId);
}

void WorldBotCreatureAI::UpdateAI(uint32 diff)
{
    if (!_sessionReady || _sessionDone)
        return;

    _tickAccum += diff;
    if (_tickAccum < TickIntervalMs)
        return;
    _tickAccum -= TickIntervalMs;

    TickStep(TickIntervalMs);
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
                AdvanceStep();
            }
        }
        return;
    }

    // Activity step — count down duration.
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
        "zone=" + std::to_string(zoneId));

    GetIdentityRepo().MarkAvailable(_identity.id, zoneId);

    me->DespawnOrUnsummon(Milliseconds(1000));
}

void WorldBotCreatureAI::JustDied(Unit* /*killer*/)
{
    // Guard: if session ended cleanly this is already done.
    // If the creature was forcibly removed (e.g. server shutdown), still release.
    if (!_sessionDone && _sessionReady)
    {
        GetIdentityRepo().MarkAvailable(_identity.id, me ? me->GetZoneId() : 0);
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
