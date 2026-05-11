#include "Config.h"
#include "DatabaseEnv.h"
#include "IWorld.h"
#include "Log.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include "ai/AbstractWorldBotProgressor.h"
#include "ai/WorldBotCreatureAI.h"
#include "script/AmbientSpawnOverride.h"
#include "script/WorldBotHotZoneTracker.h"
#include "integration/BotActivityLog.h"
#include "integration/SqlBotIdentityRepository.h"
#include "integration/SqlBotGlobalConfigRepository.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "integration/SqlBotOocConfigRepository.h"
#include "integration/SqlBotTalentPreferenceRepository.h"
#include "integration/SqlZoneIndexRepository.h"
#include "QueryResult.h"
#include "service/BotActivitySessionComposer.h"
#include "service/BotQuestRewardService.h"

#include <optional>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace living_world
{
// Live economy scale. Initialised from config at startup and updated by the
// .lw economy command. All cost hooks read this instead of hitting ConfigMgr
// on every transaction, so the command takes effect immediately.
float g_economyScale = 1.0f;

void ApplyEconomyScale(float scale, bool isReload)
{
    if (scale <= 0.0f)
    {
        LOG_ERROR("server.worldserver",
            "[LivingWorld] EconomyScale must be > 0 (got {}). Keeping current value.",
            scale);
        return;
    }

    g_economyScale = scale;

    // Repair cost is read dynamically on every repair — takes effect now.
    sWorld->setRate(RATE_REPAIRCOST, scale);

    LOG_INFO("server.worldserver",
        "[LivingWorld] EconomyScale={}{} applied.",
        scale,
        isReload
            ? " (reload — repairs + trainers live; vendor prices need restart)"
            : "");
}
} // namespace living_world

class LivingWorldWorldScript final : public WorldScript
{
public:
    LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        float const scale = sConfigMgr->GetOption<float>("LivingWorld.EconomyScale", 1.0f);
        living_world::ApplyEconomyScale(scale, reload);

        _targetAmbientPop = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientPopulation", 3);
        _populationTickMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientPopulationTickMs", 15 * 1000);

        _forcedSpawnCount = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientForceSpawnCount", 0);
        _forcedSpawnPoint.mapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientForceSpawnMapId", 0);
        _forcedSpawnPoint.x = sConfigMgr->GetOption<float>(
            "LivingWorld.AmbientForceSpawnX", 0.0f);
        _forcedSpawnPoint.y = sConfigMgr->GetOption<float>(
            "LivingWorld.AmbientForceSpawnY", 0.0f);
        _forcedSpawnPoint.z = sConfigMgr->GetOption<float>(
            "LivingWorld.AmbientForceSpawnZ", 0.0f);

        _debugSyntheticInterestEnabled = sConfigMgr->GetOption<bool>(
            "LivingWorld.DebugSyntheticInterestEnabled", false);
        _debugSyntheticInterestMapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestMapId", 0);
        _debugSyntheticInterestZoneId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestZoneId", 0);
        _debugSyntheticInterestSwitchMapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestSwitchMapId", 0);
        _debugSyntheticInterestSwitchZoneId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestSwitchZoneId", 0);
        _debugSyntheticInterestSwitchMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestSwitchMs", 0);
        _debugSyntheticInterestClearMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestClearMs", 0);
        _debugSyntheticInterestElapsedMs = 0;
        _debugSyntheticInterestSwitched = false;
        _debugSyntheticInterestCleared = false;

        std::uint32_t const debugHotZoneCooldownMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugHotZoneCooldownMs", 0);
        living_world::script::SetWorldBotHotZoneCooldownOverrideMs(debugHotZoneCooldownMs);

