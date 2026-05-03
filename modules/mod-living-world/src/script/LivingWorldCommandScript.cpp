#include "script/LivingWorldCommandGrammar.h"
#include "script/LivingWorldChatConfig.h"
#include "ai/CompanionAI.h"
#include "Trainer.h"
#include "ObjectMgr.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Entities/Item/Container/Bag.h"
#include "Item.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Common.h"
#include "Random.h"
#include "SpellMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "integration/AzerothWorldFacade.h"
#include "integration/BotSessionFactory.h"
#include "integration/RosterRepository.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlBotAccountPoolRepository.h"
#include "integration/AzerothCharacterCloneStateGateway.h"
#include "integration/SqlCharacterAchievementSyncRepository.h"
#include "integration/SqlCharacterCloneMaterializer.h"
#include "integration/SqlCharacterBankSyncRepository.h"
#include "integration/SqlBotCombatProfileSelectionRepository.h"
#include "integration/SqlCharacterEquipmentSyncRepository.h"
#include "integration/SqlCharacterInventorySyncRepository.h"
#include "integration/SqlCharacterItemSnapshotRepository.h"
#include "integration/SqlCharacterProgressSnapshotRepository.h"
#include "integration/SqlCharacterProgressSyncRepository.h"
#include "integration/SqlCharacterQuestSyncRepository.h"
#include "integration/SqlCharacterReputationSyncRepository.h"
#include "integration/WorldCommitAction.h"
#include "model/AccountAltRuntime.h"
#include "model/PlayerRosterRequest.h"
#include "model/RosterEntry.h"
#include "planner/PlannerTypes.h"
#include "planner/SimplePartyRosterPlanner.h"
#include "service/BotPlayerRegistry.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltRuntimeCoordinator.h"
#include "service/BotQuestRewardService.h"
#include "service/PartyBotService.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace Acore::ChatCommands;

namespace living_world
{
namespace script
{
namespace
{
constexpr std::uint32_t MaleAltCompanionTemplateEntry = 111;
constexpr float AltCompanionFollowDistance = 2.5f;
constexpr float AltCompanionFollowAngle = 3.14159f;

constexpr std::array<std::string_view, 10> BotConfirmPhrases =
{
    "No problem.",
    "Easy enough.",
    "Sure thing.",
    "Got it.",
    "On it.",
    "You got it, boss.",
    "Consider it done.",
    "Right away.",
    "As you wish.",
    "Gotcha.",
};

constexpr std::array<std::string_view, 9> BotDenyPhrases =
{
    "Sorry...",
    "Sorry, boss.",
    "Nah.",
    "Maybe later.",
    "Do I have to?",
    "Not this time.",
    "Can't do it.",
    "I don't know that one.",
    "That's not gonna happen.",
};

void BotSayConfirm(Player* bot)
{
    if (!bot)
        return;
    std::uint32_t const idx = urand(0, static_cast<std::uint32_t>(BotConfirmPhrases.size() - 1));
    bot->Say(BotConfirmPhrases[idx], LANG_UNIVERSAL);
}

void BotSayDeny(Player* bot)
{
    if (!bot)
        return;
    std::uint32_t const idx = urand(0, static_cast<std::uint32_t>(BotDenyPhrases.size() - 1));
    bot->Say(BotDenyPhrases[idx], LANG_UNIVERSAL);
}

constexpr std::array<std::string_view, 2> BotTargetNotFoundPhrases =
{
    "Who!?",
    "I don't see them!",
};

void BotSayTargetNotFound(Player* bot)
{
    if (!bot)
        return;
    std::uint32_t const idx = urand(0, static_cast<std::uint32_t>(BotTargetNotFoundPhrases.size() - 1));
    bot->Say(BotTargetNotFoundPhrases[idx], LANG_UNIVERSAL);
}

void BotSayModeAssist(Player* bot)  { if (bot) bot->Say("I got your back.", LANG_UNIVERSAL); }
void BotSayModePassive(Player* bot) { if (bot) bot->Say("Staying out of it.", LANG_UNIVERSAL); }
void BotSayModeHold(Player* bot)    { if (bot) bot->Say("Not moving.", LANG_UNIVERSAL); }
void BotSayModeGuard(Player* bot)   { if (bot) bot->Say("Nobody touches you.", LANG_UNIVERSAL); }

struct CommitExecutionSummary
{
    std::uint32_t spawned = 0;
    std::uint32_t attachedToFollow = 0;
    std::uint32_t skipped = 0;
    std::uint32_t failed = 0;
};

std::unordered_map<std::uint64_t, ObjectGuid> SpawnedRosterBodies;

struct AccountAltSummary
{
    std::string name;
};

std::optional<model::RosterEntry> ResolveBotRosterEntry(
    std::uint32_t accountId,
    std::variant<std::uint32_t, std::string> const& botRef);

model::BotFaction ToBotFaction(std::uint8_t raceId)
{
    TeamId const teamId = Player::TeamIdForRace(raceId);
    if (teamId == TEAM_ALLIANCE)
    {
        return model::BotFaction::Alliance;
    }

    if (teamId == TEAM_HORDE)
    {
        return model::BotFaction::Horde;
    }

    return model::BotFaction::Neutral;
}

model::BotRole ToDefaultRole(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_PRIEST:
        case CLASS_DRUID:
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
            return model::BotRole::Support;
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            return model::BotRole::Tank;
        default:
            return model::BotRole::Damage;
    }
}

std::string_view ToClassName(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return "Warrior";
        case CLASS_PALADIN:
            return "Paladin";
        case CLASS_HUNTER:
            return "Hunter";
        case CLASS_ROGUE:
            return "Rogue";
        case CLASS_PRIEST:
            return "Priest";
        case CLASS_DEATH_KNIGHT:
            return "Death Knight";
        case CLASS_SHAMAN:
            return "Shaman";
        case CLASS_MAGE:
            return "Mage";
        case CLASS_WARLOCK:
            return "Warlock";
        case CLASS_DRUID:
            return "Druid";
        default:
            return "Unknown";
    }
}

std::string_view ToFailureText(
    planner::PartyRosterFailureReason reason)
{
    switch (reason)
    {
        case planner::PartyRosterFailureReason::None:
            return "none";
        case planner::PartyRosterFailureReason::RequesterUnavailable:
            return "requester unavailable";
        case planner::PartyRosterFailureReason::RosterEntryNotFound:
            return "roster entry not found";
        case planner::PartyRosterFailureReason::RosterEntryDisabled:
            return "roster entry disabled";
        case planner::PartyRosterFailureReason::RosterEntryAlreadySummoned:
            return "roster entry already online/summoned";
        case planner::PartyRosterFailureReason::PartyFull:
            return "party is full";
        case planner::PartyRosterFailureReason::OwnershipMismatch:
            return "roster entry belongs to another account";
        case planner::PartyRosterFailureReason::DirectControlNotSupported:
            return "direct control not supported";
    }

    return "unknown failure";
}

std::string_view ToParseErrorText(CommandParseErrorKind kind)
{
    switch (kind)
    {
        case CommandParseErrorKind::Empty:
            return "empty command";
        case CommandParseErrorKind::UnknownSubsystem:
            return "unknown subsystem";
        case CommandParseErrorKind::UnknownVerb:
            return "unknown verb";
        case CommandParseErrorKind::MissingArgument:
            return "missing argument";
        case CommandParseErrorKind::InvalidArgument:
            return "invalid argument";
    }

    return "parse error";
}

void RenderUsage(ChatHandler* handler)
{
    handler->PSendSysMessage("LivingWorld usage:");
    handler->PSendSysMessage("  .lw loglevel <1-4>");
    handler->PSendSysMessage("  .lwbot list");
    handler->PSendSysMessage("  .lwbot request <rosterEntryId>");
    handler->PSendSysMessage("  .lwbot dismiss <rosterEntryId>");
    handler->PSendSysMessage("  .lwbot roster list");
    handler->PSendSysMessage("  .lwbot roster request <rosterEntryId>");
    handler->PSendSysMessage("  .lwbot roster dismiss <rosterEntryId>");
    handler->PSendSysMessage("  .lwbot <#|name> profile <1-10>");
    handler->PSendSysMessage("  .lwbot <#|name> cast <Ability Name> [on yourself|me|mytarget|focus|<name>]");
    handler->PSendSysMessage("  .lwbot <#|name> attack [<name>]");
    handler->PSendSysMessage("  .lwbot <#|name> disengage");
    handler->PSendSysMessage("  .lwbot <#|name> retreat  (30s no-combat flee mode; repeat to cancel)");
    handler->PSendSysMessage("  .lwbot <#|name> train  (must be near a class trainer)");
    handler->PSendSysMessage("  .lwbot <#|name|party> follow");
    handler->PSendSysMessage("  .lwbot <#|name|party> yoink  (teleport stuck bots to you)");
    handler->PSendSysMessage("  .lwbot <#|name|party> refreshments  (eat/drink if HP or mana < 60%%)");
    handler->PSendSysMessage("  .lwbot <#|name|party> buff  (force re-apply class buffs)");
    handler->PSendSysMessage("  .lwbot <#|name> retrieve <itemGuid> [count]");
    handler->PSendSysMessage("  .lwbot <#|name> equip <itemGuid>");
    handler->PSendSysMessage("  .lwbot <#|name> unequip <itemGuid>");
    handler->PSendSysMessage("  .lwbot quests");
    handler->PSendSysMessage("  .lwbot questmode <smart|manual>");
    handler->PSendSysMessage("  .lwbot <#|name> reward <questId> <choiceNumber>");
    handler->PSendSysMessage("  .lwbot <#|name> mode assist|passive|hold|guard");
}

std::string_view TrimRootWhitespace(std::string_view input)
{
    while (!input.empty() &&
           std::isspace(static_cast<unsigned char>(input.front())))
    {
        input.remove_prefix(1);
    }
    while (!input.empty() &&
           std::isspace(static_cast<unsigned char>(input.back())))
    {
        input.remove_suffix(1);
    }
    return input;
}

std::string_view ToSourceText(model::RosterEntrySource source)
{
    switch (source)
    {
        case model::RosterEntrySource::GenericBot:
            return "generic";
        case model::RosterEntrySource::AccountAlt:
            return "account-alt";
    }

    return "unknown";
}

std::string_view ToActionText(integration::WorldCommitAction const& action)
{
    if (std::holds_alternative<integration::SpawnRosterBodyAction>(action))
    {
        return "spawn roster body";
    }

    if (std::holds_alternative<integration::AttachToPartyAction>(action))
    {
        return "attach to party";
    }

    if (std::holds_alternative<integration::UpdateAbstractStateAction>(action))
    {
        return "update abstract state";
    }

    return "unknown action";
}

std::string_view ToBotSpawnStatusText(
    integration::BotSessionSpawnStatus status)
{
    switch (status)
    {
        case integration::BotSessionSpawnStatus::SpawnQueued:
            return "spawn queued";
        case integration::BotSessionSpawnStatus::NoAvailableBotAccount:
            return "no available bot account";
        case integration::BotSessionSpawnStatus::BotAccountNotFound:
            return "bot account not found";
        case integration::BotSessionSpawnStatus::InvalidCharacterGuid:
            return "invalid character guid";
    }

    return "unknown bot spawn status";
}

std::string_view ToAccountAltSpawnDecisionText(
    service::AccountAltSpawnDecisionKind kind)
{
    switch (kind)
    {
        case service::AccountAltSpawnDecisionKind::SpawnUsingReservedAccount:
            return "spawn using reserved runtime account";
        case service::AccountAltSpawnDecisionKind::SpawnUsingPersistentClone:
            return "spawn using persistent clone";
        case service::AccountAltSpawnDecisionKind::RecoveryRequired:
            return "recovery required";
        case service::AccountAltSpawnDecisionKind::ManualReviewRequired:
            return "manual review required";
        case service::AccountAltSpawnDecisionKind::Blocked:
            return "blocked";
    }

    return "unknown runtime decision";
}

std::string_view ToAccountAltRuntimeStateText(
    model::AccountAltRuntimeState state)
{
    switch (state)
    {
        case model::AccountAltRuntimeState::PreparingClone:
            return "preparing clone";
        case model::AccountAltRuntimeState::Active:
            return "active";
        case model::AccountAltRuntimeState::SyncingBack:
            return "syncing back";
        case model::AccountAltRuntimeState::Recovering:
            return "recovering";
        case model::AccountAltRuntimeState::Failed:
            return "failed";
        case model::AccountAltRuntimeState::SyncingEquipment:
            return "syncing equipment";
        case model::AccountAltRuntimeState::SyncingInventory:
            return "syncing inventory";
        case model::AccountAltRuntimeState::SyncingBank:
            return "syncing bank";
    }

    return "unknown runtime state";
}

std::optional<AccountAltSummary> LoadAccountAltSummary(
    std::uint64_t characterGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name FROM characters WHERE guid = {} LIMIT 1",
        characterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    AccountAltSummary summary;
    summary.name = (*result)[0].Get<std::string>();
    return summary;
}

std::optional<model::AccountAltRuntimeRecord> LoadAccountAltRuntimeForSource(
    std::uint32_t accountId,
    std::uint64_t sourceCharacterGuid)
{
    integration::SqlAccountAltRuntimeRepository runtimeRepository;
    return runtimeRepository.FindBySourceCharacter(
        accountId,
        sourceCharacterGuid);
}

void RenderAccountAltRuntimeDebug(
    ChatHandler* handler,
    char const* label,
    std::optional<model::AccountAltRuntimeRecord> const& runtime)
{
    if (!handler)
    {
        return;
    }

    if (!runtime)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::Trace),
            "LivingWorld debug [{}]: no runtime record.",
            label);
        return;
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::Trace),
        "LivingWorld debug [{}]: runtime={} state={} ownerGuid={} "
        "sourceGuid={} cloneGuid={} cloneAccount={}.",
        label,
        runtime->runtimeId,
        ToAccountAltRuntimeStateText(runtime->state),
        runtime->ownerCharacterGuid,
        runtime->sourceCharacterGuid,
        runtime->cloneCharacterGuid,
        runtime->cloneAccountId);
    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::Trace),
        "LivingWorld debug [{}]: source='{}' parked='{}' clone='{}'.",
        label,
        runtime->sourceCharacterName,
        runtime->reservedSourceCharacterName,
        runtime->cloneCharacterName);
}

