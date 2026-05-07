#include "AccountScript.h"
#include "Chat.h"
#include "Config.h"
#include "Duration.h"
#include "EventProcessor.h"
#include "ai/AmbientBotAI.h"
#include "ai/CompanionAI.h"
#include "integration/BotActivityLog.h"
#include "script/LivingWorldChatConfig.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlCharacterAchievementSyncRepository.h"
#include "integration/SqlCharacterBankSyncRepository.h"
#include "integration/SqlCharacterEquipmentSyncRepository.h"
#include "integration/SqlCharacterInventorySyncRepository.h"
#include "integration/SqlCharacterItemSnapshotRepository.h"
#include "integration/SqlCharacterNameLeaseRepository.h"
#include "integration/SqlCharacterProgressSnapshotRepository.h"
#include "integration/SqlCharacterProgressSyncRepository.h"
#include "integration/SqlCharacterQuestSyncRepository.h"
#include "integration/SqlCharacterReputationSyncRepository.h"
#include "integration/SqlCharacterSkillSyncRepository.h"
#include "integration/SqlCharacterSpellSyncRepository.h"
#include "service/AccountAltDismissalService.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltStartupRecoveryService.h"
#include "service/BotQuestRewardService.h"
#include "service/BotPlayerRegistry.h"
#include "service/BotTalentApplicator.h"
#include "integration/SqlBotTalentTemplateRepository.h"
#include "integration/SqlBotTalentPreferenceRepository.h"

#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QueryResult.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TradeData.h"
#include "WorldSession.h"
#include "Log.h"

#include <optional>
#include <unordered_set>
#include <vector>

// Economy scale global defined in LivingWorldWorldScript.cpp.
namespace living_world { extern float g_economyScale; extern void ApplyEconomyScale(float, bool); }

namespace
{
std::unordered_set<std::uint64_t> s_openedControlledTradeWindows;

// Returns true if the spell summons a mount (applies SPELL_AURA_MOUNTED).
bool IsMountSummonSpell(uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    return info && info->HasAura(SPELL_AURA_MOUNTED);
}

// Returns true for the riding-skill spells that gate mount speed tiers.
bool IsRidingSkillSpell(uint32 spellId)
{
    switch (spellId)
    {
        case 33388: // Apprentice Riding
        case 33391: // Journeyman Riding
        case 34090: // Expert Riding
        case 34091: // Artisan Riding
        case 54197: // Cold Weather Flying
            return true;
        default:
            return false;
    }
}

// Grants spellId to every character on accountId except learnedByGuid.
// Online characters receive it immediately via learnSpell; offline characters
// are updated directly in the DB so the knowledge persists at next login.
void PropagateSpellToAccountChars(
    uint32 accountId, ObjectGuid const& learnedByGuid, uint32 spellId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT guid FROM characters WHERE account = {}", accountId);
    if (!result)
        return;

    do
    {
        uint64 const guidLow = (*result)[0].Get<uint64>();
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(guidLow);
        if (guid == learnedByGuid)
            continue;

        if (Player* online = ObjectAccessor::FindPlayer(guid))
        {
            if (!online->HasSpell(spellId))
            {
                online->learnSpell(spellId, false);
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] AccountMount live: spellId={} -> '{}' guid={}",
                    spellId, online->GetName(), guidLow);
            }
        }
        else
        {
            CharacterDatabase.Execute(
                "INSERT IGNORE INTO character_spell (guid, spell, specMask) "
                "VALUES ({}, {}, 255)",
                guidLow, spellId);
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] AccountMount offline: spellId={} -> guid={}",
                spellId, guidLow);
        }
    } while (result->NextRow());
}

std::uint32_t CountQuestRows(std::uint64_t characterGuid, std::uint32_t questId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM character_queststatus WHERE guid = {} AND quest = {}",
        characterGuid,
        questId);
    if (!result)
        return 0;

    return result->Fetch()[0].Get<std::uint32_t>();
}