        if (_debugSyntheticInterestEnabled && _debugSyntheticInterestMapId != 0)
        {
            living_world::script::SetSyntheticWorldBotInterest(
                _debugSyntheticInterestMapId,
                _debugSyntheticInterestZoneId);
            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] SyntheticInterest enabled map={} zone={} switch_ms={} switch_map={} switch_zone={} hot_cooldown_override_ms={}",
                _debugSyntheticInterestMapId,
                _debugSyntheticInterestZoneId,
                _debugSyntheticInterestSwitchMs,
                _debugSyntheticInterestSwitchMapId,
                _debugSyntheticInterestSwitchZoneId,
                _debugSyntheticInterestClearMs,
                debugHotZoneCooldownMs);
        }
        else
        {
            living_world::script::ClearSyntheticWorldBotInterest();
        }

        // Force the first population check on the first world update after
        // startup/reload instead of waiting a full tick interval. This makes
        // autonomous world-bot activity visible immediately while preserving
        // the configured steady-state cadence for subsequent checks.
        _populationTimer = _populationTickMs;
    }

    void OnStartup() override
    {
        living_world::service::BotQuestRewardService().EnsureSchema();
        living_world::integration::SqlBotHazardConfigRepository().EnsureSchema();
        living_world::integration::SqlBotGlobalConfigRepository().EnsureSchema();
        living_world::integration::SqlBotOocConfigRepository().EnsureSchema();
        living_world::integration::SqlBotTalentPreferenceRepository().EnsureSchema();

        living_world::integration::SqlBotIdentityRepository identityRepo;
        std::uint32_t const recovered = identityRepo.RecoverStaleActiveSessions();
        if (recovered > 0)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] Recovered {} stale active world-bot identities on startup.",
                recovered);
        }
    }

    void OnUpdate(std::uint32_t diff) override
    {
        TickSyntheticInterestBeacon(diff);

        _abstractTickAccum += diff;
        if (_abstractTickAccum >= 1000)
        {
            living_world::script::PruneWorldBotHotZones();
            DematerializeInactiveWorldBots();
            TickAbstractWorldBots(_abstractTickAccum);
            _abstractTickAccum = 0;
        }

        _populationTimer += diff;
        if (_populationTimer < _populationTickMs)
            return;
        _populationTimer = 0;

        if (_targetAmbientPop == 0)
            return;

        TickAmbientPopulation();
    }