std::uint64_t ResolveActiveDismissGuid(
    std::uint32_t accountId,
    model::RosterEntry const& entry,
    std::optional<model::AccountAltRuntimeRecord> const& runtime = std::nullopt)
{
    if (entry.source != model::RosterEntrySource::AccountAlt)
    {
        return entry.characterGuid;
    }

    if (runtime && runtime->cloneCharacterGuid != 0)
    {
        return runtime->cloneCharacterGuid;
    }

    std::optional<model::AccountAltRuntimeRecord> loadedRuntime =
        LoadAccountAltRuntimeForSource(accountId, entry.characterGuid);
    if (!loadedRuntime || loadedRuntime->cloneCharacterGuid == 0)
    {
        return entry.characterGuid;
    }

    return loadedRuntime->cloneCharacterGuid;
}

Creature* FindActiveRosterBody(
    Player* player,
    std::uint64_t rosterEntryId)
{
    auto const itr = SpawnedRosterBodies.find(rosterEntryId);
    if (itr == SpawnedRosterBodies.end())
    {
        return nullptr;
    }

    Creature* body = player->GetMap()->GetCreature(itr->second);
    if (!body || !body->IsAlive())
    {
        SpawnedRosterBodies.erase(itr);
        return nullptr;
    }

    return body;
}

bool ExecuteSpawnRosterBodyAction(
    ChatHandler* handler,
    Player* requester,
    integration::SpawnRosterBodyAction const& action)
{
    if (action.source != model::RosterEntrySource::AccountAlt)
    {
        if (Creature* existing = FindActiveRosterBody(requester, action.rosterEntryId))
        {
            existing->GetMotionMaster()->MoveFollow(
                requester,
                AltCompanionFollowDistance,
                AltCompanionFollowAngle);
            handler->PSendSysMessage(
                "LivingWorld generic roster entry {} is already spawned and following.",
                action.rosterEntryId);
            return true;
        }

        std::uint32_t const templateEntry = MaleAltCompanionTemplateEntry;
        Position const spawnPosition = requester->GetNearPosition(2.0f, 0.0f);
        TempSummon* summon = requester->GetMap()->SummonCreature(
            templateEntry,
            spawnPosition,
            nullptr,
            0,
            requester);
        if (!summon)
        {
            handler->PSendSysMessage(
                "LivingWorld failed to summon generic roster entry {}.",
                action.rosterEntryId);
            return false;
        }

        summon->SetFaction(requester->GetFaction());
        summon->SetReactState(REACT_PASSIVE);
        summon->SetFullHealth();
        summon->GetMotionMaster()->MoveFollow(
            requester,
            AltCompanionFollowDistance,
            AltCompanionFollowAngle);

        SpawnedRosterBodies[action.rosterEntryId] = summon->GetGUID();
        handler->PSendSysMessage(
            "LivingWorld spawned generic roster entry {} with the temporary fallback.",
            action.rosterEntryId);
        return true;
    }

    // Check if THIS specific bot (by characterGuid) is already active.
    ObjectGuid const requestedGuid =
        ObjectGuid::Create<HighGuid::Player>(action.characterGuid);
    Player* existingBot =
        service::BotPlayerRegistry::Instance().FindBotForOwnerByGuid(
            requester->GetGUID(), requestedGuid);
    if (existingBot)
    {
        if (!existingBot->GetSession() || !existingBot->GetSession()->IsBotSession())
        {
            LOG_WARN(
                "server.worldserver",
                "[LivingWorld] Spawn continuity: stale registry for owner guid={} "
                "bot guid={} has invalid session — clearing and re-spawning.",
                requester->GetGUID().GetCounter(),
                existingBot->GetGUID().GetCounter());
            service::BotPlayerRegistry::Instance().UnregisterBotPlayer(existingBot);
            // fall through to PlanSpawn
        }
        else
        {
            if (!existingBot->IsInSameGroupWith(requester))
            {
                LOG_WARN(
                    "server.worldserver",
                    "[LivingWorld] Spawn continuity: bot guid={} live but absent "
                    "from owner guid={} group — re-adding.",
                    existingBot->GetGUID().GetCounter(),
                    requester->GetGUID().GetCounter());
                Group* group = requester->GetGroup();
                if (!group)
                {
                    group = new Group();
                    if (group->Create(requester))
                        sGroupMgr->AddGroup(group);
                    else
                    {
                        delete group;
                        group = nullptr;
                    }
                }
                if (group && !group->IsFull())
                    group->AddMember(existingBot);
            }
            existingBot->GetMotionMaster()->MoveFollow(
                requester,
                AltCompanionFollowDistance,
                AltCompanionFollowAngle);
            SendPlayerLog(
                handler,
                static_cast<std::uint8_t>(PlayerChatLogLevel::Detailed),
                "LivingWorld roster entry {} is already active as clone '{}' "
                "(guid {}).",
                action.rosterEntryId,
                existingBot->GetName(),
                existingBot->GetGUID().GetCounter());
            return true;
        }
    }

    ObjectGuid const requestedBotGuid =
        ObjectGuid::Create<HighGuid::Player>(action.characterGuid);
    if (service::BotPlayerRegistry::Instance().IsPendingBotForOwner(
            requester->GetGUID(),
            requestedBotGuid))
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld roster entry {} is already logging in; please wait.",
            action.rosterEntryId);
        return true;
    }

    integration::SqlAccountAltRuntimeRepository runtimeRepository;
    integration::SqlBotAccountPoolRepository botAccountPoolRepository;
    integration::SqlCharacterCloneMaterializer cloneMaterializer;
    integration::AzerothCharacterCloneStateGateway cloneStateGateway;
    integration::SqlCharacterItemSnapshotRepository itemSnapshotRepository;
    integration::SqlCharacterInventorySyncRepository inventorySyncRepository;
    integration::SqlCharacterBankSyncRepository bankSyncRepository;
    integration::SqlCharacterEquipmentSyncRepository equipmentSyncRepository;
    integration::SqlCharacterProgressSnapshotRepository snapshotRepository;
    integration::SqlCharacterProgressSyncRepository syncRepository;
    integration::SqlCharacterReputationSyncRepository reputationSyncRepository;
    integration::SqlCharacterQuestSyncRepository questSyncRepository;
    integration::SqlCharacterAchievementSyncRepository achievementSyncRepository;
    service::AccountAltRecoveryService recoveryService;
    service::AccountAltItemRecoveryOptions itemRecoveryOptions;
    itemRecoveryOptions.enableInventorySync =
        sConfigMgr->GetOption<bool>("LivingWorld.AccountAlt.EnableInventorySync", true);
    itemRecoveryOptions.enableBankSync =
        sConfigMgr->GetOption<bool>("LivingWorld.AccountAlt.EnableBankSync", true);
    service::AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        cloneStateGateway,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        reputationSyncRepository,
        questSyncRepository,
        achievementSyncRepository,
        recoveryService,
        itemRecoveryOptions);

    std::optional<AccountAltSummary> summary =
        LoadAccountAltSummary(action.characterGuid);
    std::string sourceCharacterName =
        summary ? summary->name : std::string("unknown");

    service::AccountAltSpawnDecision spawnDecision = coordinator.PlanSpawn(
        requester->GetSession()->GetAccountId(),
        action.characterGuid,
        requester->GetGUID().GetCounter(),
        sourceCharacterName);

    if (spawnDecision.kind !=
        service::AccountAltSpawnDecisionKind::SpawnUsingReservedAccount &&
        spawnDecision.kind !=
        service::AccountAltSpawnDecisionKind::SpawnUsingPersistentClone)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld blocked account-alt spawn for entry {}: {} ({})",
            action.rosterEntryId,
            ToAccountAltSpawnDecisionText(spawnDecision.kind),
            spawnDecision.reason);
        return false;
    }

    RenderAccountAltRuntimeDebug(
        handler,
        "request post-plan",
        spawnDecision.runtime);

    integration::BotSessionSpawnResult const spawnResult =
        integration::BotSessionFactory::SpawnBotPlayerOnAccount(
            spawnDecision.botAccountId,
            ObjectGuid::Create<HighGuid::Player>(
                spawnDecision.spawnCharacterGuid),
            requester->GetGUID());
    if (spawnResult.status != integration::BotSessionSpawnStatus::SpawnQueued)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld failed to queue account-alt bot login for entry {}: {}.",
            action.rosterEntryId,
            ToBotSpawnStatusText(spawnResult.status));
        return false;
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld queued account-alt bot login for entry {} using bot "
        "account {} ({}).",
        action.rosterEntryId,
        spawnResult.botAccountId,
        ToAccountAltSpawnDecisionText(spawnDecision.kind));
    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::Trace),
        "LivingWorld debug [request queue]: sourceGuid={} cloneGuid={} "
        "ownerGuid={} source='{}'.",
        action.characterGuid,
        spawnDecision.spawnCharacterGuid,
        requester->GetGUID().GetCounter(),
        sourceCharacterName);
    return true;
}

bool ExecuteAttachToPartyAction(
    ChatHandler* handler,
    Player* requester,
    integration::AttachToPartyAction const& action)
{
    Creature* body = FindActiveRosterBody(requester, action.rosterEntryId);
    if (!body)
    {
        if (Player* bot =
            service::BotPlayerRegistry::Instance().FindBotForOwner(
                requester->GetGUID()))
        {
            bot->GetMotionMaster()->MoveFollow(
                requester,
                AltCompanionFollowDistance,
                AltCompanionFollowAngle);
            SendPlayerLog(
                handler,
                static_cast<std::uint8_t>(PlayerChatLogLevel::Detailed),
                "LivingWorld attached roster entry {} as bot-player '{}' "
                "(guid {}).",
                action.rosterEntryId,
                bot->GetName(),
                bot->GetGUID().GetCounter());
            return true;
        }

        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld queued roster entry {}; bot login is still pending.",
            action.rosterEntryId);
        return true;
    }

    body->GetMotionMaster()->MoveFollow(
        requester,
        AltCompanionFollowDistance,
        AltCompanionFollowAngle);
    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::Detailed),
        "LivingWorld attached roster entry {} as a follower. Real party membership is not wired yet.",
        action.rosterEntryId);
    return true;
}

