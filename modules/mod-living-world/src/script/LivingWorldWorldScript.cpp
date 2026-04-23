#include "ScriptMgr.h"

class LivingWorldWorldScript final : public WorldScript
{
public:
    LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}
