#include "Config.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "IWorld.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "integration/BotSessionFactory.h"
#include "integration/SqlBotGlobalConfigRepository.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "integration/SqlBotOocConfigRepository.h"
#include "integration/SqlBotTalentPreferenceRepository.h"
#include "service/BotQuestRewardService.h"

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

    // Count how many ambient bots are currently marked unavailable (i.e. online).
    static std::uint32_t CountOnlineAmbientBots()
    {
        QueryResult qr = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM living_world_ambient_bot WHERE is_available = 0");
        if (!qr)
            return 0;
        return qr->Fetch()[0].Get<std::uint32_t>();
    }

    void TickAmbientPopulation()
    {
        std::uint32_t const online = CountOnlineAmbientBots();
        if (online >= _targetAmbientPop)
            return;

        std::uint32_t const toSpawn = _targetAmbientPop - online;

        // Load available ambient bots from DB.
        QueryResult qr = CharacterDatabase.Query(
            "SELECT bot_account_id, character_guid FROM living_world_ambient_bot "
            "WHERE is_available = 1 LIMIT {}",
            toSpawn);
        if (!qr)
        {
            LOG_DEBUG("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: no available ambient bots.");
            return;
        }

        do
        {
            Field const* f          = qr->Fetch();
            std::uint32_t accountId = f[0].Get<std::uint32_t>();
            std::uint64_t charGuid  = f[1].Get<std::uint64_t>();

            ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(charGuid);
            auto result = living_world::integration::BotSessionFactory
                ::SpawnHostileBotPlayerOnAccount(accountId, guid);

            if (result.status ==
                living_world::integration::BotSessionSpawnStatus::SpawnQueued)
            {
                // Mark unavailable immediately to avoid double-spawn next tick.
                CharacterDatabase.Execute(
                    "UPDATE living_world_ambient_bot SET is_available = 0 "
                    "WHERE character_guid = {}",
                    charGuid);
                LOG_INFO("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: spawning ambient bot "
                    "accountId={} characterGuid={}",
                    accountId, charGuid);
            }
            else
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: spawn failed "
                    "accountId={} characterGuid={} status={}",
                    accountId, charGuid,
                    static_cast<int>(result.status));
            }
        } while (qr->NextRow());
    }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}