CommitExecutionSummary ExecuteCommitActions(
    ChatHandler* handler,
    Player* requester,
    std::vector<integration::WorldCommitAction> const& actions)
{
    CommitExecutionSummary summary;
    for (integration::WorldCommitAction const& action : actions)
    {
        if (integration::SpawnRosterBodyAction const* spawn =
            std::get_if<integration::SpawnRosterBodyAction>(&action))
        {
            if (ExecuteSpawnRosterBodyAction(handler, requester, *spawn))
            {
                ++summary.spawned;
            }
            else
            {
                ++summary.failed;
            }
            continue;
        }

        if (integration::AttachToPartyAction const* attach =
            std::get_if<integration::AttachToPartyAction>(&action))
        {
            if (ExecuteAttachToPartyAction(handler, requester, *attach))
            {
                ++summary.attachedToFollow;
            }
            else
            {
                ++summary.failed;
            }
            continue;
        }

        ++summary.skipped;
    }

    return summary;
}

model::RosterEntry BuildRosterEntry(
    Field const* fields,
    std::uint32_t ownerAccountId)
{
    std::uint64_t const guid = fields[0].Get<std::uint64_t>();
    std::string const name = fields[1].Get<std::string>();
    std::uint8_t const raceId = fields[2].Get<std::uint8_t>();
    std::uint8_t const classId = fields[3].Get<std::uint8_t>();
    std::uint8_t const level = fields[4].Get<std::uint8_t>();

    model::RosterEntry entry;
    entry.rosterEntryId = guid;
    entry.source = model::RosterEntrySource::AccountAlt;
    entry.ownerAccountId = ownerAccountId;
    entry.characterGuid = guid;
    entry.isEnabled = true;
    entry.isAlreadySummoned =
        ObjectAccessor::FindPlayerByLowGUID(static_cast<ObjectGuid::LowType>(guid)) != nullptr;

    entry.controllableProfile.profile.botId = guid;
    entry.controllableProfile.profile.name = name;
    entry.controllableProfile.profile.raceId = raceId;
    entry.controllableProfile.profile.classId = classId;
    entry.controllableProfile.profile.faction = ToBotFaction(raceId);
    entry.controllableProfile.profile.level = level;
    entry.controllableProfile.profile.guildName = "Account Alts";
    entry.controllableProfile.profile.personality =
        model::BotPersonality::Indifferent;
    entry.controllableProfile.profile.preferredRole = ToDefaultRole(classId);
    entry.controllableProfile.canBePlayerControlled = false;
    entry.controllableProfile.canEarnProgression = true;
    entry.controllableProfile.canJoinPlayerParty = true;
    return entry;
}

class AccountAltRosterRepository final : public integration::RosterRepository
{
public:
    std::vector<model::RosterEntry> GetRosterEntriesForAccount(
        std::uint32_t accountId) const override
    {
        std::vector<model::RosterEntry> entries;
        // COALESCE: when the source alt is parked its characters.name is a
        // garbage placeholder — use sourceCharacterName from the runtime row
        // (the original visible name) so that name-based lookups keep working.
        QueryResult result = CharacterDatabase.Query(
            "SELECT c.guid, COALESCE(r.source_character_name, c.name) AS name, "
            "c.race, c.class, c.level "
            "FROM characters c "
            "LEFT JOIN living_world_account_alt_runtime r "
            "  ON r.source_character_guid = c.guid AND r.source_account_id = {} "
            "WHERE c.account = {} ORDER BY c.guid ASC",
            accountId,
            accountId);

        if (!result)
        {
            return entries;
        }

        do
        {
            entries.push_back(BuildRosterEntry(result->Fetch(), accountId));
        } while (result->NextRow());

        return entries;
    }

    std::optional<model::RosterEntry> FindRosterEntryForAccount(
        std::uint32_t accountId,
        std::uint64_t rosterEntryId) const override
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT c.guid, COALESCE(r.source_character_name, c.name) AS name, "
            "c.race, c.class, c.level "
            "FROM characters c "
            "LEFT JOIN living_world_account_alt_runtime r "
            "  ON r.source_character_guid = c.guid AND r.source_account_id = {} "
            "WHERE c.account = {} AND c.guid = {} LIMIT 1",
            accountId,
            accountId,
            rosterEntryId);

        if (!result)
        {
            return std::nullopt;
        }

        return BuildRosterEntry(result->Fetch(), accountId);
    }
};

class LiveAzerothWorldFacade final : public integration::AzerothWorldFacade
{
public:
    std::optional<integration::PlayerWorldContext> GetPlayerContext(
        std::uint64_t characterGuid) const override
    {
        Player* player = ObjectAccessor::FindPlayerByLowGUID(
            static_cast<ObjectGuid::LowType>(characterGuid));
        if (!player || !player->GetSession())
        {
            return std::nullopt;
        }

        integration::PlayerWorldContext context;
        context.identity.characterGuid = player->GetGUID().GetCounter();
        context.identity.accountId = player->GetSession()->GetAccountId();
        context.position.mapId = player->GetMapId();
        context.position.zoneId = player->GetZoneId();
        context.position.areaId = player->GetAreaId();
        context.position.x = player->GetPositionX();
        context.position.y = player->GetPositionY();
        context.position.z = player->GetPositionZ();
        context.position.orientation = player->GetOrientation();
        context.isInWorld = player->IsInWorld();
        context.isInCombat = player->IsInCombat();
        context.isDead = player->isDead();
        context.canControlCompanions = !context.isInCombat;

        if (Group const* group = player->GetGroup())
        {
            context.party.maxPartyMembers = group->isRaidGroup()
                ? MAXRAIDSIZE
                : MAXGROUPSIZE;

            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                integration::PartyMemberSnapshot snapshot;
                snapshot.characterGuid = member.guid.GetCounter();
                snapshot.isOnline =
                    ObjectAccessor::FindPlayer(member.guid) != nullptr;
                snapshot.isBotControlled = false;
                context.party.members.push_back(snapshot);
            }
        }
        else
        {
            context.party.maxPartyMembers = MAXGROUPSIZE;
        }

        return context;
    }

    std::vector<integration::SpawnAnchor> GetSpawnAnchorsInZone(
        std::uint32_t) const override
    {
        return {};
    }

    bool IsCharacterOnline(std::uint64_t characterGuid) const override
    {
        return ObjectAccessor::FindPlayerByLowGUID(
            static_cast<ObjectGuid::LowType>(characterGuid)) != nullptr;
    }
};

std::vector<model::RosterEntry> BuildVisibleRosterEntries(
    std::uint32_t accountId)
{
    AccountAltRosterRepository repository;
    return repository.GetRosterEntriesForAccount(accountId);
}

void RenderRosterList(ChatHandler* handler)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld roster commands require an in-game session.");
        return;
    }

    std::vector<model::RosterEntry> entries =
        BuildVisibleRosterEntries(session->GetAccountId());

    if (entries.empty())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld roster: no account characters found.");
        return;
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld roster entries:");
    std::uint32_t position = 1;
    for (model::RosterEntry const& entry : entries)
    {
        model::BotProfile const& profile = entry.controllableProfile.profile;
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "  [{}] {} lvl {} {} ({}){}",
            position++,
            profile.name,
            static_cast<std::uint32_t>(profile.level),
            ToClassName(profile.classId),
            ToSourceText(entry.source),
            entry.isAlreadySummoned ? " online" : "");
    }
}

void RenderRosterRequest(
    ChatHandler* handler,
    RosterRequestCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld roster requests require an in-game player.");
        return;
    }

    std::optional<model::RosterEntry> entry =
        ResolveBotRosterEntry(session->GetAccountId(), command.botRef);
    if (!entry)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bot not found in roster.");
        return;
    }

    if (entry->characterGuid == player->GetGUID().GetCounter())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld cannot spawn the character you are currently logged in on.");
        return;
    }

    if (entry->source == model::RosterEntrySource::AccountAlt)
    {
        RenderAccountAltRuntimeDebug(
            handler,
            "request pre-plan",
            LoadAccountAltRuntimeForSource(
                session->GetAccountId(),
                entry->characterGuid));
    }

    LiveAzerothWorldFacade facade;
    AccountAltRosterRepository repository;
    planner::SimplePartyRosterPlanner planner;
    service::PartyBotService service(facade, repository, planner);

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = player->GetGUID().GetCounter();
    request.requesterAccountId = session->GetAccountId();
    request.requestedRosterEntryId = entry->rosterEntryId;

    service::PartyBotDispatchResult result =
        service.DispatchRosterRequest(request);

    if (!result.isApproved)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld roster request rejected: {}.",
            ToFailureText(result.failureReason));
        return;
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld roster request approved for {}.",
        entry->controllableProfile.profile.name);
    CommitExecutionSummary const execution = ExecuteCommitActions(
        handler,
        player,
        result.commitActions);
    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld commit result: spawned {}, attached {}, skipped {}, failed {}.",
        execution.spawned,
        execution.attachedToFollow,
        execution.skipped,
        execution.failed);
}

void RenderDismissBot(
    ChatHandler* handler,
    RosterDismissCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld dismiss requires an in-game player.");
        return;
    }

    std::optional<model::RosterEntry> entry =
        ResolveBotRosterEntry(session->GetAccountId(), command.botRef);
    if (!entry)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bot not found in roster.");
        return;
    }

    if (entry->characterGuid == player->GetGUID().GetCounter())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld cannot dismiss the character you are currently logged in on.");
        return;
    }

    std::optional<model::AccountAltRuntimeRecord> runtime;
    if (entry->source == model::RosterEntrySource::AccountAlt)
    {
        runtime = LoadAccountAltRuntimeForSource(
            session->GetAccountId(),
            entry->characterGuid);
        RenderAccountAltRuntimeDebug(handler, "dismiss lookup", runtime);
    }

    std::uint64_t const activeCharacterGuid = ResolveActiveDismissGuid(
        session->GetAccountId(),
        *entry,
        runtime);
    ObjectGuid const activeGuid =
        ObjectGuid::Create<HighGuid::Player>(activeCharacterGuid);
    Player* bot = service::BotPlayerRegistry::Instance().FindBotForOwnerByGuid(
        player->GetGUID(), activeGuid);
    if (!bot)
    {
        if (runtime && runtime->cloneCharacterGuid != 0)
        {
            SendPlayerLog(
                handler,
                static_cast<std::uint8_t>(PlayerChatLogLevel::Detailed),
                "LivingWorld no active bot found for {}. Runtime expects clone "
                "guid {} on bot account {}.",
                entry->controllableProfile.profile.name,
                runtime->cloneCharacterGuid,
                runtime->cloneAccountId);
        }
        else
        {
            SendPlayerLog(
                handler,
                static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
                "LivingWorld no active bot found for {}.",
                entry->controllableProfile.profile.name);
        }
        return;
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::Trace),
        "LivingWorld debug [dismiss]: source '{}' guid {} maps to active clone "
        "'{}' guid {}.",
        entry->controllableProfile.profile.name,
        entry->characterGuid,
        bot->GetName(),
        bot->GetGUID().GetCounter());

    if (Group* group = bot->GetGroup())
        group->RemoveMember(bot->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);

    if (!bot->GetSession()->PlayerLogout())
        bot->GetSession()->LogoutPlayer(true);
    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld dismissed {}.",
        entry->controllableProfile.profile.name);
}

std::optional<model::RosterEntry> ResolveBotRosterEntry(
    std::uint32_t accountId,
    std::variant<std::uint32_t, std::string> const& botRef)
{
    std::vector<model::RosterEntry> entries =
        BuildVisibleRosterEntries(accountId);

    if (std::uint32_t const* position = std::get_if<std::uint32_t>(&botRef))
    {
        if (*position == 0 || *position > static_cast<std::uint32_t>(entries.size()))
            return std::nullopt;
        return entries[*position - 1];
    }

    std::string const& name = std::get<std::string>(botRef);
    for (model::RosterEntry const& entry : entries)
        if (entry.controllableProfile.profile.name == name)
            return entry;

    return std::nullopt;
}

