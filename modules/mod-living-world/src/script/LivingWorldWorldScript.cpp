#include "Config.h"
#include "Config.h"
#include "IWorld.h"
#include "Log.h"
#include "ScriptMgr.h"
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
            "WHERE MoneyCostBase = 0 AND MoneyCost > 0");
        } // namespace living_world

        class LivingWorldWorldScript final : public WorldScript
        {
        public:
            LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }

            void OnAfterConfigLoad(bool reload) override
            {
                float const scale = sConfigMgr->GetOption<float>("LivingWorld.EconomyScale", 1.0f);
                living_world::ApplyEconomyScale(scale, reload);
            }

            void OnStartup() override
            {
                living_world::service::BotQuestRewardService().EnsureSchema();
                living_world::integration::SqlBotHazardConfigRepository().EnsureSchema();
                living_world::integration::SqlBotGlobalConfigRepository().EnsureSchema();
                living_world::integration::SqlBotOocConfigRepository().EnsureSchema();
                living_world::integration::SqlBotTalentPreferenceRepository().EnsureSchema();
            }
        };

        void AddSC_LivingWorldWorldScript()
        {
            new LivingWorldWorldScript();
        }
