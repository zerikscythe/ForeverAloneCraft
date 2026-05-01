#include "ScriptMgr.h"
#include "service/BotQuestRewardService.h"

class LivingWorldWorldScript final : public WorldScript
{
public:
    LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }

    void OnStartup() override
    {
        living_world::service::BotQuestRewardService().EnsureSchema();
    }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}