// Returns true when botRef is the "party" broadcast token.
bool IsPartyBotRef(std::variant<std::uint32_t, std::string> const& botRef)
{
    std::string const* name = std::get_if<std::string>(&botRef);
    return name && *name == "Party";
}

// Resolve ALL active bot Player*s for the owner (used by party broadcast).
std::vector<Player*> ResolvePartyBotsForOwner(Player* owner)
{
    if (!owner)
        return {};
    return service::BotPlayerRegistry::Instance().FindBotsForOwner(owner->GetGUID());
}

// Resolve the active bot Player* for a given botRef (index or name).
// Returns null if the bot is not in the roster or not currently online.
Player* ResolveActiveBotForOwner(
    Player* owner,
    std::variant<std::uint32_t, std::string> const& botRef)
{
    if (!owner)
        return nullptr;

    std::optional<model::RosterEntry> entry =
        ResolveBotRosterEntry(owner->GetSession()->GetAccountId(), botRef);
    if (!entry)
        return nullptr;

    // Resolve the active character guid (may be a clone for AccountAlt).
    std::uint64_t activeGuidLow = entry->characterGuid;
    if (entry->source == model::RosterEntrySource::AccountAlt)
    {
        std::optional<model::AccountAltRuntimeRecord> runtime =
            LoadAccountAltRuntimeForSource(
                owner->GetSession()->GetAccountId(),
                entry->characterGuid);
        if (runtime && runtime->cloneCharacterGuid != 0)
            activeGuidLow = runtime->cloneCharacterGuid;
    }

    ObjectGuid const activeGuid =
        ObjectGuid::Create<HighGuid::Player>(activeGuidLow);
    return service::BotPlayerRegistry::Instance().FindBotForOwnerByGuid(
        owner->GetGUID(), activeGuid);
}

std::vector<Player*> ResolveSelectedBotsForOwner(
    Player* owner,
    std::variant<std::uint32_t, std::string> const& botRef)
{
    if (IsPartyBotRef(botRef))
        return ResolvePartyBotsForOwner(owner);

    if (Player* bot = ResolveActiveBotForOwner(owner, botRef))
        return { bot };

    return {};
}

bool IsAnyOwnerBotRetreating(ObjectGuid const& ownerGuid)
{
    for (Player* bot : service::BotPlayerRegistry::Instance().FindBotsForOwner(ownerGuid))
    {
        if (bot && living_world::ai::IsBotRetreating(bot->GetGUID()))
            return true;
    }

    return false;
}

// Find the highest-rank spell by name that the bot actually knows.
// SpellInfo::SpellName[LOCALE_enUS] holds the DBC display name.
std::uint32_t ResolveSpellByName(Player const* bot, std::string const& name)
{
    std::uint32_t bestSpellId = 0;
    std::uint8_t  bestRank    = 0;

    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (!playerSpell || playerSpell->State == PLAYERSPELL_REMOVED)
            continue;

        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info || !info->SpellName[LOCALE_enUS])
            continue;

        if (!StringEqualI(info->SpellName[LOCALE_enUS], name))
            continue;

        std::uint8_t const rank = info->GetRank();
        if (rank > bestRank)
        {
            bestRank    = rank;
            bestSpellId = spellId;
        }
    }

    return bestSpellId;
}

void HandleBotModeSet(
    ChatHandler* handler,
    BotModeSetCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot mode commands require an in-game player.");
        return;
    }

    Player* bot = service::BotPlayerRegistry::Instance().FindBotForOwner(
        player->GetGUID());
    if (!bot)
    {
        handler->PSendSysMessage(
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    service::BotPlayerRegistry::Instance().SetBotMode(player->GetGUID(), command.mode);

    switch (command.mode)
    {
        case model::BotCombatMode::Assist:  BotSayModeAssist(bot);  break;
        case model::BotCombatMode::Passive: BotSayModePassive(bot); break;
        case model::BotCombatMode::Hold:    BotSayModeHold(bot);    break;
        case model::BotCombatMode::Guard:   BotSayModeGuard(bot);   break;
    }
}

void HandleBotCast(
    ChatHandler* handler,
    BotCastCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot cast requires an in-game player.");
        return;
    }

    std::optional<model::RosterEntry> entry =
        ResolveBotRosterEntry(session->GetAccountId(), command.botRef);
    if (!entry)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bot not found in roster.");
        return;
    }

    if (entry->characterGuid == player->GetGUID().GetCounter())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld cannot command the character you are logged in on.");
        return;
    }

    Player* bot = ResolveActiveBotForOwner(player, command.botRef);
    if (!bot)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld {} is not active. Use '.lwbot request <id>' first.",
            entry->controllableProfile.profile.name);
        return;
    }

    std::uint32_t const spellId = ResolveSpellByName(bot, command.spellName);
    if (!spellId)
    {
        BotSayDeny(bot);
        return;
    }

    // Resolve target from the normalized targetName token:
    //   (no "on" clause) / "Yourself" → the bot itself
    //   "Me"       → the owner/player giving the command
    //   "Mytarget" → whatever the owner currently has targeted (works for mobs)
    //   "Focus"    → the owner's focus target
    //   anything else → a character name looked up online
    Unit* target = nullptr;
    if (!command.targetName.has_value() || *command.targetName == "Yourself")
    {
        target = bot;
    }
    else if (*command.targetName == "Me")
    {
        target = player;
    }
    else if (*command.targetName == "Mytarget")
    {
        ObjectGuid const selection = player->GetTarget();
        if (!selection)
        {
            BotSayTargetNotFound(bot);
            return;
        }
        target = ObjectAccessor::GetUnit(*player, selection);
        if (!target)
        {
            BotSayTargetNotFound(bot);
            return;
        }
    }
    else if (*command.targetName == "Focus")
    {
        // This AzerothCore branch does not expose a dedicated player focus
        // update field, so "focus" falls back to the player's current target.
        ObjectGuid const focus = player->GetTarget();
        if (!focus)
        {
            BotSayTargetNotFound(bot);
            return;
        }
        target = ObjectAccessor::GetUnit(*player, focus);
        if (!target)
        {
            BotSayTargetNotFound(bot);
            return;
        }
    }
    else
    {
        target = ObjectAccessor::FindPlayerByName(*command.targetName);
        if (!target)
        {
            BotSayTargetNotFound(bot);
            return;
        }
    }

    BotSayConfirm(bot);
    bot->CastSpell(target, spellId, false);
}

void HandleBotAttack(
    ChatHandler* handler,
    BotAttackCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot attack requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    // Resolve the target: named target, owner's current target, or owner's victim.
    Unit* target = nullptr;
    if (command.targetName.has_value())
    {
        target = ObjectAccessor::FindPlayerByName(*command.targetName);
        if (!target)
        {
            BotSayTargetNotFound(bots.front());
            return;
        }
    }
    else
    {
        // Prefer owner's active victim; fall back to current selection.
        target = player->GetVictim();
        if (!target)
        {
            ObjectGuid const sel = player->GetTarget();
            if (sel)
                target = ObjectAccessor::GetUnit(*player, sel);
        }
        if (!target)
        {
            BotSayTargetNotFound(bots.front());
            return;
        }
    }

    for (Player* bot : bots)
        living_world::ai::SetBotForcedTarget(bot->GetGUID(), target->GetGUID());
    BotSayConfirm(bots.front());
}

void HandleBotYoink(
    ChatHandler* handler,
    BotYoinkCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot yoink requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    std::uint32_t yoinkedCount = 0;
    for (Player* bot : bots)
    {
        if (!bot || bot == player)
            continue;

        if (bot->GetMapId() != player->GetMapId())
            continue;

        living_world::ai::SetBotDisengaged(bot->GetGUID(), false);
        bot->AttackStop();
        bot->InterruptNonMeleeSpells(false);
        bot->GetMotionMaster()->Clear(false);
        bot->NearTeleportTo(
            player->GetPositionX(),
            player->GetPositionY(),
            player->GetPositionZ(),
            player->GetOrientation(),
            true);
        bot->GetMotionMaster()->MoveFollow(player, 2.5f, 3.14159f);
        ++yoinkedCount;
    }

    if (yoinkedCount == 0)
    {
        handler->SendErrorMessage("LivingWorld yoink failed: no active bots on your map.");
        return;
    }

    BotSayConfirm(bots.front());
}

struct TaughtAbilityEntry
{
    std::uint32_t spellId = 0;
    std::string name;
    std::string rank;
};

struct BotTrainingResult
{
    std::uint64_t availableCost = 0;
    std::vector<TaughtAbilityEntry> taughtAbilities;
};

std::string FormatTrainingMoney(std::uint64_t amount)
{
    std::uint64_t const gold = amount / GOLD;
    amount %= GOLD;
    std::uint64_t const silver = amount / SILVER;
    amount %= SILVER;
    std::uint64_t const copper = amount;

    std::string text;
    if (gold > 0)
        text += std::to_string(gold) + " Gold";
    if (silver > 0)
    {
        if (!text.empty())
            text += ' ';
        text += std::to_string(silver) + " Silver";
    }
    if (copper > 0 || text.empty())
    {
        if (!text.empty())
            text += ' ';
        text += std::to_string(copper) + " Copper";
    }

    return text;
}

std::vector<TaughtAbilityEntry> ResolveTrainerSpellAbilities(
    std::uint32_t trainerSpellId,
    LocaleConstant locale)
{
    std::vector<TaughtAbilityEntry> abilities;

    auto appendSpell = [locale, &abilities](std::uint32_t spellId)
    {
        if (!spellId)
            return;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);

        TaughtAbilityEntry entry;
        entry.spellId = spellId;
        if (spellInfo)
        {
            char const* spellName = spellInfo->SpellName[locale];
            if (!spellName || spellName[0] == '\0')
                spellName = spellInfo->SpellName[DEFAULT_LOCALE];
            char const* spellRank = spellInfo->Rank[locale];
            if (!spellRank || spellRank[0] == '\0')
                spellRank = spellInfo->Rank[DEFAULT_LOCALE];

            entry.name = (spellName && spellName[0] != '\0')
                ? spellName
                : "Ability " + std::to_string(spellId);
            if (spellRank && spellRank[0] != '\0')
                entry.rank = spellRank;
        }
        else
        {
            entry.name = "Ability " + std::to_string(spellId);
        }

        abilities.push_back(std::move(entry));
    };

    SpellInfo const* trainerSpellInfo = sSpellMgr->GetSpellInfo(trainerSpellId);
    bool learnedTriggeredSpell = false;
    if (trainerSpellInfo)
    {
        for (SpellEffectInfo const& spellEffectInfo : trainerSpellInfo->GetEffects())
        {
            if (!spellEffectInfo.IsEffect(SPELL_EFFECT_LEARN_SPELL) ||
                !spellEffectInfo.TriggerSpell)
                continue;

            appendSpell(spellEffectInfo.TriggerSpell);
            learnedTriggeredSpell = true;
        }
    }

    if (!learnedTriggeredSpell)
        appendSpell(trainerSpellId);

    return abilities;
}

void PersistLearnedBotSpell(Player* bot, std::uint32_t spellId)
{
    CharacterDatabase.Execute(
        "INSERT IGNORE INTO character_spell (guid, spell, specMask) "
        "SELECT source_character_guid, {}, 1 "
        "FROM living_world_account_alt_runtime "
        "WHERE clone_character_guid = {} LIMIT 1",
        spellId,
        bot->GetGUID().GetCounter());
}

bool DeductTrainingCost(Player* bot, Player* owner, std::uint32_t cost)
{
    if (cost == 0)
        return true;

    std::uint64_t const botGold = bot->GetMoney();
    if (botGold >= cost)
    {
        bot->ModifyMoney(-static_cast<int64>(cost));
        return true;
    }

    std::uint32_t const remainder = cost - static_cast<std::uint32_t>(botGold);
    if (owner->GetMoney() < remainder)
        return false;

    if (botGold > 0)
        bot->ModifyMoney(-static_cast<int64>(botGold));
    owner->ModifyMoney(-static_cast<int64>(remainder));
    return true;
}

