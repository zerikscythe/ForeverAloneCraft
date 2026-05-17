#include "ai/AmbientBotAI.h"
#include "ai/TravelWatchdog.h"
#include "integration/BotActivityLog.h"
#include "integration/SqlZoneIndexRepository.h"
#include "service/BotActivitySessionComposer.h"

#include "DatabaseEnv.h"
#include "Duration.h"
#include "EventProcessor.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldSession.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace living_world
{
namespace ai
{
namespace
{
// Yards — bot is considered "arrived" once within this distance of anchor.
constexpr float ArrivalThreshold = 15.f;
// How often we log an activity-tick heartbeat (every N ticks of 500ms = N*500ms).
constexpr std::uint32_t ActivityLogIntervalTicks = 120; // 60 seconds

service::AmbientSessionTask const* TryGetTask(
    service::AmbientSession const& session,
    std::int32_t taskIndex)
{
    if (taskIndex < 0)
        return nullptr;

    std::size_t const index = static_cast<std::size_t>(taskIndex);
    if (index >= session.tasks.size())
        return nullptr;

    return &session.tasks[index];
}

std::string DescribeTask(
    service::AmbientSession const& session,
    std::int32_t taskIndex)
{
    service::AmbientSessionTask const* task = TryGetTask(session, taskIndex);
    if (!task)
        return "task=none";

    return "task="
        + std::to_string(static_cast<std::size_t>(taskIndex) + 1)
        + "/" + std::to_string(session.tasks.size())
        + " key='" + task->activityKey
        + "' family='" + task->taskFamily
        + "' type='" + task->activityType
        + "' zone=" + std::to_string(task->targetZoneId)
        + " name='" + task->displayName + "'";
}

std::string DescribeStep(
    service::AmbientSession const& session,
    std::size_t stepIndex,
    service::AmbientStep const& step)
{
    std::string detail =
        "step=" + std::to_string(stepIndex + 1)
        + "/" + std::to_string(session.steps.size())
        + " label='" + step.label + "' "
        + DescribeTask(session, step.taskIndex);

    return detail;
}

bool IsFirstStepForTask(
    service::AmbientSession const& session,
    std::size_t stepIndex,
    service::AmbientStep const& step)
{
    if (step.taskIndex < 0)
        return false;

    if (stepIndex == 0)
        return true;

    return session.steps[stepIndex - 1].taskIndex != step.taskIndex;
}

bool IsLastStepForTask(
    service::AmbientSession const& session,
    std::size_t stepIndex,
    service::AmbientStep const& step)
{
    if (step.taskIndex < 0)
        return false;

    if (stepIndex + 1 >= session.steps.size())
        return true;

    return session.steps[stepIndex + 1].taskIndex != step.taskIndex;
}

std::string DescribeSessionSummary(service::AmbientSession const& session)
{
    std::string detail =
        "session='" + session.displayName
        + "' source_kind='" + (session.sourceKind.empty() ? std::string("unknown") : session.sourceKind)
        + "' source_key='" + (!session.sourceKey.empty() ? session.sourceKey : session.activityKey)
        + "' tasks=" + std::to_string(session.tasks.size())
        + " steps=" + std::to_string(session.steps.size());

    for (std::size_t i = 0; i < session.tasks.size(); ++i)
    {
        service::AmbientSessionTask const& task = session.tasks[i];
        detail += " | task="
            + std::to_string(i + 1)
            + "/" + std::to_string(session.tasks.size())
            + " key='" + task.activityKey
            + "' family='" + task.taskFamily
            + "' zone=" + std::to_string(task.targetZoneId)
            + " name='" + task.displayName + "'";
    }

    return detail;
}

// ---------------------------------------------------------------------------
// Event
// ---------------------------------------------------------------------------
class AmbientBotAIEvent final : public BasicEvent
{
public:
    AmbientBotAIEvent(
        ObjectGuid botGuid,
        service::AmbientSession session,
        std::size_t currentStep = 0,
        std::uint32_t stepElapsedSec = 0,
        std::uint8_t  notInWorldRetries = 0,
        std::uint32_t activityLogTick = 0,
        TravelWatchdogState travelWatchdog = {})
        : _botGuid(botGuid)
        , _session(std::move(session))
        , _currentStep(currentStep)
        , _stepElapsedSec(stepElapsedSec)
        , _notInWorldRetries(notInWorldRetries)
        , _activityLogTick(activityLogTick)
        , _travelWatchdog(travelWatchdog)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* bot = ObjectAccessor::FindPlayer(_botGuid);
        if (!bot)
            return true; // Bot gone — stop.

        if (!bot->IsInWorld())
        {
            constexpr std::uint8_t MaxRetries = 8;
            if (_notInWorldRetries >= MaxRetries)
                return true;
            Milliseconds const delay =
                500ms * (1u << std::min(_notInWorldRetries, std::uint8_t{3}));
            bot->m_Events.AddEventAtOffset(
                new AmbientBotAIEvent(
                    _botGuid, _session, _currentStep,
                    _stepElapsedSec, _notInWorldRetries + 1, _activityLogTick, _travelWatchdog),
                delay);
            return true;
        }

        // All steps complete — log out and return to pool.
        if (_currentStep >= _session.steps.size())
        {
            integration::BotActivityLog::Record(bot, "despawn",
                "Session complete: " + _session.displayName);
            CharacterDatabase.Execute(
                "UPDATE living_world_ambient_bot "
                "SET is_available = 1, last_activity_at = NOW() "
                "WHERE character_guid = {}",
                bot->GetGUID().GetCounter());
            if (!bot->GetSession()->PlayerLogout())
                bot->GetSession()->LogoutPlayer(true);
            return true;
        }

        service::AmbientStep const& step = _session.steps[_currentStep];
        TickStep(bot, step);
        return true;
    }

private:
    void TickStep(Player* bot, service::AmbientStep const& step)
    {
        using T = service::AmbientStepType;
        switch (step.type)
        {
            case T::Travel:
                TickTravel(bot, step);
                break;
            case T::Transit:
                TickTransit(bot, step);
                break;
            case T::GatherHerb:
            case T::GatherOre:
            case T::Fish:
            case T::Patrol:
            case T::Idle:
                TickActivity(bot, step);
                break;
        }
    }

    void TickTravel(Player* bot, service::AmbientStep const& step)
    {
        if (_stepElapsedSec == 0)
        {
            ResetTravelWatchdog(_travelWatchdog);
            if (IsFirstStepForTask(_session, _currentStep, step))
            {
                integration::BotActivityLog::Record(
                    bot, "task_start", DescribeTask(_session, step.taskIndex));
            }

            integration::BotActivityLog::Record(
                bot, "step_start", DescribeStep(_session, _currentStep, step));
        }

        // Cross-map travel: teleport the bot to the destination map/zone.
        // Same-map travel: use MovePoint so the bot physically walks.
        if (bot->GetMapId() != step.mapId)
        {
            integration::BotActivityLog::Record(
                bot, "travel_teleport", DescribeStep(_session, _currentStep, step));
            bot->TeleportTo(step.mapId, step.x, step.y, step.z,
                bot->GetOrientation());
            AdvanceAndReschedule(bot, 2000ms);
            return;
        }

        float const dist = bot->GetDistance(step.x, step.y, step.z);
        if (dist <= ArrivalThreshold)
        {
            integration::BotActivityLog::Record(
                bot,
                "travel_arrive",
                DescribeStep(_session, _currentStep, step)
                    + " dist=" + std::to_string(static_cast<int>(dist)) + "m");
            AdvanceAndReschedule(bot, 500ms);
            return;
        }

        TravelWatchdogSignal const signal = UpdateTravelWatchdog(
            _travelWatchdog,
            bot->GetPositionX(),
            bot->GetPositionY(),
            bot->GetPositionZ(),
            500u);
        if (signal == TravelWatchdogSignal::Stuck || signal == TravelWatchdogSignal::Timeout)
        {
            char const* eventType = signal == TravelWatchdogSignal::Timeout
                ? "travel_timeout"
                : "travel_stuck";
            integration::BotActivityLog::Record(
                bot,
                eventType,
                DescribeStep(_session, _currentStep, step)
                    + " action=recover_teleport");
            bot->NearTeleportTo(step.x, step.y, step.z, bot->GetOrientation());
            AdvanceAndReschedule(bot, 500ms);
            return;
        }

        // Issue MovePoint only when we're not already heading there.
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType()
            != POINT_MOTION_TYPE)
        {
            integration::BotActivityLog::Record(
                bot,
                "travel_start",
                DescribeStep(_session, _currentStep, step)
                    + " dist=" + std::to_string(static_cast<int>(dist)) + "m");
            bot->GetMotionMaster()->MovePoint(
                static_cast<std::uint32_t>(_currentStep),
                step.x, step.y, step.z);
        }

        Reschedule(bot, 500ms);
    }

    void TickTransit(Player* bot, service::AmbientStep const& step)
    {
        if (_stepElapsedSec == 0)
        {
            if (IsFirstStepForTask(_session, _currentStep, step))
            {
                integration::BotActivityLog::Record(
                    bot, "task_start", DescribeTask(_session, step.taskIndex));
            }

            integration::BotActivityLog::Record(
                bot, "travel_transit_start", DescribeStep(_session, _currentStep, step));
        }

        _stepElapsedSec += 1;
        if (_stepElapsedSec < step.durationSec)
        {
            Reschedule(bot, 1s);
            return;
        }

        if (bot->GetMapId() != step.mapId)
        {
            bot->TeleportTo(step.mapId, step.x, step.y, step.z, bot->GetOrientation());
        }
        else
        {
            bot->NearTeleportTo(step.x, step.y, step.z, bot->GetOrientation());
        }

        integration::BotActivityLog::Record(
            bot, "travel_transit_arrive", DescribeStep(_session, _currentStep, step));
        AdvanceAndReschedule(bot, 500ms);
    }

    void TickActivity(Player* bot, service::AmbientStep const& step)
    {
        // Log activity start on the very first tick.
        if (_stepElapsedSec == 0)
        {
            if (IsFirstStepForTask(_session, _currentStep, step))
            {
                integration::BotActivityLog::Record(
                    bot, "task_start", DescribeTask(_session, step.taskIndex));
            }

            integration::BotActivityLog::Record(
                bot, "activity_start", DescribeStep(_session, _currentStep, step));
        }

        // Periodic heartbeat log.
        ++_activityLogTick;
        if (_activityLogTick % ActivityLogIntervalTicks == 0)
        {
            integration::BotActivityLog::Record(
                bot,
                "activity_tick",
                DescribeStep(_session, _currentStep, step)
                    + " elapsed=" + std::to_string(_stepElapsedSec) + "s");
        }

        _stepElapsedSec += 1; // Each tick = ~1 second (2 ticks per second * 0.5s)

        if (_stepElapsedSec >= step.durationSec)
        {
            integration::BotActivityLog::Record(
                bot, "activity_complete", DescribeStep(_session, _currentStep, step));

            if (IsLastStepForTask(_session, _currentStep, step))
            {
                integration::BotActivityLog::Record(
                    bot, "task_complete", DescribeTask(_session, step.taskIndex));
            }

            AdvanceAndReschedule(bot, 500ms);
            return;
        }

        Reschedule(bot, 500ms);
    }

    // Move to next step and reschedule.
    void AdvanceAndReschedule(Player* bot, Milliseconds delay)
    {
        bot->m_Events.AddEventAtOffset(
            new AmbientBotAIEvent(
                _botGuid, _session, _currentStep + 1,
                    0, 0, _activityLogTick, {}),
            delay);
    }

    // Reschedule the same step.
    void Reschedule(Player* bot, Milliseconds delay)
    {
        bot->m_Events.AddEventAtOffset(
            new AmbientBotAIEvent(
                _botGuid, _session, _currentStep,
                    _stepElapsedSec, 0, _activityLogTick, _travelWatchdog),
            delay);
    }

    ObjectGuid               _botGuid;
    service::AmbientSession  _session;
    std::size_t              _currentStep      = 0;
    std::uint32_t            _stepElapsedSec   = 0;
    std::uint8_t             _notInWorldRetries = 0;
    std::uint32_t            _activityLogTick  = 0;
    TravelWatchdogState      _travelWatchdog;
};

// Fetch ambient bot's skills from DB to inform session composition.
struct AmbientBotProfile
{
    std::uint8_t faction       = 2;
    std::uint8_t classId       = 0;
    std::uint8_t level         = 1;
    bool         hasHerbalism  = false;
    bool         hasMining     = false;
    bool         hasFishing    = false;
};

std::optional<AmbientBotProfile> LoadAmbientProfile(std::uint64_t charGuid)
{
    QueryResult qr = CharacterDatabase.Query(
        "SELECT faction, class_id, level, "
        "has_herbalism, has_mining, has_fishing "
        "FROM living_world_ambient_bot WHERE character_guid = {}",
        charGuid);
    if (!qr)
        return std::nullopt;

    Field const* f = qr->Fetch();
    AmbientBotProfile p;
    p.faction      = f[0].Get<std::uint8_t>();
    p.classId      = f[1].Get<std::uint8_t>();
    p.level        = f[2].Get<std::uint8_t>();
    p.hasHerbalism = f[3].Get<std::uint8_t>() != 0;
    p.hasMining    = f[4].Get<std::uint8_t>() != 0;
    p.hasFishing   = f[5].Get<std::uint8_t>() != 0;
    return p;
}
} // namespace

