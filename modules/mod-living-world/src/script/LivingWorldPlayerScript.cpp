#include "Chat.h"
#include "ai/CompanionAI.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlCharacterEquipmentSyncRepository.h"
#include "integration/SqlCharacterItemSnapshotRepository.h"
#include "integration/SqlCharacterProgressSnapshotRepository.h"
#include "integration/SqlCharacterProgressSyncRepository.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltStartupRecoveryService.h"
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
void RunOwnerStartupRecovery(Player* player)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    living_world::integration::SqlAccountAltRuntimeRepository runtimeRepository;
    living_world::integration::SqlCharacterItemSnapshotRepository
        itemSnapshotRepository;
    living_world::integration::SqlCharacterEquipmentSyncRepository
        equipmentSyncRepository;
    living_world::integration::SqlCharacterProgressSnapshotRepository
        snapshotRepository;
    living_world::integration::SqlCharacterProgressSyncRepository syncRepository;
    living_world::service::AccountAltRecoveryService recoveryService;
    living_world::service::AccountAltStartupRecoveryService startupRecoveryService(
        runtimeRepository,
        itemSnapshotRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    living_world::service::AccountAltStartupRecoverySummary summary =
        startupRecoveryService.RecoverForAccount(
            player->GetSession()->GetAccountId());
    if (summary.scanned == 0)
    {
        return;
    }

    ChatHandler handler(player->GetSession());
    if (summary.recoveredSyncs > 0)
    {
        handler.PSendSysMessage(
            "LivingWorld recovered {} interrupted account-alt sync(s) on login.",
            summary.recoveredSyncs);
    }
    if (summary.pendingRecovery > 0)
    {
        handler.PSendSysMessage(
            "LivingWorld found {} account-alt runtime(s) that still need recovery before reuse.",
            summary.pendingRecovery);
    }
    if (summary.manualReviewRequired > 0 || summary.blocked > 0)
    {
        handler.PSendSysMessage(
            "LivingWorld found {} runtime(s) needing manual review and {} blocked runtime(s).",
            summary.manualReviewRequired,
            summary.blocked);
    }
}

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
        if (!player || !player->GetSession())
        {
            return;
        }

        if (!player->GetSession()->IsBotSession())
        {
            RunOwnerStartupRecovery(player);
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