BotTrainingResult TrainBotFromClassTrainers(
    Player* owner,
    Player* bot,
    LocaleConstant locale)
{
    struct TeachEntry
    {
        std::uint32_t trainerSpellId = 0;
        std::uint32_t cost = 0;
        std::vector<TaughtAbilityEntry> learnedAbilities;
    };

    BotTrainingResult result;
    std::vector<TeachEntry> toTeach;

    std::vector<Trainer::Trainer const*> const& trainers =
        sObjectMgr->GetClassTrainers(bot->getClass());

    for (Trainer::Trainer const* trainer : trainers)
    {
        if (!trainer)
            continue;

        for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
        {
            if (!trainer->CanTeachSpell(bot, &trainerSpell))
                continue;

            bool already = false;
            for (TeachEntry const& entry : toTeach)
            {
                if (entry.trainerSpellId == trainerSpell.SpellId)
                {
                    already = true;
                    break;
                }
            }

            if (already)
                continue;

            result.availableCost += trainerSpell.MoneyCost;

            TeachEntry entry;
            entry.trainerSpellId = trainerSpell.SpellId;
            entry.cost = trainerSpell.MoneyCost;
            entry.learnedAbilities = ResolveTrainerSpellAbilities(
                trainerSpell.SpellId,
                locale);
            toTeach.push_back(std::move(entry));
        }
    }

    if (toTeach.empty())
        return result;

    std::stable_sort(
        toTeach.begin(),
        toTeach.end(),
        [](TeachEntry const& left, TeachEntry const& right)
        {
            return left.cost < right.cost;
        });

    for (TeachEntry const& entry : toTeach)
    {
        if (!DeductTrainingCost(bot, owner, entry.cost))
            break;

        for (TaughtAbilityEntry const& ability : entry.learnedAbilities)
        {
            bot->learnSpell(ability.spellId, false);
            PersistLearnedBotSpell(bot, ability.spellId);
            result.taughtAbilities.push_back(ability);
        }
    }

    return result;
}

void SendBotTrainingOutput(
    ChatHandler* handler,
    Player* bot,
    BotTrainingResult const& result)
{
    if (!handler || !bot)
        return;

    if (result.taughtAbilities.empty())
    {
        if (result.availableCost > 0)
        {
            handler->PSendSysMessage(
                "Abilities are available for {}",
                FormatTrainingMoney(result.availableCost));
        }
        else
        {
            handler->PSendSysMessage(
                "{} already knows everything available at this level.",
                bot->GetName());
        }

        return;
    }

    for (TaughtAbilityEntry const& ability : result.taughtAbilities)
    {
        if (ability.rank.empty())
        {
            handler->PSendSysMessage(
                "{} Gaind {}",
                bot->GetName(),
                ability.name);
        }
        else
        {
            handler->PSendSysMessage(
                "{} Gaind {} {}",
                bot->GetName(),
                ability.name,
                ability.rank);
        }
    }
}

void HandleBotTrain(
    ChatHandler* handler,
    BotTrainCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot train requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    if (IsPartyBotRef(command.botRef))
    {
        bool taughtAnything = false;

        for (Player* bot : bots)
        {
            BotTrainingResult const result = TrainBotFromClassTrainers(
                player,
                bot,
                static_cast<LocaleConstant>(session->GetSessionDbcLocale()));
            if (!result.taughtAbilities.empty())
                taughtAnything = true;
            SendBotTrainingOutput(handler, bot, result);
        }

        if (!taughtAnything)
        {
            return;
        }

        BotSayConfirm(bots.front());
        return;
    }

    Player* bot = bots.front();
    BotTrainingResult const result = TrainBotFromClassTrainers(
        player,
        bot,
        static_cast<LocaleConstant>(session->GetSessionDbcLocale()));
    SendBotTrainingOutput(handler, bot, result);

    if (result.taughtAbilities.empty())
        return;

    BotSayConfirm(bot);
}

void HandleBotDisengage(
    ChatHandler* handler,
    BotDisengageCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot disengage requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    for (Player* bot : bots)
        living_world::ai::SetBotDisengaged(bot->GetGUID(), true);
    BotSayConfirm(bots.front());
}

// Sends a system message to the owner after a delay via the owner's event queue.
// If botGuid is set, the message is suppressed when retreat is no longer active.
class OwnerDelayedMessageEvent final : public BasicEvent
{
public:
    OwnerDelayedMessageEvent(ObjectGuid ownerGuid, std::string message,
                             ObjectGuid botGuid = ObjectGuid::Empty,
                             bool suppressIfNotRetreating = false)
        : _ownerGuid(ownerGuid), _message(std::move(message)),
          _botGuid(botGuid), _suppressIfNotRetreating(suppressIfNotRetreating) {}

    bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
    {
        if (_suppressIfNotRetreating)
        {
            if (!_botGuid)
            {
                if (!IsAnyOwnerBotRetreating(_ownerGuid))
                    return true;
            }
            else if (!living_world::ai::IsBotRetreating(_botGuid))
            {
                return true;
            }
        }

        Player* owner = ObjectAccessor::FindConnectedPlayer(_ownerGuid);
        if (owner && owner->GetSession())
            ChatHandler(owner->GetSession()).SendSysMessage(_message.c_str());
        return true;
    }
private:
    ObjectGuid  _ownerGuid;
    std::string _message;
    ObjectGuid  _botGuid;
    bool        _suppressIfNotRetreating;
};

void HandleBotRetreat(
    ChatHandler* handler,
    BotRetreatCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot retreat requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    constexpr uint32_t RetreatDurationMs = 30000;

    if (IsPartyBotRef(command.botRef))
    {
        bool anyRetreating = false;
        for (Player* bot : bots)
        {
            if (living_world::ai::IsBotRetreating(bot->GetGUID()))
            {
                anyRetreating = true;
                break;
            }
        }

        if (anyRetreating)
        {
            for (Player* bot : bots)
            {
                if (living_world::ai::IsBotRetreating(bot->GetGUID()))
                    living_world::ai::SetBotRetreat(bot->GetGUID(), RetreatDurationMs);
            }

            SendPlayerLog(
                handler,
                static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
                "LivingWorld party retreat cancelled. Party re-engaging.");
            bots.front()->Say("Retreat cancelled, back in the fight!", LANG_UNIVERSAL);
            return;
        }

        for (Player* bot : bots)
            living_world::ai::SetBotRetreat(bot->GetGUID(), RetreatDurationMs);

        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld party is disengaged and fleeing for the next 30 seconds. "
            "Use '.lwbot party retreat' again to cancel.");
        bots.front()->Say("Falling back! Stay close!", LANG_UNIVERSAL);

        ObjectGuid const ownerGuid = player->GetGUID();

        player->m_Events.AddEventAtOffset(
            new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 5 seconds.", ObjectGuid::Empty, true),
            Milliseconds(25000));
        player->m_Events.AddEventAtOffset(
            new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 3 seconds.", ObjectGuid::Empty, true),
            Milliseconds(27000));
        player->m_Events.AddEventAtOffset(
            new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 2 seconds.", ObjectGuid::Empty, true),
            Milliseconds(28000));
        player->m_Events.AddEventAtOffset(
            new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 1 second.", ObjectGuid::Empty, true),
            Milliseconds(29000));
        player->m_Events.AddEventAtOffset(
            new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat over. Party is back in action.", ObjectGuid::Empty, true),
            Milliseconds(30000));
        return;
    }

    Player* bot = bots.front();
    bool const activated = living_world::ai::SetBotRetreat(bot->GetGUID(), RetreatDurationMs);

    if (!activated)
    {
        // Was already retreating — cancelled.
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld retreat cancelled. Party re-engaging.");
        bot->Say("Retreat cancelled, back in the fight!", LANG_UNIVERSAL);
        return;
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld party is disengaged and fleeing for the next 30 seconds. "
        "Use '.lwbot retreat' again to cancel.");
    bot->Say("Falling back! Stay close!", LANG_UNIVERSAL);

    ObjectGuid const ownerGuid = player->GetGUID();

    ObjectGuid const botGuid = bot->GetGUID();

    // 5-second countdown: warn at T+25s, T+27s, T+28s, T+29s, expire at T+30s.
    // All events check IsBotRetreating so they are silently skipped if cancelled.
    player->m_Events.AddEventAtOffset(
        new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 5 seconds.", botGuid, true),
        Milliseconds(25000));
    player->m_Events.AddEventAtOffset(
        new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 3 seconds.", botGuid, true),
        Milliseconds(27000));
    player->m_Events.AddEventAtOffset(
        new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 2 seconds.", botGuid, true),
        Milliseconds(28000));
    player->m_Events.AddEventAtOffset(
        new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat ends in 1 second.", botGuid, true),
        Milliseconds(29000));
    player->m_Events.AddEventAtOffset(
        new OwnerDelayedMessageEvent(ownerGuid, "LivingWorld retreat over. Party is back in action.", botGuid, true),
        Milliseconds(30000));
}

void HandleBotProfileSet(
    ChatHandler* handler,
    BotProfileSetCommand const& command)
{
    WorldSession* session = handler->GetSession();
    if (!session)
    {
        handler->SendErrorMessage("LivingWorld bot profile commands require an in-game session.");
        return;
    }

    std::optional<model::RosterEntry> entry =
        ResolveBotRosterEntry(session->GetAccountId(), command.botRef);
    if (!entry)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bot not found in roster.");
        return;
    }

    if (entry->characterGuid == session->GetPlayer()->GetGUID().GetCounter())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld cannot switch profiles for the character you are currently logged in on.");
        return;
    }

    std::uint64_t const characterGuid = entry->characterGuid;
    std::uint8_t const slot = command.profileSlot;

    integration::SqlBotCombatProfileSelectionRepository selectionRepository;
    selectionRepository.SaveRuntimeSelection(
        model::BotCombatRuntimeSelection {
            characterGuid,
            slot,
        });

    if (Player* activeBot = ResolveActiveBotForOwner(session->GetPlayer(), command.botRef))
        living_world::ai::InvalidateBotCombatCaches(activeBot->GetGUID());

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld set active profile slot {} for {}.",
        static_cast<std::uint32_t>(slot),
        entry->controllableProfile.profile.name);
}

// ---------------------------------------------------------------
// Inventory helpers for refreshments
// ---------------------------------------------------------------

constexpr std::uint32_t SpellCategoryFood  = 11;
constexpr std::uint32_t SpellCategoryDrink = 59;

Item* FindConsumableInInventory(Player* bot, std::uint32_t spellCategory)
{
    auto matches = [&](Item* item) -> bool {
        if (!item) return false;
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || proto->Class != ITEM_CLASS_CONSUMABLE) return false;
        for (std::uint8_t i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (static_cast<std::uint32_t>(proto->Spells[i].SpellCategory) == spellCategory)
                return true;
        return false;
    };

    for (std::uint8_t slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot); matches(item))
            return item;

    for (std::uint8_t bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* pBag = bot->GetBagByPos(bag))
            for (std::uint32_t slot = 0; slot < pBag->GetBagSize(); ++slot)
                if (Item* item = pBag->GetItemByPos(slot); matches(item))
                    return item;

    return nullptr;
}

void BotUseConsumable(Player* bot, Item* item)
{
    if (!item) return;
    ItemTemplate const* proto = item->GetTemplate();
    for (std::uint8_t i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId > 0 &&
            proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
        {
            bot->CastSpell(bot, static_cast<std::uint32_t>(proto->Spells[i].SpellId), false);
            return;
        }
    }
}

// ---------------------------------------------------------------
// Follow / refreshments / buff handlers
// ---------------------------------------------------------------

void HandleBotFollow(
    ChatHandler* handler,
    BotFollowCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bot follow requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    for (Player* bot : bots)
    {
        living_world::ai::SetBotDisengaged(bot->GetGUID(), false);
        bot->AttackStop();
        bot->GetMotionMaster()->Clear(false);
        bot->GetMotionMaster()->MoveFollow(player, 2.5f, 3.14159f);
    }
    BotSayConfirm(bots.front());
}