std::uint32_t CountActiveQuestRowsForQuest(std::uint64_t characterGuid, std::uint32_t questId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM character_queststatus WHERE guid = {} AND quest = {} AND status != 0",
        characterGuid,
        questId);
    if (!result)
        return 0;

    return result->Fetch()[0].Get<std::uint32_t>();
}

void SendLWBotAddonMessage(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    std::string msg = "LWBOT\t" + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(
        data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, msg);
    player->GetSession()->SendPacket(&data);
}

void SendQuestRewardsAddonState(Player* player)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    living_world::service::BotQuestRewardService questRewardService;
    SendLWBotAddonMessage(player, "QCLR");
    SendLWBotAddonMessage(
        player,
        std::string("QMODE;") +
            (questRewardService.GetRewardMode(player->GetGUID().GetCounter()) ==
                    living_world::service::BotQuestRewardMode::Manual
                ? "MANUAL"
                : "SMART"));

    for (living_world::service::PendingQuestReward const& pending :
         questRewardService.BuildPendingRewards(player))
    {
        std::string payload = "QST;";
        payload += pending.botName;
        payload += ';';
        payload += std::to_string(pending.questId);
        payload += ';';
        payload += pending.questTitle;

        for (living_world::service::QuestRewardChoice const& choice : pending.choices)
        {
            payload += ';';
            payload += std::to_string(choice.choiceNumber);
            payload += ':';
            payload += std::to_string(choice.itemId);
            payload += ':';
            payload += std::to_string(choice.count);
        }

        SendLWBotAddonMessage(player, payload);
    }

    SendLWBotAddonMessage(player, "QEND");
}

struct StartupRuntimeRecoverySummary
{
    std::uint32_t scanned = 0;
    std::uint32_t progressSynced = 0;
    std::uint32_t reputationSynced = 0;
    std::uint32_t questsSynced = 0;
    std::uint32_t achievementsSynced = 0;
    std::uint32_t spellsSynced = 0;
    std::uint32_t skillsSynced = 0;
    std::uint32_t equipmentSynced = 0;
    std::uint32_t inventorySynced = 0;
    std::uint32_t bankSynced = 0;
    std::uint32_t namesRestored = 0;
    std::uint32_t runtimesRetired = 0;
    std::uint32_t manualReviewRequired = 0;
    std::uint32_t blocked = 0;
};

StartupRuntimeRecoverySummary RecoverAccountAltRuntimesForAccount(
    std::uint32_t accountId)
{
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
    living_world::integration::SqlCharacterReputationSyncRepository
        reputationSyncRepository;
    living_world::integration::SqlCharacterQuestSyncRepository questSyncRepository;
    living_world::integration::SqlCharacterAchievementSyncRepository
        achievementSyncRepository;
    living_world::integration::SqlCharacterSpellSyncRepository spellSyncRepository;
    living_world::integration::SqlCharacterSkillSyncRepository skillSyncRepository;
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
        reputationSyncRepository,
        questSyncRepository,
        achievementSyncRepository,
        spellSyncRepository,
        skillSyncRepository,
        recoveryService,
        itemRecoveryOptions);

    StartupRuntimeRecoverySummary summary;
    for (living_world::model::AccountAltRuntimeRecord const& runtime :
         runtimeRepository.ListRecoverableForAccount(accountId))
    {
        ++summary.scanned;

        if (runtime.cloneCharacterGuid == 0)
        {
            ++summary.blocked;
            LOG_WARN(
                "server.worldserver",
                "[LivingWorldDebug] AccountLoginRecovery runtimeId={} sourceAccountId={} "
                "blocked: clone identity incomplete.",
                runtime.runtimeId,
                accountId);
            continue;
        }

        living_world::service::AccountAltDismissalSummary const result =
            dismissalService.DismissClone(runtime.cloneCharacterGuid);
        summary.progressSynced += result.progressSynced ? 1u : 0u;
        summary.reputationSynced += result.reputationSynced ? 1u : 0u;
        summary.questsSynced += result.questsSynced ? 1u : 0u;
        summary.achievementsSynced += result.achievementsSynced ? 1u : 0u;
        summary.spellsSynced += result.spellsSynced ? 1u : 0u;
        summary.skillsSynced += result.skillsSynced ? 1u : 0u;
        summary.equipmentSynced += result.equipmentSynced ? 1u : 0u;
        summary.inventorySynced += result.inventorySynced ? 1u : 0u;
        summary.bankSynced += result.bankSynced ? 1u : 0u;
        summary.namesRestored += result.namesRestored ? 1u : 0u;
        summary.runtimesRetired += result.runtimeRetired ? 1u : 0u;
        summary.manualReviewRequired += result.manualReviewRequired ? 1u : 0u;
        summary.blocked += result.blocked ? 1u : 0u;
    }

    return summary;
}

