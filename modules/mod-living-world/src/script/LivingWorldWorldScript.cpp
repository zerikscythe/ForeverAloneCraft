#include "Config.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "IWorld.h"
#include "Log.h"
#include "MapMgr.h"
#include "ScriptMgr.h"
#include "ai/WorldBotCreatureAI.h"
#include "integration/SqlBotIdentityRepository.h"
#include "integration/SqlBotGlobalConfigRepository.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "integration/SqlBotOocConfigRepository.h"
#include "integration/SqlBotTalentPreferenceRepository.h"
#include "service/BotActivitySessionComposer.h"
#include "service/BotQuestRewardService.h"

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
            "LivingWorld.AmbientPopulationTickMs", 5 * 60 * 1000);
    }

    void OnStartup() override
    {
        living_world::service::BotQuestRewardService().EnsureSchema();
        living_world::integration::SqlBotHazardConfigRepository().EnsureSchema();
        living_world::integration::SqlBotGlobalConfigRepository().EnsureSchema();
        living_world::integration::SqlBotOocConfigRepository().EnsureSchema();
        living_world::integration::SqlBotTalentPreferenceRepository().EnsureSchema();
    }

    void OnUpdate(std::uint32_t diff) override
    {
        _populationTimer += diff;
        if (_populationTimer < _populationTickMs)
            return;
        _populationTimer = 0;

        if (_targetAmbientPop == 0)
            return;

        TickAmbientPopulation();
    }

private:
    std::uint32_t _populationTimer   = 0;
    std::uint32_t _targetAmbientPop  = 3;
    std::uint32_t _populationTickMs  = 5 * 60 * 1000;

    // Entry ID of the generic world bot creature_template.
    // Defined in data/sql/world/living_world_world_bot_template.sql.
    static constexpr std::uint32_t WorldBotEntry = 9900001;

    // Faction -> (map_id, spawn_x, spawn_y, spawn_z)
    // Horde bots start near Crossroads (Kalimdor); Alliance near Stormwind (EK).
    struct SpawnPoint { std::uint32_t mapId; float x, y, z; };
    static SpawnPoint SpawnPointForFaction(std::uint8_t faction)
    {
        if (faction == 2) // Horde — Crossroads, Barrens, Kalimdor
            return { 1, -462.f, -2642.f, 96.f };
        return { 0, -8924.f, 529.f, 96.f }; // Alliance — outside Stormwind, EK
    }

    // Count currently active creature world bots in identity ledger.
    static std::uint32_t CountOnlineWorldBots()
    {
        QueryResult qr = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM living_world_bot_identity WHERE is_available = 0");
        if (!qr)
            return 0;
        return qr->Fetch()[0].Get<std::uint32_t>();
    }

    void TickAmbientPopulation()
    {
        std::uint32_t const online = CountOnlineWorldBots();
        if (online >= _targetAmbientPop)
            return;

        std::uint32_t const toSpawn = _targetAmbientPop - online;

        // Load available identities — mix of factions.
        living_world::integration::SqlBotIdentityRepository identityRepo;
        std::vector<living_world::integration::BotIdentityRecord> identities =
            identityRepo.LoadAvailable(0, toSpawn);

        if (identities.empty())
        {
            LOG_DEBUG("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: no available bot identities.");
            return;
        }

        living_world::service::BotActivitySessionComposer composer;

        for (auto const& identity : identities)
        {
            // Compose a session for this identity.
            auto session = composer.Compose(
                identity.faction,
                identity.level,
                identity.hasHerbalism,
                identity.hasMining,
                identity.hasFishing);

            if (!session)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: no session for "
                    "identity='{}' level={} faction={}",
                    identity.name, identity.level, identity.faction);
                continue;
            }

            // Find the correct world map.
            SpawnPoint const sp = SpawnPointForFaction(identity.faction);
            Map* map = sMapMgr->FindMap(sp.mapId, 0);
            if (!map)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: map {} not loaded, "
                    "skipping identity='{}'",
                    sp.mapId, identity.name);
                continue;
            }

            // Summon the creature.
            Position pos;
            pos.Relocate(sp.x, sp.y, sp.z, 0.f);
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
                LOG_INFO("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: spawned '{}' "
                    "level={} spec='{}' steps={}",
                    identity.name, identity.level,
                    identity.specKey, session->steps.size());
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