void ScheduleAmbientBotAI(Player* botPlayer)
{
    if (!botPlayer)
        return;

    std::uint64_t const guid = botPlayer->GetGUID().GetCounter();

    auto profile = LoadAmbientProfile(guid);
    if (!profile)
    {
        LOG_WARN("server.worldserver",
            "[LivingWorld] ScheduleAmbientBotAI: no ambient profile for guid={}", guid);
        return;
    }

    service::BotActivitySessionComposer composer;
    auto session = composer.Compose(
        profile->faction,
        profile->level,
        profile->hasHerbalism,
        profile->hasMining,
        profile->hasFishing,
        botPlayer->GetZoneId(),
        0,
        "",
        "");

    if (!session)
    {
        LOG_WARN("server.worldserver",
            "[LivingWorld] ScheduleAmbientBotAI: no eligible activity for guid={} "
            "faction={} level={}",
            guid, profile->faction, profile->level);
        // Log out gracefully — nothing to do.
        integration::BotActivityLog::Record(botPlayer, "despawn",
            "No eligible activity found");
        CharacterDatabase.Execute(
            "UPDATE living_world_ambient_bot "
            "SET is_available = 1 WHERE character_guid = {}",
            guid);
        if (!botPlayer->GetSession()->PlayerLogout())
            botPlayer->GetSession()->LogoutPlayer(true);
        return;
    }

    LOG_INFO("server.worldserver",
        "[LivingWorld] AmbientBot guid={} name='{}' assigned {}",
        guid, botPlayer->GetName(), DescribeSessionSummary(*session));

    integration::BotActivityLog::Record(
        botPlayer, "ai_assigned", DescribeSessionSummary(*session));

    botPlayer->m_Events.AddEventAtOffset(
        new AmbientBotAIEvent(botPlayer->GetGUID(), std::move(*session)),
        500ms);
}

} // namespace ai
} // namespace living_world