void HandleBotRefreshments(
    ChatHandler* handler,
    BotRefreshmentsCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld refreshments requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    std::uint32_t fed = 0;
    for (Player* bot : bots)
    {
        bool const needFood = bot->GetHealthPct() < 60.0f;
        bool const needDrink = bot->GetMaxPower(POWER_MANA) > 0 &&
            100.0f * static_cast<float>(bot->GetPower(POWER_MANA)) /
                     static_cast<float>(bot->GetMaxPower(POWER_MANA)) < 60.0f;

        if (!needFood && !needDrink)
            continue;

        if (needFood)
            BotUseConsumable(bot, FindConsumableInInventory(bot, SpellCategoryFood));
        if (needDrink)
            BotUseConsumable(bot, FindConsumableInInventory(bot, SpellCategoryDrink));
        ++fed;
    }

    if (fed > 0)
        BotSayConfirm(bots.front());
    else
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no bots needed refreshments.");
}

void HandleBotBuff(
    ChatHandler* handler,
    BotBuffCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld buff requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    for (Player* bot : bots)
        living_world::ai::ForceBotBuffRefresh(bot, player);
    BotSayConfirm(bots.front());
}

std::string SanitizeAddonField(std::string_view input)
{
    std::string out(input);
    std::replace(out.begin(), out.end(), ';', ',');
    return out;
}

// Send an addon message to a player's client using the LWBOT prefix.
// The client fires CHAT_MSG_ADDON with prefix="LWBOT" and the payload as
// the message argument. No 255-byte limit applies — this is server→client.
void SendLWBotAddonMessage(Player* player, std::string const& payload)
{
    std::string msg = "LWBOT\t" + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(
        data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, msg);
    player->GetSession()->SendPacket(&data);
}

void SendQuestRewardModeAddonMessage(
    Player* player,
    service::BotQuestRewardMode mode)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    SendLWBotAddonMessage(
        player,
        std::string("QMODE;") +
            (mode == service::BotQuestRewardMode::Manual ? "MANUAL" : "SMART"));
}

void SendQuestRewardsAddonState(Player* player)
{
    if (!player || !player->GetSession())
    {
        return;
    }

    service::BotQuestRewardService questRewardService;
    SendLWBotAddonMessage(player, "QCLR");
    SendQuestRewardModeAddonMessage(
        player,
        questRewardService.GetRewardMode(player->GetGUID().GetCounter()));

    for (service::PendingQuestReward const& pending :
         questRewardService.BuildPendingRewards(player))
    {
        std::string payload = "QST;";
        payload += pending.botName;
        payload += ';';
        payload += std::to_string(pending.questId);
        payload += ';';
        payload += pending.questTitle;

        for (service::QuestRewardChoice const& choice : pending.choices)
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

// Serialize a single item's link fields into the shared colon-delimited format.
// Fields: itemId:enchId:g1:g2:g3:guidLow  (gems sent as socket enchantment IDs
// so the client can reconstruct a usable hyperlink; count appended for bags).
std::string SerializeItemFields(Item const* item, bool includeCount)
{
    std::string s;
    s += std::to_string(item->GetEntry());
    s += ':';
    s += std::to_string(item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT));
    s += ':';
    s += std::to_string(item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT));
    s += ':';
    s += std::to_string(item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2));
    s += ':';
    s += std::to_string(item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3));
    if (includeCount)
    {
        s += ':';
        s += std::to_string(item->GetCount());
    }
    s += ':';
    s += std::to_string(item->GetGUID().GetCounter());
    return s;
}

// Build the full INV payload for the LWBOT addon message.
// Format: INV;<botName>;<G:slot:fields...>;<B:bagIdx:slotIdx:fields...>
std::string BuildBotInventoryPayload(Player* bot)
{
    std::string payload = "INV;";
    payload += bot->GetName();

    // Equipped gear (slots 0–18)
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item const* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;
        payload += ";G:";
        payload += std::to_string(slot);
        payload += ':';
        payload += SerializeItemFields(item, false);
    }

    // Backpack (bag index 0 in the protocol, slot offset normalised to 0-based)
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item const* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;
        payload += ";B:0:";
        payload += std::to_string(slot - INVENTORY_SLOT_ITEM_START);
        payload += ':';
        payload += SerializeItemFields(item, true);
    }

    // Equipped bags (indices 1–4 in the protocol)
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag const* bag = bot->GetBagByPos(bagSlot);
        if (!bag)
            continue;
        uint8 const bagIdx = bagSlot - INVENTORY_SLOT_BAG_START + 1;
        for (uint32 s = 0; s < bag->GetBagSize(); ++s)
        {
            Item const* item = bag->GetItemByPos(static_cast<uint8>(s));
            if (!item)
                continue;
            payload += ";B:";
            payload += std::to_string(bagIdx);
            payload += ':';
            payload += std::to_string(s);
            payload += ':';
            payload += SerializeItemFields(item, true);
        }
    }

    return payload;
}

// Find an item anywhere in the bot's inventory (gear + bags) by GUID low.
// Returns the item and records its bag/slot for RemoveItem.
Item* FindBotItemByGuidLow(
    Player* bot,
    std::uint32_t guidLow,
    uint8& outBag,
    uint8& outSlot)
{
    // Equipped gear
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetGUID().GetCounter() == guidLow)
        {
            outBag  = INVENTORY_SLOT_BAG_0;
            outSlot = slot;
            return item;
        }
    }

    // Backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetGUID().GetCounter() == guidLow)
        {
            outBag  = INVENTORY_SLOT_BAG_0;
            outSlot = slot;
            return item;
        }
    }

    // Equipped bags
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = bot->GetBagByPos(bagSlot);
        if (!bag)
            continue;
        for (uint32 s = 0; s < bag->GetBagSize(); ++s)
        {
            Item* item = bag->GetItemByPos(static_cast<uint8>(s));
            if (item && item->GetGUID().GetCounter() == guidLow)
            {
                outBag  = bagSlot;
                outSlot = static_cast<uint8>(s);
                return item;
            }
        }
    }

    return nullptr;
}

void HandleBotBags(
    ChatHandler* handler,
    BotBagsCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bags requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Player* bot = bots.front();
    SendLWBotAddonMessage(player, BuildBotInventoryPayload(bot));
}

void HandleBotRetrieve(
    ChatHandler* handler,
    BotRetrieveCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld retrieve requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Player* bot = bots.front();
    uint8 foundBag = 0, foundSlot = 0;
    Item* item = FindBotItemByGuidLow(bot, command.itemGuidLow, foundBag, foundSlot);
    if (!item)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld retrieve: item not found on bot.");
        return;
    }

    std::uint32_t const sourceCount = item->GetCount();
    std::uint32_t const retrieveCount = command.itemCount.value_or(sourceCount);
    if (retrieveCount == 0 || retrieveCount > sourceCount)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld retrieve: invalid stack count.");
        return;
    }

    ItemPosCountVec dest;
    InventoryResult const canStore =
        player->CanStoreItem(
            NULL_BAG,
            NULL_SLOT,
            dest,
            item->GetEntry(),
            retrieveCount,
            item,
            false);
    if (canStore != EQUIP_ERR_OK)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld retrieve: your inventory is full.");
        return;
    }

    if (retrieveCount == sourceCount)
    {
        bot->RemoveItem(foundBag, foundSlot, true);
        player->StoreItem(dest, item, true);
    }
    else
    {
        Item* splitItem = item->CloneItem(retrieveCount, player);
        if (!splitItem)
        {
            SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
                "LivingWorld retrieve: failed to split item stack.");
            return;
        }

        item->SetCount(sourceCount - retrieveCount);
        item->SetState(ITEM_CHANGED, bot);
        if (bot->IsInWorld())
        {
            item->SendUpdateToPlayer(bot);
        }

        player->StoreItem(dest, splitItem, true);
    }

    // Push refreshed inventory back so the addon panel updates automatically.
    SendLWBotAddonMessage(player, BuildBotInventoryPayload(bot));
}

uint8 ResolveBotBagMoveDestination(std::uint8_t bagIndex)
{
    if (bagIndex == 0)
        return NULL_BAG;
    if (bagIndex >= 1 && bagIndex <= 4)
        return static_cast<uint8>(INVENTORY_SLOT_BAG_START + (bagIndex - 1));
    return NULL_SLOT;
}

void HandleBotBagMove(
    ChatHandler* handler,
    BotBagMoveCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld bagmove requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Player* bot = bots.front();
    uint8 foundBag = 0, foundSlot = 0;
    Item* item = FindBotItemByGuidLow(bot, command.itemGuidLow, foundBag, foundSlot);
    if (!item)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bagmove: item not found on bot.");
        return;
    }

    uint8 const destinationBag = ResolveBotBagMoveDestination(command.bagIndex);
    if (destinationBag == NULL_SLOT)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bagmove: invalid target bag index.");
        return;
    }

    if (command.bagIndex > 0 && !bot->GetBagByPos(destinationBag))
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bagmove: that bag slot is empty.");
        return;
    }

    ItemPosCountVec dest;
    InventoryResult const canStore = bot->CanStoreItem(destinationBag, NULL_SLOT, dest, item, false);
    if (canStore != EQUIP_ERR_OK)
    {
        BotSayDeny(bot);
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld bagmove: no room in that bag.");
        return;
    }

    uint16 const src = (static_cast<uint16>(foundBag) << 8) | static_cast<uint16>(foundSlot);
    if (dest.size() == 1 && dest[0].pos == src)
    {
        SendLWBotAddonMessage(player, BuildBotInventoryPayload(bot));
        return;
    }

    bot->RemoveItem(foundBag, foundSlot, true);
    bot->StoreItem(dest, item, true);
    BotSayConfirm(bot);
    SendLWBotAddonMessage(player, BuildBotInventoryPayload(bot));
}

void HandleBotEquip(
    ChatHandler* handler,
    BotEquipCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld equip requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Player* bot = bots.front();
    uint8 foundBag = 0, foundSlot = 0;
    Item* item = FindBotItemByGuidLow(bot, command.itemGuidLow, foundBag, foundSlot);
    if (!item)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld equip: item not found on bot.");
        return;
    }

    // Determine the destination equip slot. swap=true lets CanEquipItem choose
    // any valid slot, including occupied ones (we handle displacement below).
    uint16 dest = 0;
    InventoryResult canEquip = bot->CanEquipItem(NULL_SLOT, dest, item, true, false);
    if (canEquip != EQUIP_ERR_OK)
    {
        BotSayDeny(bot);
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld equip: bot cannot equip that item (class/level restriction?).");
        return;
    }

    // dest is a packed (bag<<8|slot) position; extract the equipment slot byte.
    uint8 const equipSlot = static_cast<uint8>(dest & 0xFF);

    // If the destination slot is already occupied, move the existing item to bags.
    Item* displaced = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);
    if (displaced)
    {
        ItemPosCountVec bagDest;
        InventoryResult canStore =
            bot->CanStoreItem(NULL_BAG, NULL_SLOT, bagDest, displaced, false);
        if (canStore != EQUIP_ERR_OK)
        {
            BotSayDeny(bot);
            SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
                "LivingWorld equip: no bag space to unequip the current item.");
            return;
        }
        bot->RemoveItem(INVENTORY_SLOT_BAG_0, equipSlot, true);
        bot->StoreItem(bagDest, displaced, true);
    }

    bot->RemoveItem(foundBag, foundSlot, true);
    bot->EquipItem(dest, item, true);
    BotSayConfirm(bot);

    SendLWBotAddonMessage(player, BuildBotInventoryPayload(bot));
}

