#include "ai/CompanionAI.h"
#include "service/BotPlayerRegistry.h"

#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <optional>

namespace
{
void AddBotToOwnerGroup(Player* bot, Player* owner)
{
    Group* group = owner->GetGroup();
    if (!group)
    {
        group = new Group();
        if (!group->Create(owner))
        {
            delete group;
            return;
        }
        sGroupMgr->AddGroup(group);
    }
    if (group->IsFull())
        return;
    group->AddMember(bot);
}
} // namespace

class LivingWorldPlayerScript final : public PlayerScript
{
public:
    LivingWorldPlayerScript() : PlayerScript("LivingWorldPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !player->GetSession() ||
            !player->GetSession()->IsBotSession())
        {
            return;
        }

        std::optional<ObjectGuid> ownerGuid =
            living_world::service::BotPlayerRegistry::Instance()
                .RegisterBotPlayer(player);
        if (!ownerGuid)
        {
            return;
        }

        Player* owner = ObjectAccessor::FindPlayer(*ownerGuid);
        if (owner)
        {
            living_world::ai::ScheduleCompanionAI(player, owner);
            AddBotToOwnerGroup(player, owner);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player || !player->GetSession() ||
            !player->GetSession()->IsBotSession())
        {
            return;
        }

        if (Group* group = player->GetGroup())
            group->RemoveMember(player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);

        living_world::service::BotPlayerRegistry::Instance()
            .UnregisterBotPlayer(player);
        LoginDatabase.Execute(
            "UPDATE living_world_bot_account_pool "
            "SET is_available = 1, reserved_for = NULL WHERE account_id = {};",
            player->GetSession()->GetAccountId());
    }
};

void AddSC_LivingWorldPlayerScript()
{
    new LivingWorldPlayerScript();
}