std::vector<ObjectGuid> CollectOfflineCloneGroupMembers(Player* player)
{
    if (!player || !player->GetSession())
    {
        return {};
    }

    if (!player->GetGroup())
    {
        return {};
    }

    QueryResult result = CharacterDatabase.Query(
        "SELECT clone_character_guid FROM living_world_account_alt_runtime "
        "WHERE source_account_id = {} AND clone_character_guid IS NOT NULL "
        "AND clone_character_guid != 0",
        player->GetSession()->GetAccountId());
    if (!result)
    {
        return {};
    }

    std::vector<ObjectGuid> toRemove;
    do
    {
        std::uint64_t cloneGuidLow = (*result)[0].Get<std::uint64_t>();
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(cloneGuidLow);
        Group* currentGroup = player->GetGroup();
        if (currentGroup && currentGroup->IsMember(guid) && !ObjectAccessor::FindPlayer(guid))
        {
            toRemove.push_back(guid);
        }
    } while (result->NextRow());

    return toRemove;
}

void CleanupStaleGroupBots(Player* player)
{
    std::vector<ObjectGuid> toRemove = CollectOfflineCloneGroupMembers(player);

    if (toRemove.empty())
    {
        return;
    }

    // NOTE:
    // Login-time group mutation here has produced rare use-after-free crashes
    // inside Group::RemoveMember/BroadcastGroupUpdate in debug builds.
    // Keep detection/logging for visibility, but avoid mutating group state
    // during this sensitive phase; stale slots are cleaned by normal group
    // lifecycle/logout flows.
    LOG_WARN(
        "server.worldserver",
        "[LivingWorldDebug] CleanupStaleGroupBots detected {} offline clone(s) "
        "for owner='{}' on login; skipping immediate removal for safety.",
        toRemove.size(),
        player->GetName());

    for (ObjectGuid const& guid : toRemove)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] CleanupStaleGroupBots deferred removal cloneGuid={} owner='{}'.",
            guid.GetCounter(),
            player->GetName());
    }
}

void RemoveOfflineCloneGroupMembers(Player* player)
{
    if (!player || !player->GetSession())
        return;

    Group* group = player->GetGroup();
    if (!group)
        return;

    std::vector<ObjectGuid> toRemove = CollectOfflineCloneGroupMembers(player);
    if (toRemove.empty())
        return;

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] DeferredGroupCleanup removing {} offline clone(s) for owner='{}'.",
        toRemove.size(),
        player->GetName());

    for (ObjectGuid const& guid : toRemove)
    {
        if (!group->IsMember(guid) || ObjectAccessor::FindPlayer(guid))
            continue;

        group->RemoveMember(guid, GROUP_REMOVEMETHOD_LEAVE);
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] DeferredGroupCleanup removed offline cloneGuid={} owner='{}'.",
            guid.GetCounter(),
            player->GetName());
    }
}

class DeferredOwnerGroupCleanupEvent final : public BasicEvent
{
public:
    explicit DeferredOwnerGroupCleanupEvent(ObjectGuid ownerGuid)
        : _ownerGuid(ownerGuid)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* owner = ObjectAccessor::FindConnectedPlayer(_ownerGuid);
        if (owner)
            RemoveOfflineCloneGroupMembers(owner);
        return true;
    }

