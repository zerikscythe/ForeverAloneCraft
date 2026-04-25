#include "Chat.h"
#include "Config.h"
#include "ai/CompanionAI.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlCharacterBankSyncRepository.h"
#include "integration/SqlCharacterEquipmentSyncRepository.h"
#include "integration/SqlCharacterInventorySyncRepository.h"
#include "integration/SqlCharacterItemSnapshotRepository.h"
#include "integration/SqlCharacterNameLeaseRepository.h"
#include "integration/SqlCharacterProgressSnapshotRepository.h"
#include "integration/SqlCharacterProgressSyncRepository.h"
#include "service/AccountAltDismissalService.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltStartupRecoveryService.h"
#include "service/BotPlayerRegistry.h"

#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TradeData.h"
#include "WorldSession.h"
#include "Log.h"

#include <optional>
#include <unordered_set>

namespace
{
std::unordered_set<std::uint64_t> s_openedControlledTradeWindows;

void RunOwnerStartupRecovery(Player* player)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    living_world::integration::SqlAccountAltRuntimeRepository runtimeRepository;
    living_world::integration::SqlCharacterItemSnapshotRepository
        itemSnapshotRepository;
    living_world::integration::SqlCharacterInventorySyncRepository
        inventorySyncRepository;
    living_world::integration::SqlCharacterBankSyncRepository
        bankSyncRepository;
    living_world::integration::SqlCharacterEquipmentSyncRepository
        equipmentSyncRepository;
    living_world::integration::SqlCharacterProgressSnapshotRepository
        snapshotRepository;
    living_world::integration::SqlCharacterProgressSyncRepository syncRepository;
    living_world::service::AccountAltRecoveryService recoveryService;
    living_world::service::AccountAltItemRecoveryOptions itemRecoveryOptions;
    itemRecoveryOptions.enableInventorySync =
        sConfigMgr->GetOption<bool>("LivingWorld.AccountAlt.EnableInventorySync", true);
    itemRecoveryOptions.enableBankSync =
        sConfigMgr->GetOption<bool>("LivingWorld.AccountAlt.EnableBankSync", true);
    living_world::service::AccountAltStartupRecoveryService startupRecoveryService(
        runtimeRepository,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService,
        itemRecoveryOptions);

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

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] OwnerLoginRecovery character='{}' guid={} accountId={} "
        "scanned={} recovered={} pending={} manualReview={} blocked={}",
        player->GetName(),
        player->GetGUID().GetCounter(),
        player->GetSession()->GetAccountId(),
        summary.scanned,
        summary.recoveredSyncs,
        summary.pendingRecovery,
        summary.manualReviewRequired,
        summary.blocked);
}

void AddBotToOwnerGroup(Player* bot, Player* owner)
{
    if (!bot || !owner)
    {
        LOG_WARN(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup skipped for null bot/owner "
            "bot={} owner={}",
            bot != nullptr,
            owner != nullptr);
        return;
    }

    if (bot->GetGroup() && bot->IsInSameGroupWith(owner))
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup bot guid={} owner guid={} "
            "already share a group; skipping duplicate add.",
            bot->GetGUID().GetCounter(),
            owner->GetGUID().GetCounter());
        return;
    }

    if (Group* botGroup = bot->GetGroup();
        botGroup && !bot->IsInSameGroupWith(owner))
    {
        LOG_WARN(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup bot guid={} has stale "
            "group guid={}; removing before owner attach.",
            bot->GetGUID().GetCounter(),
            botGroup->GetGUID().GetCounter());
        botGroup->RemoveMember(bot->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
    }

    Group* group = owner->GetGroup();
    if (!group)
    {
        group = new Group();
        if (!group->Create(owner))
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorldDebug] AddBotToOwnerGroup failed to create group "
                "for owner guid={}",
                owner->GetGUID().GetCounter());
            delete group;
            return;
        }
        sGroupMgr->AddGroup(group);
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup created group guid={} for "
            "owner guid={}",
            group->GetGUID().GetCounter(),
            owner->GetGUID().GetCounter());
    }

    if (group->IsFull())
    {
        LOG_WARN(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup group guid={} is full for "
            "owner guid={} bot guid={}",
            group->GetGUID().GetCounter(),
            owner->GetGUID().GetCounter(),
            bot->GetGUID().GetCounter());
        return;
    }

    for (Group::MemberSlot const& member : group->GetMemberSlots())
    {
        if (member.guid != bot->GetGUID())
            continue;

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup bot guid={} already exists "
            "in owner group guid={}; skipping duplicate slot.",
            bot->GetGUID().GetCounter(),
            group->GetGUID().GetCounter());
        return;
    }

    if (!group->AddMember(bot))
    {
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] AddBotToOwnerGroup failed AddMember for bot "
            "guid={} owner guid={} group guid={}",
            bot->GetGUID().GetCounter(),
            owner->GetGUID().GetCounter(),
            group->GetGUID().GetCounter());
        return;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] AddBotToOwnerGroup added bot guid={} ('{}') to "
        "owner guid={} ('{}') group guid={}",
        bot->GetGUID().GetCounter(),
        bot->GetName(),
        owner->GetGUID().GetCounter(),
        owner->GetName(),
        group->GetGUID().GetCounter());
}