void HandleBotUnequip(
    ChatHandler* handler,
    BotUnequipCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld unequip requires an in-game player.");
        return;
    }

    std::vector<Player*> bots = ResolveSelectedBotsForOwner(player, command.botRef);
    if (bots.empty())
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Player* bot = bots.front();
    uint8 foundBag = 0, foundSlot = 0;
    Item* item = FindBotItemByGuidLow(bot, command.itemGuidLow, foundBag, foundSlot);
    if (!item)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld unequip: item not found on bot.");
        return;
    }

    if (foundBag != INVENTORY_SLOT_BAG_0 || foundSlot >= INVENTORY_SLOT_ITEM_START)
    {
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld unequip: item is not currently equipped.");
        return;
    }

    ItemPosCountVec dest;
    InventoryResult const canStore =
        bot->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
    if (canStore != EQUIP_ERR_OK)
    {
        BotSayDeny(bot);
        SendPlayerLog(handler, static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld unequip: no bag space on bot.");
        return;
    }

    bot->RemoveItem(foundBag, foundSlot, true);
    bot->StoreItem(dest, item, true);
    BotSayConfirm(bot);

    SendLWBotAddonMessage(player, BuildBotInventoryPayload(bot));
}

void SendQuestActionsAddonState(Player* player)
{
    if (!player || !player->GetSession())
        return;

    SendLWBotAddonMessage(player, "QACLR");

    Unit* target = player->GetSelectedUnit();
    Creature* creature = target ? target->ToCreature() : nullptr;
    if (!creature || !creature->IsAlive() ||
        !(creature->GetNpcFlags() & UNIT_NPC_FLAG_QUESTGIVER))
    {
        SendLWBotAddonMessage(player, "QAEND");
        return;
    }

    std::uint32_t const npcEntry = creature->GetEntry();
    QuestRelationBounds questRelations =
        sObjectMgr->GetCreatureQuestRelationBounds(npcEntry);
    QuestRelationBounds questInvolvedRelations =
        sObjectMgr->GetCreatureQuestInvolvedRelationBounds(npcEntry);

    for (Player* bot :
         service::BotPlayerRegistry::Instance().FindBotsForOwner(player->GetGUID()))
    {
        if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
            continue;

        for (auto it = questRelations.first; it != questRelations.second; ++it)
        {
            std::uint32_t const questId = it->second;
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            if (bot->GetQuestStatus(questId) != QUEST_STATUS_NONE)
                continue;

            if (!bot->CanTakeQuest(quest, false))
                continue;

            if (!bot->CanAddQuest(quest, false))
                continue;

            std::string payload = "QA;";
            payload += bot->GetName();
            payload += ';';
            payload += std::to_string(questId);
            payload += ';';
            payload += SanitizeAddonField(quest->GetTitle());
            payload += ";PICKUP";
            SendLWBotAddonMessage(player, payload);
        }

        for (auto it = questInvolvedRelations.first;
             it != questInvolvedRelations.second; ++it)
        {
            std::uint32_t const questId = it->second;
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            QuestStatus const status = bot->GetQuestStatus(questId);
            if (status != QUEST_STATUS_COMPLETE)
                continue;

            if (bot->GetQuestRewardStatus(questId))
                continue;

            std::string payload = "QA;";
            payload += bot->GetName();
            payload += ';';
            payload += std::to_string(questId);
            payload += ';';
            payload += SanitizeAddonField(quest->GetTitle());
            payload += ";TURNIN";

            if (quest->GetRewChoiceItemsCount() > 0)
                payload += ";CHOICES";

            SendLWBotAddonMessage(player, payload);
        }
    }

    SendLWBotAddonMessage(player, "QAEND");
}

void HandleQuestActions(
    ChatHandler* handler,
    QuestActionsCommand const&)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld questactions requires an in-game player.");
        return;
    }

    SendQuestActionsAddonState(player);
}

void HandleBotQuestPickup(
    ChatHandler* handler,
    BotQuestPickupCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld pickup requires an in-game player.");
        return;
    }

    Player* bot = ResolveActiveBotForOwner(player, command.botRef);
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(command.questId);
    if (!quest)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld pickup: quest {} not found.",
            command.questId);
        return;
    }

    if (!bot->CanTakeQuest(quest, false) || !bot->CanAddQuest(quest, false))
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld pickup: {} cannot take quest [{}].",
            bot->GetName(), quest->GetTitle());
        return;
    }

    bot->AddQuestAndCheckCompletion(quest, nullptr);

    BotSayConfirm(bot);
    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld {} picked up [{}].",
        bot->GetName(), quest->GetTitle());

    SendQuestActionsAddonState(player);
}

void HandleBotQuestTurnin(
    ChatHandler* handler,
    BotQuestTurninCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld turnin requires an in-game player.");
        return;
    }

    Player* bot = ResolveActiveBotForOwner(player, command.botRef);
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(command.questId);
    if (!quest)
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld turnin: quest {} not found.",
            command.questId);
        return;
    }

    if (bot->GetQuestStatus(command.questId) != QUEST_STATUS_COMPLETE ||
        bot->GetQuestRewardStatus(command.questId))
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld turnin: {} has not completed [{}].",
            bot->GetName(), quest->GetTitle());
        return;
    }

    if (!bot->CanRewardQuest(quest, false))
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld turnin: {} cannot turn in [{}] right now.",
            bot->GetName(), quest->GetTitle());
        return;
    }

    if (quest->GetRewChoiceItemsCount() > 0)
    {
        service::BotQuestRewardService questRewardService;
        service::BotQuestRewardMode const mode =
            questRewardService.GetRewardMode(player->GetGUID().GetCounter());

        if (mode == service::BotQuestRewardMode::Smart)
        {
            std::string error;
            std::optional<std::uint32_t> smartChoice;

            struct SmartPick
            {
                float score = -1000000.0f;
                std::uint32_t index = 0;
            };

            SmartPick best;
            SmartPick second;

            for (std::uint32_t i = 0; i < quest->GetRewChoiceItemsCount(); ++i)
            {
                if (!quest->RewardChoiceItemId[i] ||
                    !bot->CanRewardQuest(quest, i, false))
                    continue;

                ItemTemplate const* tmpl =
                    sObjectMgr->GetItemTemplate(quest->RewardChoiceItemId[i]);
                float score = tmpl ? static_cast<float>(tmpl->SellPrice) / 1000.0f
                                     + static_cast<float>(tmpl->ItemLevel)
                                   : 0.0f;
                if (score > best.score)
                {
                    second = best;
                    best = { score, i };
                }
                else if (score > second.score)
                {
                    second = { score, i };
                }
            }

            if (best.score > -999999.0f &&
                (second.score <= -999999.0f ||
                 std::fabs(best.score - second.score) >= 25.0f))
            {
                questRewardService.RewardBotQuestById(
                    player, bot, command.questId,
                    static_cast<std::uint8_t>(best.index + 1), error);
                BotSayConfirm(bot);
            }
        }

        SendQuestRewardsAddonState(player);
    }
    else
    {
        bot->RewardQuest(quest, 0, bot, false);
        BotSayConfirm(bot);
    }

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld {} turned in [{}].",
        bot->GetName(), quest->GetTitle());

    SendQuestActionsAddonState(player);
}

// ---------------------------------------------------------------
// Training actions — target-based trainer spell browsing / learning
// ---------------------------------------------------------------

Trainer::Trainer const* ResolveTargetedTrainer(Player* player)
{
    Unit* target = player->GetSelectedUnit();
    Creature* creature = target ? target->ToCreature() : nullptr;
    if (!creature || !creature->IsAlive() ||
        !(creature->GetNpcFlags() & UNIT_NPC_FLAG_TRAINER))
        return nullptr;

    Trainer::Trainer const* trainer = sObjectMgr->GetTrainer(creature->GetEntry());
    if (!trainer || trainer->GetTrainerType() != Trainer::Type::Class)
        return nullptr;

    if (!trainer->IsTrainerValidForPlayer(player))
        return nullptr;

    return trainer;
}

struct BotTrainerSpellOption
{
    std::uint32_t trainerSpellId = 0;
    std::uint32_t cost = 0;
    std::vector<TaughtAbilityEntry> abilities;
};

std::vector<BotTrainerSpellOption> CollectBotTrainerSpellOptions(
    Player* bot,
    LocaleConstant locale)
{
    std::vector<BotTrainerSpellOption> options;

    std::vector<Trainer::Trainer const*> const& trainers =
        sObjectMgr->GetClassTrainers(bot->getClass());

    for (Trainer::Trainer const* trainer : trainers)
    {
        if (!trainer)
            continue;

        for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
        {
            if (!trainer->CanTeachSpell(bot, &trainerSpell))
                continue;

            bool already = false;
            for (BotTrainerSpellOption const& option : options)
            {
                if (option.trainerSpellId == trainerSpell.SpellId)
                {
                    already = true;
                    break;
                }
            }

            if (already)
                continue;

            BotTrainerSpellOption option;
            option.trainerSpellId = trainerSpell.SpellId;
            option.cost = trainerSpell.MoneyCost;
            option.abilities = ResolveTrainerSpellAbilities(
                trainerSpell.SpellId,
                locale);
            options.push_back(std::move(option));
        }
    }

    std::stable_sort(
        options.begin(),
        options.end(),
        [](BotTrainerSpellOption const& left, BotTrainerSpellOption const& right)
        {
            return left.cost < right.cost;
        });

    return options;
}

BotTrainerSpellOption const* FindBotTrainerSpellOption(
    std::vector<BotTrainerSpellOption> const& options,
    std::uint32_t trainerSpellId)
{
    for (BotTrainerSpellOption const& option : options)
    {
        if (option.trainerSpellId == trainerSpellId)
            return &option;
    }

    return nullptr;
}

void SendTrainActionsAddonState(Player* player)
{
    if (!player || !player->GetSession())
        return;

    SendLWBotAddonMessage(player, "TACLR");

    Trainer::Trainer const* trainer = ResolveTargetedTrainer(player);
    if (!trainer)
    {
        SendLWBotAddonMessage(player,
            "TAEND;" + std::to_string(player->GetMoney()));
        return;
    }

    LocaleConstant const locale =
        static_cast<LocaleConstant>(player->GetSession()->GetSessionDbcLocale());

    for (Player* bot :
         service::BotPlayerRegistry::Instance().FindBotsForOwner(player->GetGUID()))
    {
        if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
            continue;

        std::vector<BotTrainerSpellOption> const options =
            CollectBotTrainerSpellOptions(bot, locale);
        if (options.empty())
            continue;

        bool hasBotHeader = false;

        for (BotTrainerSpellOption const& option : options)
        {
            if (!hasBotHeader)
            {
                std::string header = "TABOT;";
                header += bot->GetName();
                header += ';';
                header += std::to_string(bot->GetMoney());
                SendLWBotAddonMessage(player, header);
                hasBotHeader = true;
            }

            std::string spellLabel;
            for (TaughtAbilityEntry const& ability : option.abilities)
            {
                if (!spellLabel.empty())
                    spellLabel += ", ";
                spellLabel += ability.name;
                if (!ability.rank.empty())
                {
                    spellLabel += " (";
                    spellLabel += ability.rank;
                    spellLabel += ')';
                }
            }
            if (spellLabel.empty())
                spellLabel = "Spell " + std::to_string(option.trainerSpellId);

            std::string payload = "TA;";
            payload += bot->GetName();
            payload += ';';
            payload += std::to_string(option.trainerSpellId);
            payload += ';';
            payload += SanitizeAddonField(spellLabel);
            payload += ';';
            payload += std::to_string(option.cost);
            SendLWBotAddonMessage(player, payload);
        }
    }

    SendLWBotAddonMessage(player,
        "TAEND;" + std::to_string(player->GetMoney()));
}

void HandleTrainActions(
    ChatHandler* handler,
    TrainActionsCommand const&)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld trainactions requires an in-game player.");
        return;
    }

    SendTrainActionsAddonState(player);
}