private:
    ObjectGuid _ownerGuid;
};

void RunOwnerStartupRecovery(Player* player)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    StartupRuntimeRecoverySummary summary =
        RecoverAccountAltRuntimesForAccount(player->GetSession()->GetAccountId());

    if (summary.scanned == 0)
    {
        return;
    }

    ChatHandler handler(player->GetSession());
    if (summary.runtimesRetired > 0)
    {
        living_world::script::SendPlayerLog(
            &handler,
            static_cast<std::uint8_t>(
                living_world::script::PlayerChatLogLevel::BareMinimum),
            "LivingWorld recovered {} interrupted account-alt runtime(s) on login.",
            summary.runtimesRetired);
    }
    if (summary.namesRestored > 0)
    {
        living_world::script::SendPlayerLog(
            &handler,
            static_cast<std::uint8_t>(
                living_world::script::PlayerChatLogLevel::BareMinimum),
            "LivingWorld restored {} reserved name lease(s) on login.",
            summary.namesRestored);
    }
    if (summary.manualReviewRequired > 0 || summary.blocked > 0)
    {
        living_world::script::SendPlayerLog(
            &handler,
            static_cast<std::uint8_t>(
                living_world::script::PlayerChatLogLevel::BareMinimum),
            "LivingWorld found {} runtime(s) needing manual review and {} blocked runtime(s).",
            summary.manualReviewRequired,
            summary.blocked);
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] OwnerLoginRecovery character='{}' guid={} accountId={} "
        "scanned={} progress={} reputation={} quests={} achievements={} "
        "spells={} skills={} equipment={} inventory={} bank={} "
        "namesRestored={} retired={} manualReview={} blocked={}",
        player->GetName(),
        player->GetGUID().GetCounter(),
        player->GetSession()->GetAccountId(),
        summary.scanned,
        summary.progressSynced,
        summary.reputationSynced,
        summary.questsSynced,
        summary.achievementsSynced,
        summary.spellsSynced,
        summary.skillsSynced,
        summary.equipmentSynced,
        summary.inventorySynced,
        summary.bankSynced,
        summary.namesRestored,
        summary.runtimesRetired,
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

    bool isOwned = false;
    for (Player* b : living_world::service::BotPlayerRegistry::Instance()
                         .FindBotsForOwner(owner->GetGUID()))
    {
        if (b == bot) { isOwned = true; break; }
    }
    if (!isOwned)
        return nullptr;

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
    std::vector<Player*> bots = living_world::service::BotPlayerRegistry::Instance()
                                    .FindBotsForOwner(player->GetGUID());
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] DismissOwnerBot owner='{}' guid={} botCount={}",
        player ? player->GetName() : "<null>",
        player ? player->GetGUID().GetCounter() : 0,
        bots.size());

    for (Player* bot : bots)
    {
        if (!bot || !bot->GetSession())
            continue;

        living_world::ai::ClearBotOverride(bot->GetGUID());

        if (Group* group = bot->GetGroup())
            group->RemoveMember(bot->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);

        bool const alreadyLoggingOut = bot->GetSession()->PlayerLogout();
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] DismissOwnerBot bot='{}' guid={} alreadyLoggingOut={}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            alreadyLoggingOut);

        if (!alreadyLoggingOut)
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
    living_world::integration::SqlCharacterReputationSyncRepository
        reputationSyncRepository;
    living_world::integration::SqlCharacterQuestSyncRepository questSyncRepository;
    living_world::integration::SqlCharacterAchievementSyncRepository
        achievementSyncRepository;
    living_world::integration::SqlCharacterSpellSyncRepository spellSyncRepository;
    living_world::integration::SqlCharacterSkillSyncRepository skillSyncRepository;
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
        reputationSyncRepository,
        questSyncRepository,
        achievementSyncRepository,
        spellSyncRepository,
        skillSyncRepository,
        recoveryService,
        itemRecoveryOptions);

    living_world::service::AccountAltDismissalSummary summary =
        dismissalService.DismissClone(player->GetGUID().GetCounter());
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] BotLogoutRecovery character='{}' guid={} "
        "runtimeFound={} progress={} reputation={} quests={} achievements={} "
        "spells={} skills={} equipment={} inventory={} bank={} "
        "namesRestored={} runtimeRetired={} manualReview={} blocked={} reason='{}'",
        player->GetName(),
        player->GetGUID().GetCounter(),
        summary.runtimeFound,
        summary.progressSynced,
        summary.reputationSynced,
        summary.questsSynced,
        summary.achievementsSynced,
        summary.spellsSynced,
        summary.skillsSynced,
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
            CleanupStaleGroupBots(player);
            player->m_Events.AddEventAtOffset(
                new DeferredOwnerGroupCleanupEvent(player->GetGUID()),
                2s);
            return;
        }

        std::optional<ObjectGuid> ownerGuid =
            living_world::service::BotPlayerRegistry::Instance()
                .RegisterBotPlayer(player);
        if (!ownerGuid)
        {
            return;
        }

        // ownerGuid counter == 0 is the ownerless sentinel written by
        // SpawnHostileBotPlayerOnAccount (ObjectGuid::Empty as owner).
        // Check living_world_ambient_bot to determine which AI path to take:
        //   - ambient bot  → AmbientBotAI  (travel + activity, no combat)
        //   - hostile bot  → HostileCompanionAI (fight back, no follow)
        if (ownerGuid->GetCounter() == 0)
        {
            std::uint64_t const guid = player->GetGUID().GetCounter();
            QueryResult ambientRow = CharacterDatabase.Query(
                "SELECT 1 FROM living_world_ambient_bot WHERE character_guid = {}",
                guid);
            if (ambientRow)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] AmbientBotLogin bot='{}' guid={} — "
                    "scheduling ambient AI.",
                    player->GetName(), guid);
                living_world::integration::BotActivityLog::Record(
                    player, "spawned");
                living_world::ai::ScheduleAmbientBotAI(player);
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] HostileBotLogin bot='{}' guid={} — "
                    "scheduling hostile AI, no group join.",
                    player->GetName(), guid);
                living_world::ai::ScheduleHostileCompanionAI(player);
            }
            return;
        }

        // Copy all spells the owner has learned.
        // it knows exactly what the owner knows, no more. Use .lwbot train to
        // teach new spells at a class trainer (charges the owner gold).
        Player* owner = ObjectAccessor::FindPlayer(*ownerGuid);
        if (owner)
        {
            for (auto const& [spellId, playerSpell] : owner->GetSpellMap())
            {
                if (playerSpell
                    && playerSpell->State != PLAYERSPELL_REMOVED)
                {
                    player->learnSpell(spellId, false);
                }
            }
        }

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

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] BotOnLogout character='{}' guid={}",
            player->GetName(),
            player->GetGUID().GetCounter());

        if (Group* group = player->GetGroup())
            group->RemoveMember(player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);

        // Flush in-memory state (including quest log) to the DB before the
        // dismissal service reads character_queststatus for clone→source sync.
        // OnPlayerLogout fires before the normal SaveToDB in the logout path,
        // so we must save explicitly here.
        player->SaveToDB(false, false);

        RunBotDismissalRecovery(player);

        // Release the pool account so it can be reused for the next spawn.
        LoginDatabase.Execute(
            "UPDATE living_world_bot_account_pool "
            "SET is_available = 1, reserved_for = NULL WHERE account_id = {}",
            player->GetSession()->GetAccountId());

        // Release the raid pool character slot if this was a pool bot.
        // This is a no-op (0 rows updated) for AccountAlt clone bots.
        CharacterDatabase.Execute(
            "UPDATE living_world_pool_character SET is_available = 1 "
            "WHERE character_guid = {}",
            player->GetGUID().GetCounter());

        // Release ambient bot slot if this was an ambient bot.
        CharacterDatabase.Execute(
            "UPDATE living_world_ambient_bot "
            "SET is_available = 1, last_activity_at = NOW() "
            "WHERE character_guid = {}",
            player->GetGUID().GetCounter());

        living_world::service::BotPlayerRegistry::Instance()
            .UnregisterBotPlayer(player);
    }

    void SyncActiveQuestStateToSource(Player* bot, uint32 questId, bool removeQuest) const
    {
        if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
        {
            return;
        }

        living_world::integration::SqlAccountAltRuntimeRepository runtimeRepository;
        std::optional<living_world::model::AccountAltRuntimeRecord> runtime =
            runtimeRepository.FindByCloneCharacter(bot->GetGUID().GetCounter());
        if (!runtime)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] QuestSync live: no runtime for bot='{}' guid={} questId={}.",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                questId);
            return;
        }

        std::uint32_t const cloneRowsBeforeSave =
            CountQuestRows(runtime->cloneCharacterGuid, questId);
        std::uint32_t const sourceRowsBeforeSync =
            CountQuestRows(runtime->sourceCharacterGuid, questId);

        // Persist the clone's in-memory quest log first, then merge the quest
        // tables back into the parked/original source character immediately so
        // accept/abandon state survives even if later logout recovery misses.
        bot->SaveToDB(false, false);

        std::uint32_t const cloneRowsAfterSave =
            CountQuestRows(runtime->cloneCharacterGuid, questId);

        if (removeQuest)
        {
            CharacterDatabase.DirectExecute(
                "DELETE FROM character_queststatus WHERE guid = {} AND quest = {}",
                runtime->sourceCharacterGuid,
                questId);
        }

        living_world::integration::SqlCharacterQuestSyncRepository questSyncRepository;
        bool const synced = questSyncRepository.SyncQuestsFromCloneToSource(
            runtime->sourceCharacterGuid,
            runtime->cloneCharacterGuid);
        std::uint32_t const sourceRowsAfterSync =
            CountQuestRows(runtime->sourceCharacterGuid, questId);
        std::uint32_t const sourceActiveRowsAfterSync =
            CountActiveQuestRowsForQuest(runtime->sourceCharacterGuid, questId);
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] QuestSync live: bot='{}' cloneGuid={} sourceGuid={} "
            "questId={} removeQuest={} cloneRowsBeforeSave={} cloneRowsAfterSave={} "
            "sourceRowsBeforeSync={} sourceRowsAfterSync={} sourceActiveRowsAfterSync={} synced={}",
            bot->GetName(),
            runtime->cloneCharacterGuid,
            runtime->sourceCharacterGuid,
            questId,
            removeQuest,
            cloneRowsBeforeSave,
            cloneRowsAfterSave,
            sourceRowsBeforeSync,
            sourceRowsAfterSync,
            sourceActiveRowsAfterSync,
            synced);
    }

    void OnPlayerBeforeTrainerListSpellCost(Player* /*player*/, Creature* /*trainer*/,
        uint32 /*spellId*/, int32& moneyCost) override
    {
        float const scale = living_world::g_economyScale;
        if (scale > 0.0f && scale != 1.0f)
            moneyCost = std::max(0, int32(moneyCost * scale));
    }

    void OnPlayerBeforeTrainerTeachSpell(Player* /*player*/, Creature* /*trainer*/,
        uint32 /*spellId*/, int32& moneyCost) override
    {
        float const scale = living_world::g_economyScale;
        if (scale > 0.0f && scale != 1.0f)
            moneyCost = std::max(0, int32(moneyCost * scale));
    }

    void OnPlayerBeforeVendorItemPrice(Player* /*player*/, Creature* /*vendor*/,
        uint32 /*itemId*/, uint32& price) override
    {
        float const scale = living_world::g_economyScale;
        if (scale > 0.0f && scale != 1.0f)
            price = uint32(price * scale);
    }

    void OnPlayerLearnSpell(Player* player, uint32 spellID) override
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
            return;

        // Mount summons and riding skills are account-wide: every character on
        // this account learns the spell so no toon is left without a mount
        // their sibling earned.
        if (IsMountSummonSpell(spellID) || IsRidingSkillSpell(spellID))
        {
            PropagateSpellToAccountChars(
                player->GetSession()->GetAccountId(),
                player->GetGUID(),
                spellID);
        }

        // Mirror all learned spells (including the mount just learned) out to
        // any active bot clones that belong to this owner.
        for (Player* bot : living_world::service::BotPlayerRegistry::Instance()
                               .FindBotsForOwner(player->GetGUID()))
        {
            if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
                continue;
            if (bot->HasSpell(spellID))
                continue;
            bot->learnSpell(spellID, false);
            LOG_DEBUG(
                "server.worldserver",
                "[LivingWorldDebug] SpellSync learn: mirrored spellId={} from owner='{}' to bot='{}' guid={}.",
                spellID, player->GetName(), bot->GetName(), bot->GetGUID().GetCounter());
        }
    }

    void OnPlayerLearnTalents(Player* player, uint32 talentId, uint32 talentRank, uint32 spellId) override
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
            return;

        for (Player* bot : living_world::service::BotPlayerRegistry::Instance()
                               .FindBotsForOwner(player->GetGUID()))
        {
            if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
                continue;
            // learnSpell handles the talent passive — the talent's spellId is what
            // actually needs to exist on the bot for modifiers to apply.
            if (spellId && !bot->HasSpell(spellId))
            {
                bot->learnSpell(spellId, false);
                LOG_DEBUG(
                    "server.worldserver",
                    "[LivingWorldDebug] TalentSync: mirrored talentId={} rank={} spellId={} "
                    "from owner='{}' to bot='{}' guid={}.",
                    talentId, talentRank, spellId,
                    player->GetName(), bot->GetName(), bot->GetGUID().GetCounter());
            }
        }
    }

    void OnPlayerQuestAccept(Player* player, Quest const* quest) override
    {
        // Skip bots — only real owners trigger quest propagation.
        if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
            return;

        uint32 questId = quest->GetQuestId();
        std::vector<Player*> bots = living_world::service::BotPlayerRegistry::Instance()
                                        .FindBotsForOwner(player->GetGUID());
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] QuestSync accept entry owner='{}' guid={} questId={} botCount={}",
            player->GetName(),
            player->GetGUID().GetCounter(),
            questId,
            bots.size());

        for (Player* bot : bots)
        {
            if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
                continue;

            // Skip if bot already has this quest in any non-none state.
            if (bot->GetQuestStatus(questId) != QUEST_STATUS_NONE)
            {
                LOG_DEBUG(
                    "server.worldserver",
                    "[LivingWorldDebug] QuestSync accept: bot='{}' guid={} already has questId={}, skipping.",
                    bot->GetName(), bot->GetGUID().GetCounter(), questId);
                continue;
            }

            if (!bot->CanTakeQuest(quest, false) || !bot->CanAddQuest(quest, false))
            {
                LOG_DEBUG(
                    "server.worldserver",
                    "[LivingWorldDebug] QuestSync accept: bot='{}' guid={} ineligible for questId={}, skipping.",
                    bot->GetName(), bot->GetGUID().GetCounter(), questId);
                continue;
            }

            // Pass nullptr as giver — no per-giver hooks needed for sync.
            bot->AddQuestAndCheckCompletion(quest, nullptr);
            SyncActiveQuestStateToSource(bot, questId, false);

            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] QuestSync accept: pushed questId={} from owner='{}' to bot='{}' guid={}.",
                questId, player->GetName(), bot->GetName(), bot->GetGUID().GetCounter());
        }
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (!player || !quest || !player->GetSession())
        {
            return;
        }

        if (!player->GetSession()->IsBotSession())
        {
            living_world::service::BotQuestRewardService questRewardService;
            questRewardService.ApplyOwnerQuestRewardToBots(player, quest);
            SendQuestRewardsAddonState(player);

            std::uint32_t const pendingCount =
                static_cast<std::uint32_t>(
                    questRewardService.BuildPendingRewards(player).size());
            if (pendingCount > 0)
            {
                ChatHandler handler(player->GetSession());
                living_world::script::SendPlayerLog(
                    &handler,
                    static_cast<std::uint8_t>(living_world::script::PlayerChatLogLevel::BareMinimum),
                    "LivingWorld {} bot quest reward choice(s) waiting in the Quests tab.",
                    pendingCount);
            }

            return;
        }

        uint32 const questId = quest->GetQuestId();
        SyncActiveQuestStateToSource(player, questId, false);

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] QuestSync complete: bot='{}' guid={} questId={}.",
            player->GetName(),
            player->GetGUID().GetCounter(),
            questId);
    }

    void OnPlayerQuestAbandon(Player* player, uint32 questId) override
    {
        // Skip bots — abandons are mirrored from owner to bot, not bot to owner.
        if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
            return;

        for (Player* bot : living_world::service::BotPlayerRegistry::Instance()
                               .FindBotsForOwner(player->GetGUID()))
        {
            if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
                continue;

            QuestStatus status = bot->GetQuestStatus(questId);
            if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_REWARDED)
            {
                LOG_DEBUG(
                    "server.worldserver",
                    "[LivingWorldDebug] QuestSync abandon: bot='{}' guid={} does not have questId={}, skipping.",
                    bot->GetName(), bot->GetGUID().GetCounter(), questId);
                continue;
            }

            // RemoveActiveQuest clears quest log slot and status map entry.
            bot->AbandonQuest(questId);
            bot->RemoveActiveQuest(questId);
            SyncActiveQuestStateToSource(bot, questId, true);

            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] QuestSync abandon: removed questId={} from bot='{}' guid={} (owner='{}').",
                questId, bot->GetName(), bot->GetGUID().GetCounter(), player->GetName());
        }
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

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] OwnerBeforeLogout owner='{}' guid={}",
            player->GetName(),
            player->GetGUID().GetCounter());
        s_openedControlledTradeWindows.erase(player->GetGUID().GetCounter());
        DismissOwnerBot(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!player || !player->GetSession() || !player->GetSession()->IsBotSession())
            return;

        std::uint64_t const sourceCharGuid = player->GetGUID().GetCounter();

        living_world::integration::SqlBotTalentTemplateRepository templateRepo;
        living_world::integration::SqlBotTalentPreferenceRepository preferenceRepo;
        living_world::integration::SqlAccountAltRuntimeRepository altRuntimeRepo;

        std::optional<living_world::model::BotTalentPreference> pref =
            preferenceRepo.GetPreference(sourceCharGuid);
        if (pref && !pref->autoApplyOnLevel)
            return;

        living_world::service::BotTalentApplicator applicator(
            templateRepo, preferenceRepo, altRuntimeRepo);
        applicator.ApplyPreferredTemplate(player, sourceCharGuid);
    }
};

class LivingWorldAccountScript final : public AccountScript
{
public:
    LivingWorldAccountScript() : AccountScript("LivingWorldAccountScript") { }

    void OnAccountLogin(uint32 accountId) override
    {
        StartupRuntimeRecoverySummary const summary =
            RecoverAccountAltRuntimesForAccount(accountId);
        if (summary.scanned == 0)
            return;

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AccountLoginRecovery accountId={} scanned={} "
            "progress={} reputation={} quests={} achievements={} "
            "spells={} skills={} equipment={} inventory={} bank={} "
            "namesRestored={} retired={} manualReview={} blocked={}",
            accountId,
            summary.scanned,
            summary.progressSynced,
            summary.reputationSynced,
            summary.questsSynced,
            summary.achievementsSynced,
            summary.spellsSynced,
            summary.skillsSynced,
            summary.equipmentSynced,
            summary.inventorySynced,
            summary.bankSynced,
            summary.namesRestored,
            summary.runtimesRetired,
            summary.manualReviewRequired,
            summary.blocked);
    }
};

void AddSC_LivingWorldPlayerScript()
{
    new LivingWorldPlayerScript();
    new LivingWorldAccountScript();
}