Player* FindOwnerControlledTradeBot(Player* owner)
{
    if (!owner)
    {
        return nullptr;
    }

    TradeData* ownerTrade = owner->GetTradeData();
    if (!ownerTrade)
    {
        return nullptr;
    }

    Player* bot = ownerTrade->GetTrader();
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
    {
        return nullptr;
    }

    if (living_world::service::BotPlayerRegistry::Instance().FindBotForOwner(
            owner->GetGUID()) != bot)
    {
        return nullptr;
    }

    return bot;
}

void MaybeAutoAcceptControlledBotTrade(Player* owner)
{
    if (!owner || !owner->GetSession() || owner->GetSession()->IsBotSession())
    {
        return;
    }

    std::uint64_t const ownerGuid = owner->GetGUID().GetCounter();
    TradeData* ownerTrade = owner->GetTradeData();
    if (!ownerTrade)
    {
        s_openedControlledTradeWindows.erase(ownerGuid);
        return;
    }

    Player* bot = FindOwnerControlledTradeBot(owner);
    if (!bot || !bot->IsWithinDistInMap(owner, TRADE_DISTANCE, false))
    {
        return;
    }

    TradeData* botTrade = bot->GetTradeData();
    if (!botTrade || botTrade->GetTrader() != owner)
    {
        return;
    }

    if (!s_openedControlledTradeWindows.contains(ownerGuid))
    {
        WorldPacket packet;
        bot->GetSession()->HandleBeginTradeOpcode(packet);
        s_openedControlledTradeWindows.insert(ownerGuid);
        return;
    }

    if (!ownerTrade->IsAccepted() || ownerTrade->IsInAcceptProcess() ||
        botTrade->IsAccepted() || botTrade->IsInAcceptProcess())
    {
        return;
    }

    WorldPacket packet;
    bot->GetSession()->HandleAcceptTradeOpcode(packet);
}

void DismissOwnerBot(Player* player)
{
    Player* bot = living_world::service::BotPlayerRegistry::Instance()
                      .FindBotForOwner(player->GetGUID());
    if (!bot || !bot->GetSession())
    {
        return;
    }

    if (Group* group = bot->GetGroup())
    {
        group->RemoveMember(bot->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
    }

    if (!bot->GetSession()->PlayerLogout())
    {
        bot->GetSession()->LogoutPlayer(true);
    }
}

void RunBotDismissalRecovery(Player* player)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    living_world::integration::SqlAccountAltRuntimeRepository runtimeRepository;
    living_world::integration::SqlCharacterItemSnapshotRepository
        itemSnapshotRepository;
    living_world::integration::SqlCharacterInventorySyncRepository
        inventorySyncRepository;
    living_world::integration::SqlCharacterBankSyncRepository
        bankSyncRepository;
    living_world::integration::SqlCharacterEquipmentSyncRepository
        equipmentSyncRepository;
    living_world::integration::SqlCharacterNameLeaseRepository
        nameLeaseRepository;
    living_world::integration::SqlCharacterProgressSnapshotRepository
        snapshotRepository;
    living_world::integration::SqlCharacterProgressSyncRepository syncRepository;
    living_world::service::AccountAltRecoveryService recoveryService;
    living_world::service::AccountAltItemRecoveryOptions itemRecoveryOptions;
    itemRecoveryOptions.enableInventorySync =
        sConfigMgr->GetOption<bool>("LivingWorld.AccountAlt.EnableInventorySync", true);
    itemRecoveryOptions.enableBankSync =
        sConfigMgr->GetOption<bool>("LivingWorld.AccountAlt.EnableBankSync", true);
    living_world::service::AccountAltDismissalService dismissalService(
        runtimeRepository,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        nameLeaseRepository,
        snapshotRepository,
        syncRepository,
        recoveryService,
        itemRecoveryOptions);

    living_world::service::AccountAltDismissalSummary summary =
        dismissalService.DismissClone(player->GetGUID().GetCounter());
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] BotLogoutRecovery character='{}' guid={} "
        "runtimeFound={} progress={} equipment={} inventory={} bank={} "
        "namesRestored={} runtimeRetired={} manualReview={} blocked={} reason='{}'",
        player->GetName(),
        player->GetGUID().GetCounter(),
        summary.runtimeFound,
        summary.progressSynced,
        summary.equipmentSynced,
        summary.inventorySynced,
        summary.bankSynced,
        summary.namesRestored,
        summary.runtimeRetired,
        summary.manualReviewRequired,
        summary.blocked,
        summary.reason);
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
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] BotLoginAttach bot='{}' guid={} owner='{}' "
                "guid={}",
                player->GetName(),
                player->GetGUID().GetCounter(),
                owner->GetName(),
                owner->GetGUID().GetCounter());
            living_world::ai::ScheduleCompanionAI(player, owner);
            AddBotToOwnerGroup(player, owner);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player || !player->GetSession())
        {
            return;
        }

        s_openedControlledTradeWindows.erase(player->GetGUID().GetCounter());
        if (!player->GetSession()->IsBotSession())
            return;

        if (Group* group = player->GetGroup())
            group->RemoveMember(player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);

        RunBotDismissalRecovery(player);

        living_world::service::BotPlayerRegistry::Instance()
            .UnregisterBotPlayer(player);
    }

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        MaybeAutoAcceptControlledBotTrade(player);
    }

    void OnPlayerBeforeLogout(Player* player) override
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
        {
            return;
        }

        s_openedControlledTradeWindows.erase(player->GetGUID().GetCounter());
        DismissOwnerBot(player);
    }
};

void AddSC_LivingWorldPlayerScript()
{
    new LivingWorldPlayerScript();
}