void HandleBotTrainSpell(
    ChatHandler* handler,
    BotTrainSpellCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld trainspell requires an in-game player.");
        return;
    }

    Player* bot = ResolveActiveBotForOwner(player, command.botRef);
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
    {
        SendPlayerLog(handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot.");
        return;
    }

    if (!ResolveTargetedTrainer(player))
    {
        SendPlayerLog(handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld trainspell: target a trainer NPC first.");
        return;
    }

    LocaleConstant const locale =
        static_cast<LocaleConstant>(session->GetSessionDbcLocale());
    std::vector<BotTrainerSpellOption> const options =
        CollectBotTrainerSpellOptions(bot, locale);
    BotTrainerSpellOption const* option =
        FindBotTrainerSpellOption(options, command.trainerSpellId);
    if (!option)
    {
        SendPlayerLog(handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld trainspell: spell not available for {}.",
            bot->GetName());
        return;
    }

    BotTrainingResult result;
    result.availableCost = option->cost;

    if (DeductTrainingCost(bot, player, option->cost))
    {
        for (TaughtAbilityEntry const& ability : option->abilities)
        {
            bot->learnSpell(ability.spellId, false);
            PersistLearnedBotSpell(bot, ability.spellId);
            result.taughtAbilities.push_back(ability);
        }
    }

    SendBotTrainingOutput(handler, bot, result);
    if (!result.taughtAbilities.empty())
        BotSayConfirm(bot);
    SendTrainActionsAddonState(player);
}

void HandleBotTrainAll(
    ChatHandler* handler,
    BotTrainAllCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld trainall requires an in-game player.");
        return;
    }

    Player* bot = ResolveActiveBotForOwner(player, command.botRef);
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
    {
        SendPlayerLog(handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot.");
        return;
    }

    if (!ResolveTargetedTrainer(player))
    {
        SendPlayerLog(handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld trainall: target a trainer NPC first.");
        return;
    }

    LocaleConstant const locale =
        static_cast<LocaleConstant>(session->GetSessionDbcLocale());
    std::vector<BotTrainerSpellOption> const toLearn =
        CollectBotTrainerSpellOptions(bot, locale);

    if (toLearn.empty())
    {
        SendPlayerLog(handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld trainall: {} has nothing to learn here.",
            bot->GetName());
        SendTrainActionsAddonState(player);
        return;
    }

    BotTrainingResult result;
    for (BotTrainerSpellOption const& entry : toLearn)
    {
        result.availableCost += entry.cost;

        if (!DeductTrainingCost(bot, player, entry.cost))
            break;

        for (TaughtAbilityEntry const& ability : entry.abilities)
        {
            bot->learnSpell(ability.spellId, false);
            PersistLearnedBotSpell(bot, ability.spellId);
            result.taughtAbilities.push_back(ability);
        }
    }

    SendBotTrainingOutput(handler, bot, result);
    if (!result.taughtAbilities.empty())
        BotSayConfirm(bot);

    SendTrainActionsAddonState(player);
}

void HandleQuestRewards(
    ChatHandler* handler,
    QuestRewardsCommand const&)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld quests requires an in-game player.");
        return;
    }

    SendQuestRewardsAddonState(player);
}

void HandleQuestRewardModeSet(
    ChatHandler* handler,
    QuestRewardModeSetCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld questmode requires an in-game player.");
        return;
    }

    service::BotQuestRewardService questRewardService;
    service::BotQuestRewardMode const mode = command.smartMode
        ? service::BotQuestRewardMode::Smart
        : service::BotQuestRewardMode::Manual;
    questRewardService.SetRewardMode(player->GetGUID().GetCounter(), mode);

    SendPlayerLog(
        handler,
        static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
        "LivingWorld quest reward mode set to {}.",
        command.smartMode ? "smart" : "manual");

    SendQuestRewardModeAddonMessage(player, mode);
}

void HandleBotRewardChoice(
    ChatHandler* handler,
    BotRewardChoiceCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld reward requires an in-game player.");
        return;
    }

    Player* bot = ResolveActiveBotForOwner(player, command.botRef);
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld no active bot. Use '.lwbot request <id>' first.");
        return;
    }

    service::BotQuestRewardService questRewardService;
    std::string error;
    if (!questRewardService.RewardBotQuestById(
            player,
            bot,
            command.questId,
            command.choiceNumber,
            error))
    {
        SendPlayerLog(
            handler,
            static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum),
            "LivingWorld reward failed: {}.",
            error);
        return;
    }

    SendQuestRewardsAddonState(player);
}

bool HandleParsedCommand(
    ChatHandler* handler,
    ParsedCommand const& parsed)
{
    if (CommandParseError const* error =
        std::get_if<CommandParseError>(&parsed))
    {
        if (error->kind == CommandParseErrorKind::Empty)
        {
            RenderUsage(handler);
            return true;
        }

        handler->PSendSysMessage(
            "LivingWorld command error: {} ({})",
            ToParseErrorText(error->kind),
            error->detail);
        RenderUsage(handler);
        return true;
    }

    if (std::get_if<RosterListCommand>(&parsed))
    {
        RenderRosterList(handler);
        return true;
    }

    if (RosterRequestCommand const* command =
        std::get_if<RosterRequestCommand>(&parsed))
    {
        RenderRosterRequest(handler, *command);
        return true;
    }

    if (RosterDismissCommand const* command =
        std::get_if<RosterDismissCommand>(&parsed))
    {
        RenderDismissBot(handler, *command);
        return true;
    }

    if (BotProfileSetCommand const* command =
        std::get_if<BotProfileSetCommand>(&parsed))
    {
        HandleBotProfileSet(handler, *command);
        return true;
    }

    if (BotCastCommand const* command =
        std::get_if<BotCastCommand>(&parsed))
    {
        HandleBotCast(handler, *command);
        return true;
    }

    if (BotAttackCommand const* command =
        std::get_if<BotAttackCommand>(&parsed))
    {
        HandleBotAttack(handler, *command);
        return true;
    }

    if (BotDisengageCommand const* command =
        std::get_if<BotDisengageCommand>(&parsed))
    {
        HandleBotDisengage(handler, *command);
        return true;
    }

    if (BotTrainCommand const* command =
        std::get_if<BotTrainCommand>(&parsed))
    {
        HandleBotTrain(handler, *command);
        return true;
    }

    if (BotRetreatCommand const* command =
        std::get_if<BotRetreatCommand>(&parsed))
    {
        HandleBotRetreat(handler, *command);
        return true;
    }

    if (BotFollowCommand const* command =
        std::get_if<BotFollowCommand>(&parsed))
    {
        HandleBotFollow(handler, *command);
        return true;
    }

    if (BotYoinkCommand const* command =
        std::get_if<BotYoinkCommand>(&parsed))
    {
        HandleBotYoink(handler, *command);
        return true;
    }

    if (BotRefreshmentsCommand const* command =
        std::get_if<BotRefreshmentsCommand>(&parsed))
    {
        HandleBotRefreshments(handler, *command);
        return true;
    }

    if (BotBuffCommand const* command =
        std::get_if<BotBuffCommand>(&parsed))
    {
        HandleBotBuff(handler, *command);
        return true;
    }

    if (BotBagsCommand const* command =
        std::get_if<BotBagsCommand>(&parsed))
    {
        HandleBotBags(handler, *command);
        return true;
    }

    if (BotRetrieveCommand const* command =
        std::get_if<BotRetrieveCommand>(&parsed))
    {
        HandleBotRetrieve(handler, *command);
        return true;
    }

    if (BotBagMoveCommand const* command =
        std::get_if<BotBagMoveCommand>(&parsed))
    {
        HandleBotBagMove(handler, *command);
        return true;
    }

    if (BotEquipCommand const* command =
        std::get_if<BotEquipCommand>(&parsed))
    {
        HandleBotEquip(handler, *command);
        return true;
    }

    if (BotUnequipCommand const* command =
        std::get_if<BotUnequipCommand>(&parsed))
    {
        HandleBotUnequip(handler, *command);
        return true;
    }

    if (QuestActionsCommand const* command =
        std::get_if<QuestActionsCommand>(&parsed))
    {
        HandleQuestActions(handler, *command);
        return true;
    }

    if (BotQuestPickupCommand const* command =
        std::get_if<BotQuestPickupCommand>(&parsed))
    {
        HandleBotQuestPickup(handler, *command);
        return true;
    }

    if (BotQuestTurninCommand const* command =
        std::get_if<BotQuestTurninCommand>(&parsed))
    {
        HandleBotQuestTurnin(handler, *command);
        return true;
    }

    if (TrainActionsCommand const* command =
        std::get_if<TrainActionsCommand>(&parsed))
    {
        HandleTrainActions(handler, *command);
        return true;
    }

    if (BotTrainSpellCommand const* command =
        std::get_if<BotTrainSpellCommand>(&parsed))
    {
        HandleBotTrainSpell(handler, *command);
        return true;
    }

    if (BotTrainAllCommand const* command =
        std::get_if<BotTrainAllCommand>(&parsed))
    {
        HandleBotTrainAll(handler, *command);
        return true;
    }

    if (QuestRewardsCommand const* command =
        std::get_if<QuestRewardsCommand>(&parsed))
    {
        HandleQuestRewards(handler, *command);
        return true;
    }

    if (QuestRewardModeSetCommand const* command =
        std::get_if<QuestRewardModeSetCommand>(&parsed))
    {
        HandleQuestRewardModeSet(handler, *command);
        return true;
    }

    if (BotRewardChoiceCommand const* command =
        std::get_if<BotRewardChoiceCommand>(&parsed))
    {
        HandleBotRewardChoice(handler, *command);
        return true;
    }

    if (BotModeSetCommand const* command =
        std::get_if<BotModeSetCommand>(&parsed))
    {
        HandleBotModeSet(handler, *command);
        return true;
    }

    return false;
}
} // namespace
} // namespace script
} // namespace living_world

class LivingWorldCommandScript final : public CommandScript
{
public:
    LivingWorldCommandScript() : CommandScript("LivingWorldCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        ChatCommandTable commandTable =
        {
            { "lw", HandleLivingWorldRootCommand, SEC_PLAYER, Console::No },
            { "lwbot", HandleLivingWorldCommand, SEC_PLAYER, Console::No }
        };

        return commandTable;
    }

private:
    static bool HandleLivingWorldRootCommand(ChatHandler* handler, char const* args)
    {
        std::string_view arguments =
            living_world::script::TrimRootWhitespace(args ? args : "");
        if (arguments.empty())
        {
            living_world::script::RenderUsage(handler);
            return true;
        }

        constexpr std::string_view loglevelToken = "loglevel";
        if (!arguments.starts_with(loglevelToken))
        {
            handler->PSendSysMessage(
                "LivingWorld command error: unknown subsystem ({})",
                std::string(arguments));
            living_world::script::RenderUsage(handler);
            return true;
        }

        arguments.remove_prefix(loglevelToken.size());
        arguments = living_world::script::TrimRootWhitespace(arguments);
        if (arguments.empty())
        {
            handler->PSendSysMessage(
                "LivingWorld command error: missing argument (loglevel requires 1-4)");
            return true;
        }

        std::uint64_t level = 0;
        auto const parseResult = std::from_chars(
            arguments.data(),
            arguments.data() + arguments.size(),
            level);
        if (parseResult.ec != std::errc{} ||
            parseResult.ptr != arguments.data() + arguments.size() ||
            level < living_world::script::DefaultPlayerChatLogLevel ||
            level > living_world::script::MaxPlayerChatLogLevel)
        {
            handler->PSendSysMessage(
                "LivingWorld command error: invalid argument (loglevel must be 1-4)");
            return true;
        }

        WorldSession* session = handler ? handler->GetSession() : nullptr;
        if (!session)
        {
            if (handler)
                handler->SendErrorMessage("LivingWorld loglevel requires an in-game session.");
            return true;
        }

        living_world::script::SetPlayerChatLogLevel(
            session->GetAccountId(),
            static_cast<std::uint8_t>(level));
        handler->PSendSysMessage(
            "LivingWorld chat log level set to {} ({}). Server logs still keep full trace.",
            static_cast<std::uint32_t>(level),
            living_world::script::DescribePlayerChatLogLevel(
                static_cast<std::uint8_t>(level)));
        return true;
    }

    static bool HandleLivingWorldCommand(ChatHandler* handler, char const* args)
    {
        return living_world::script::HandleParsedCommand(
            handler,
            living_world::script::ParseLivingWorldCommand(args ? args : ""));
    }
};

void AddSC_LivingWorldCommandScript()
{
    new LivingWorldCommandScript();
}