private:
    // Spawn position chosen for a world bot materialization.
    struct SpawnPoint { std::uint32_t mapId; float x, y, z; };

    struct AbstractWorldBotRuntime
    {
        living_world::integration::BotIdentityRecord identity;
        living_world::service::AmbientSession session;
        living_world::ai::AbstractWorldBotProgressState progress;
        std::uint64_t worldOnlineMs = 0;
    };

    struct MaterializedWorldBotHandle
    {
        std::uint32_t mapId = 0;
        ObjectGuid guid;
    };

    std::uint32_t _populationTimer   = 0;
    std::uint32_t _targetAmbientPop  = 3;
    std::uint32_t _populationTickMs  = 15 * 1000;
    std::uint32_t _forcedSpawnCount  = 0;
    std::uint32_t _abstractTickAccum = 0;
    bool _debugSyntheticInterestEnabled = false;
    bool _debugSyntheticInterestSwitched = false;
    bool _debugSyntheticInterestCleared = false;
    std::uint32_t _debugSyntheticInterestMapId = 0;
    std::uint32_t _debugSyntheticInterestZoneId = 0;
    std::uint32_t _debugSyntheticInterestSwitchMapId = 0;
    std::uint32_t _debugSyntheticInterestSwitchZoneId = 0;
    std::uint32_t _debugSyntheticInterestSwitchMs = 0;
    std::uint32_t _debugSyntheticInterestClearMs = 0;
    std::uint32_t _debugSyntheticInterestElapsedMs = 0;
    SpawnPoint _forcedSpawnPoint { 0u, 0.0f, 0.0f, 0.0f };
    std::unordered_map<std::uint32_t, AbstractWorldBotRuntime> _abstractWorldBots;
    std::unordered_map<std::uint32_t, MaterializedWorldBotHandle> _materializedWorldBots;

    // Entry ID of the generic world bot creature_template.
    // Defined in data/sql/world/living_world_world_bot_template.sql.
    static constexpr std::uint32_t WorldBotEntry = 9900001;

    static std::optional<SpawnPoint> ToSpawnPoint(
        living_world::model::ZoneEntry const& zone)
    {
        return SpawnPoint{ zone.mapId, zone.anchorX, zone.anchorY, zone.anchorZ };
    }

    bool HasForcedSpawnOverride() const
    {
        return living_world::script::HasForcedSpawnOverride({
            _forcedSpawnCount,
            _forcedSpawnPoint.mapId
        });
    }

    static std::uint32_t GetHubZoneIdForIdentity(
        living_world::integration::BotIdentityRecord const& identity)
    {
        if (identity.homeZoneId != 0)
            return identity.homeZoneId;

        if (identity.level < 60)
            return identity.faction == 2 ? 1637u : 1519u;

        if (identity.level < 70)
            return 3703u;

        return 4395u;
    }

    static std::optional<SpawnPoint> ResolveSpawnPoint(
        living_world::integration::BotIdentityRecord const& identity,
        living_world::service::AmbientSession const& session)
    {
        living_world::integration::SqlZoneIndexRepository zoneRepo;

        std::optional<std::uint16_t> requiredMapId;
        if (!session.steps.empty())
            requiredMapId = session.steps.front().mapId;

        auto matchesRequiredMap =
            [&](living_world::model::ZoneEntry const& zone) -> bool
            {
                return !requiredMapId || zone.mapId == *requiredMapId;
            };

        if (identity.lastSeenZoneId != 0)
        {
            if (auto const zone = zoneRepo.Find(identity.lastSeenZoneId))
            {
                if (matchesRequiredMap(*zone))
                    return ToSpawnPoint(*zone);
            }
        }

        if (auto const hubZone = zoneRepo.Find(GetHubZoneIdForIdentity(identity)))
        {
            if (matchesRequiredMap(*hubZone))
                return ToSpawnPoint(*hubZone);
        }

        if (!session.steps.empty())
        {
            living_world::service::AmbientStep const& first = session.steps.front();
            return SpawnPoint{ first.mapId, first.x, first.y, first.z };
        }

        return std::nullopt;
    }

    // Count currently active creature world bots in identity ledger.
    static std::uint32_t CountOnlineWorldBots()
    {
        QueryResult qr = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM living_world_bot_identity "
            "WHERE is_available = 0 AND is_retired = 0");
        if (!qr)
            return 0;
        return qr->Fetch()[0].Get<std::uint32_t>();
    }

    static void LogActiveWorldBotRoster()
    {
        QueryResult qr = CharacterDatabase.Query(
            "SELECT i.id, i.name, a.event_type, a.detail, a.map_id, a.zone_id, a.pos_x, a.pos_y, a.pos_z "
            "FROM living_world_bot_identity i "
            "LEFT JOIN living_world_bot_activity_log a ON a.id = ("
            "  SELECT MAX(id) FROM living_world_bot_activity_log WHERE bot_guid = i.id) "
            "WHERE i.is_available = 0 AND i.is_retired = 0 "
            "ORDER BY i.id ASC");
        if (!qr)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] ActiveWorldBots: none");
            return;
        }

        do
        {
            Field const* f = qr->Fetch();
            std::uint64_t const identityId = f[0].Get<std::uint64_t>();
            std::string const name = f[1].Get<std::string>();
            std::string const eventType = f[2].IsNull() ? "unknown" : f[2].Get<std::string>();
            std::string const detail = f[3].IsNull() ? "" : f[3].Get<std::string>();
            std::uint32_t const mapId = f[4].IsNull() ? 0u : f[4].Get<std::uint32_t>();
            std::uint32_t const zoneId = f[5].IsNull() ? 0u : f[5].Get<std::uint32_t>();
            float const x = f[6].IsNull() ? 0.f : f[6].Get<float>();
            float const y = f[7].IsNull() ? 0.f : f[7].Get<float>();
            float const z = f[8].IsNull() ? 0.f : f[8].Get<float>();

            LOG_INFO("server.worldserver",
                "[LivingWorld] ActiveWorldBot: name='{}' identity={} event='{}' detail='{}' map={} zone={} pos=({:.1f},{:.1f},{:.1f})",
                name, identityId, eventType, detail, mapId, zoneId, x, y, z);
        } while (qr->NextRow());
    }

    static std::uint32_t ResolveStepZoneId(
        living_world::service::AmbientSession const& session,
        std::size_t stepIndex)
    {
        if (stepIndex >= session.steps.size())
            return 0;

        living_world::service::AmbientStep const& step = session.steps[stepIndex];
        if (step.taskIndex < 0)
            return 0;

        std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
        if (taskIndex >= session.tasks.size())
            return 0;

        return session.tasks[taskIndex].targetZoneId;
    }

    static bool HasInterestedPlayerForMapAndZone(std::uint32_t mapId, std::uint32_t zoneId)
    {
        if (living_world::script::HasSyntheticWorldBotInterest(mapId, zoneId))
            return true;

        std::shared_lock<std::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
        {
            (void)guid;
            if (!player || !player->IsInWorld() || !player->GetSession() || player->GetSession()->IsBotSession())
                continue;

            if (player->IsInFlight())
                continue;

            if (player->GetMapId() != mapId)
                continue;

            if (zoneId != 0 && player->GetZoneId() != zoneId)
                continue;

            return true;
        }

        return false;
    }

    static bool IsZoneHotOrInterested(std::uint32_t mapId, std::uint32_t zoneId)
    {
        return HasInterestedPlayerForMapAndZone(mapId, zoneId)
            || living_world::script::IsWorldBotZoneHot(mapId, zoneId);
    }

    void TickSyntheticInterestBeacon(std::uint32_t diff)
    {
        if (!_debugSyntheticInterestEnabled || _debugSyntheticInterestMapId == 0)
            return;

        _debugSyntheticInterestElapsedMs += diff;

        if (!_debugSyntheticInterestSwitched
            && _debugSyntheticInterestSwitchMs != 0
            && _debugSyntheticInterestSwitchMapId != 0
            && _debugSyntheticInterestElapsedMs >= _debugSyntheticInterestSwitchMs)
        {
            living_world::script::SetSyntheticWorldBotInterest(
                _debugSyntheticInterestSwitchMapId,
                _debugSyntheticInterestSwitchZoneId);
            _debugSyntheticInterestSwitched = true;

            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] SyntheticInterest switched map={} zone={} after_ms={}",
                _debugSyntheticInterestSwitchMapId,
                _debugSyntheticInterestSwitchZoneId,
                _debugSyntheticInterestSwitchMs);
        }

        if (!_debugSyntheticInterestCleared
            && _debugSyntheticInterestClearMs != 0
            && _debugSyntheticInterestElapsedMs >= _debugSyntheticInterestClearMs)
        {
            living_world::script::ClearSyntheticWorldBotInterest();
            _debugSyntheticInterestCleared = true;

            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] SyntheticInterest cleared after_ms={}",
                _debugSyntheticInterestClearMs);
        }
    }

    static bool CanMaterializeAbstractRuntime(
        AbstractWorldBotRuntime const& runtime)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return false;

        living_world::service::AmbientStep const& step = runtime.session.steps[runtime.progress.currentStep];
        if (step.type == living_world::service::AmbientStepType::Travel
            && runtime.progress.stepStartMapId != 0
            && runtime.progress.stepStartMapId != step.mapId)
        {
            return false;
        }

        std::uint32_t const zoneId = ResolveStepZoneId(runtime.session, runtime.progress.currentStep);
        std::uint32_t const mapId = runtime.progress.stepStartMapId != 0
            ? runtime.progress.stepStartMapId
            : step.mapId;
        return HasInterestedPlayerForMapAndZone(mapId, zoneId);
    }

    static std::string DescribeAbstractRuntime(
        AbstractWorldBotRuntime const& runtime)
    {
        return "source_kind='" + (runtime.session.sourceKind.empty() ? std::string("unknown") : runtime.session.sourceKind)
            + "' source_key='" + (!runtime.session.sourceKey.empty() ? runtime.session.sourceKey : runtime.session.activityKey)
            + "' step=" + std::to_string(runtime.progress.currentStep)
            + " step_elapsed_ms=" + std::to_string(runtime.progress.stepElapsedMs)
            + " world_online_ms=" + std::to_string(runtime.worldOnlineMs);
    }

    static std::string DescribeAbstractResumeState(
        living_world::integration::BotIdentityRecord const& identity)
    {
        if (identity.lastSeenZoneId != 0)
        {
            return "resume_from_zone=" + std::to_string(identity.lastSeenZoneId)
                + " session_count=" + std::to_string(identity.sessionCount);
        }

        return "fresh_spawn session_count=" + std::to_string(identity.sessionCount);
    }

    bool MaterializeAbstractWorldBot(AbstractWorldBotRuntime const& runtime)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return false;

        living_world::ai::AbstractWorldBotInterpolatedPosition const pos =
            living_world::ai::ComputeAbstractWorldBotInterpolatedPosition(runtime.session, runtime.progress);

        Map* map = sMapMgr->FindMap(pos.mapId, 0);
        if (!map)
            return false;

        Position spawnPos;
        spawnPos.Relocate(pos.x, pos.y, pos.z, 0.0f);
        Creature* bot = map->SummonCreature(WorldBotEntry, spawnPos);
        if (!bot)
            return false;

        living_world::integration::BotActivityLog::RecordAbstract(
            runtime.identity.name,
            runtime.identity.id,
            "status_change",
            "materializing_from_abstract -> " + DescribeAbstractRuntime(runtime),
            pos.mapId,
            ResolveStepZoneId(runtime.session, runtime.progress.currentStep),
            pos.x,
            pos.y,
            pos.z);

        if (auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI()))
        {
            ai->SetIdentityAndSession(
                runtime.identity,
                runtime.session,
                runtime.progress.currentStep,
                runtime.progress.stepElapsedMs,
                runtime.worldOnlineMs,
                true,
                true);
            _materializedWorldBots[runtime.identity.id] = MaterializedWorldBotHandle{
                pos.mapId,
                bot->GetGUID()
            };
            return true;
        }

        bot->DespawnOrUnsummon(Milliseconds(0));
        return false;
    }

    void DematerializeInactiveWorldBots()
    {
        if (_materializedWorldBots.empty())
            return;

        for (auto itr = _materializedWorldBots.begin(); itr != _materializedWorldBots.end(); )
        {
            MaterializedWorldBotHandle const& handle = itr->second;
            Map* map = sMapMgr->FindMap(handle.mapId, 0);
            if (!map)
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            Creature* bot = map->GetCreature(handle.guid);
            if (!bot || !bot->IsInWorld())
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI());
            if (!ai)
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            living_world::ai::WorldBotCreatureAI::RuntimeSnapshot snapshot;
            if (!ai->BuildRuntimeSnapshot(snapshot))
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            std::uint32_t const zoneId = ResolveStepZoneId(snapshot.session, snapshot.progress.currentStep);
            std::uint32_t const mapId = snapshot.progress.stepStartMapId != 0
                ? snapshot.progress.stepStartMapId
                : (snapshot.progress.currentStep < snapshot.session.steps.size()
                    ? snapshot.session.steps[snapshot.progress.currentStep].mapId
                    : 0u);
            if (IsZoneHotOrInterested(mapId, zoneId))
            {
                ++itr;
                continue;
            }

            AbstractWorldBotRuntime runtime;
            runtime.identity = snapshot.identity;
            runtime.session = snapshot.session;
            runtime.progress = snapshot.progress;
            runtime.worldOnlineMs = snapshot.worldOnlineMs;

            _abstractWorldBots[runtime.identity.id] = runtime;

            living_world::integration::BotActivityLog::RecordAbstract(
                runtime.identity.name,
                runtime.identity.id,
                "status_change",
                "dematerializing_to_abstract -> " + DescribeAbstractRuntime(runtime),
                runtime.progress.stepStartMapId,
                zoneId,
                runtime.progress.stepStartX,
                runtime.progress.stepStartY,
                runtime.progress.stepStartZ);

            bot->DespawnOrUnsummon(Milliseconds(0));
            itr = _materializedWorldBots.erase(itr);
        }
    }

    void TickAbstractWorldBots(std::uint32_t diff)
    {
        if (_abstractWorldBots.empty())
            return;

        for (auto itr = _abstractWorldBots.begin(); itr != _abstractWorldBots.end(); )
        {
            AbstractWorldBotRuntime& runtime = itr->second;

            if (CanMaterializeAbstractRuntime(runtime) && MaterializeAbstractWorldBot(runtime))
            {
                itr = _abstractWorldBots.erase(itr);
                continue;
            }

            runtime.worldOnlineMs += diff;
            auto const outcome = living_world::ai::AdvanceAbstractWorldBotProgress(
                runtime.session,
                runtime.progress,
                diff);

            if (outcome.advancedStep)
            {
                living_world::integration::BotActivityLog::RecordAbstract(
                    runtime.identity.name,
                    runtime.identity.id,
                    "status_change",
                    "abstract_progress -> " + DescribeAbstractRuntime(runtime),
                    runtime.progress.stepStartMapId,
                    ResolveStepZoneId(runtime.session, std::min(runtime.progress.currentStep, runtime.session.steps.empty() ? std::size_t{0} : runtime.session.steps.size() - 1)),
                    runtime.progress.stepStartX,
                    runtime.progress.stepStartY,
                    runtime.progress.stepStartZ);
            }

            if (outcome.sessionComplete)
            {
                std::uint32_t const lastSeenZoneId = runtime.session.steps.empty()
                    ? runtime.identity.lastSeenZoneId
                    : ResolveStepZoneId(runtime.session, runtime.session.steps.size() - 1);

                living_world::integration::BotActivityLog::RecordAbstract(
                    runtime.identity.name,
                    runtime.identity.id,
                    "session_complete",
                    "abstract_offscreen session_complete world_online_ms=" + std::to_string(runtime.worldOnlineMs),
                    runtime.progress.stepStartMapId,
                    lastSeenZoneId,
                    runtime.progress.stepStartX,
                    runtime.progress.stepStartY,
                    runtime.progress.stepStartZ);

                living_world::integration::SqlBotIdentityRepository().CompleteWorldSession(
                    runtime.identity.id,
                    lastSeenZoneId,
                    runtime.worldOnlineMs);
                itr = _abstractWorldBots.erase(itr);
                continue;
            }

            ++itr;
        }
    }

    void TickAmbientPopulation()
    {
        std::uint32_t const online = CountOnlineWorldBots();
        LOG_INFO("server.worldserver",
            "[LivingWorld] AmbientPopulationTick: online={} target={} tick_ms={}",
            online, _targetAmbientPop, _populationTickMs);

        if (HasForcedSpawnOverride())
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: forced spawn override active for up to {} bots at map={} pos=({:.1f},{:.1f},{:.1f})",
                _forcedSpawnCount,
                _forcedSpawnPoint.mapId,
                _forcedSpawnPoint.x,
                _forcedSpawnPoint.y,
                _forcedSpawnPoint.z);
        }

        LogActiveWorldBotRoster();

        if (online >= _targetAmbientPop)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: population already satisfied.");
            return;
        }

        std::uint32_t const toSpawn = _targetAmbientPop - online;
        LOG_INFO("server.worldserver",
            "[LivingWorld] AmbientPopulationTick: requesting up to {} new world bots.",
            toSpawn);

        // Load available identities — mix of factions.
        living_world::integration::SqlBotIdentityRepository identityRepo;
        std::vector<living_world::integration::BotIdentityRecord> identities =
            identityRepo.LoadAvailable(0, toSpawn);

        if (identities.empty())
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: no available bot identities.");
            return;
        }

        LOG_INFO("server.worldserver",
            "[LivingWorld] AmbientPopulationTick: selected {} candidate identities.",
            identities.size());

        living_world::service::BotActivitySessionComposer composer;
        std::uint32_t forcedSpawnedThisTick = 0;

        for (auto const& identity : identities)
        {
            bool usedForcedSpawn = false;

            // Compose a session for this identity.
            auto session = composer.Compose(
                identity.faction,
                identity.level,
                identity.hasHerbalism,
                identity.hasMining,
                identity.hasFishing,
                identity.lastSeenZoneId != 0 ? identity.lastSeenZoneId : GetHubZoneIdForIdentity(identity),
                identity.homeZoneId,
                identity.homeAnchorPointKey,
                identity.homeBindPointKey);

            if (!session)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: no session for "
                    "identity='{}' level={} faction={}",
                    identity.name, identity.level, identity.faction);
                continue;
            }

            std::optional<SpawnPoint> sp;
            if (living_world::script::ShouldUseForcedSpawn(
                    { _forcedSpawnCount, _forcedSpawnPoint.mapId },
                    online,
                    forcedSpawnedThisTick))
            {
                sp = _forcedSpawnPoint;
                usedForcedSpawn = true;
            }
            else
            {
                sp = ResolveSpawnPoint(identity, *session);
            }

            if (!sp)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: no valid spawn point for "
                    "identity='{}' level={} faction={} lastSeenZone={}",
                    identity.name, identity.level, identity.faction, identity.lastSeenZoneId);
                continue;
            }

            AbstractWorldBotRuntime abstractRuntime;
            abstractRuntime.identity = identity;
            abstractRuntime.session = *session;
            abstractRuntime.progress.currentStep = 0;
            abstractRuntime.progress.stepElapsedMs = 0;
            abstractRuntime.progress.stepStartMapId = static_cast<std::uint16_t>(sp->mapId);
            abstractRuntime.progress.stepStartX = sp->x;
            abstractRuntime.progress.stepStartY = sp->y;
            abstractRuntime.progress.stepStartZ = sp->z;

            if (!CanMaterializeAbstractRuntime(abstractRuntime))
            {
                identityRepo.MarkActive(identity.id);
                _abstractWorldBots[identity.id] = abstractRuntime;

                living_world::integration::BotActivityLog::RecordAbstract(
                    identity.name,
                    identity.id,
                    "session_start",
                    "abstract_offscreen " + DescribeAbstractRuntime(abstractRuntime),
                    abstractRuntime.progress.stepStartMapId,
                    ResolveStepZoneId(abstractRuntime.session, 0),
                    abstractRuntime.progress.stepStartX,
                    abstractRuntime.progress.stepStartY,
                    abstractRuntime.progress.stepStartZ);

                living_world::integration::BotActivityLog::RecordAbstract(
                    identity.name,
                    identity.id,
                    "status_change",
                    DescribeAbstractResumeState(identity) + " -> abstract_offscreen",
                    abstractRuntime.progress.stepStartMapId,
                    ResolveStepZoneId(abstractRuntime.session, 0),
                    abstractRuntime.progress.stepStartX,
                    abstractRuntime.progress.stepStartY,
                    abstractRuntime.progress.stepStartZ);

                LOG_INFO("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: abstracted '{}' level={} spec='{}' source_kind='{}' source_key='{}' steps={} map={} pos=({:.1f},{:.1f},{:.1f}){}",
                    identity.name,
                    identity.level,
                    identity.specKey,
                    session->sourceKind.empty() ? "unknown" : session->sourceKind,
                    session->sourceKey.empty() ? session->activityKey : session->sourceKey,
                    session->steps.size(),
                    sp->mapId,
                    sp->x,
                    sp->y,
                    sp->z,
                    usedForcedSpawn ? " forced_spawn_override" : "");
                continue;
            }

            // Find the correct world map.
            Map* map = sMapMgr->FindMap(sp->mapId, 0);
            if (!map)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: map {} not loaded, "
                    "skipping identity='{}'",
                    sp->mapId, identity.name);
                continue;
            }

            // Summon the creature.
            Position pos;
            pos.Relocate(sp->x, sp->y, sp->z, 0.f);
            Creature* bot = map->SummonCreature(WorldBotEntry, pos);
            if (!bot)
            {
                LOG_ERROR("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: SummonCreature failed "
                    "for identity='{}'",
                    identity.name);
                continue;
            }

            // Give the AI its identity and session.
            if (auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI()))
            {
                ai->SetIdentityAndSession(identity, *session);
                _materializedWorldBots[identity.id] = MaterializedWorldBotHandle{
                    sp->mapId,
                    bot->GetGUID()
                };
                if (usedForcedSpawn)
                    ++forcedSpawnedThisTick;

                LOG_INFO("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: spawned '{}' "
                    "level={} spec='{}' source_kind='{}' source_key='{}' steps={} map={} pos=({:.1f},{:.1f},{:.1f}){}",
                    identity.name, identity.level,
                    identity.specKey,
                    session->sourceKind.empty() ? "unknown" : session->sourceKind,
                    session->sourceKey.empty() ? session->activityKey : session->sourceKey,
                    session->steps.size(),
                    sp->mapId, sp->x, sp->y, sp->z,
                    usedForcedSpawn
                        ? " forced_spawn_override"
                        : "");
            }
            else
            {
                LOG_ERROR("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: creature has wrong AI "
                    "for identity='{}' — check creature_template ScriptName",
                    identity.name);
                bot->DespawnOrUnsummon(Milliseconds(0));
            }
        }
    }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}
