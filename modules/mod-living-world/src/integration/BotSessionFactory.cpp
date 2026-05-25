#include "integration/BotSessionFactory.h"

#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QueryResult.h"
#include "Log.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include "integration/SqlBotIdentityRepository.h"
#include "integration/SqlBotShellRuntimeRepository.h"
#include "model/BotRuntimeKind.h"
#include "service/BotLedgerShellHydratorService.h"
#include "service/BotPlayerRegistry.h"

#include <cmath>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace living_world
{
namespace integration
{
namespace
{
struct BotAccountInfo
{
    std::uint32_t id = 0;
    std::string name;
    std::uint32_t flags = 0;
    std::uint8_t expansion = 0;
    int64 muteTime = 0;
    LocaleConstant locale = LOCALE_enUS;
    std::uint32_t recruiter = 0;
    AccountTypes security = SEC_PLAYER;
    std::uint32_t totalTime = 0;
};

struct LedgerShellBinding
{
    std::uint32_t accountId = 0;
    std::uint64_t characterGuid = 0;
    bool leasedThisCall = false;
};

std::optional<std::uint32_t> ReserveBotAccount(ObjectGuid ownerCharacterGuid)
{
    QueryResult result = LoginDatabase.Query(
        "SELECT account_id FROM living_world_bot_account_pool "
        "WHERE is_available = 1 LIMIT 1");
    if (!result)
    {
        return std::nullopt;
    }

    std::uint32_t const accountId = (*result)[0].Get<std::uint32_t>();
    LoginDatabase.Execute(
        "UPDATE living_world_bot_account_pool "
        "SET is_available = 0, reserved_for = {} WHERE account_id = {};",
        ownerCharacterGuid.GetCounter(),
        accountId);
    return accountId;
}

void MarkBotAccountReserved(std::uint32_t accountId, std::uint64_t reservedFor)
{
    LoginDatabase.Execute(
        "UPDATE living_world_bot_account_pool "
        "SET is_available = 0, reserved_for = {} WHERE account_id = {};",
        reservedFor,
        accountId);
}

void ReleaseBotAccountReservation(std::uint32_t accountId)
{
    LoginDatabase.Execute(
        "UPDATE living_world_bot_account_pool "
        "SET is_available = 1, reserved_for = NULL WHERE account_id = {};",
        accountId);
}

std::optional<BotAccountInfo> LoadBotAccountInfo(std::uint32_t accountId)
{
    QueryResult result = LoginDatabase.Query(
        "SELECT a.id, a.username, a.Flags, a.expansion, a.mutetime, "
        "a.locale, a.recruiter, a.totaltime, COALESCE(MAX(aa.gmlevel), 0) "
        "FROM account a "
        "LEFT JOIN account_access aa ON aa.id = a.id AND aa.RealmID IN (-1, 0) "
        "WHERE a.id = {} "
        "GROUP BY a.id, a.username, a.Flags, a.expansion, a.mutetime, "
        "a.locale, a.recruiter, a.totaltime",
        accountId);
    if (!result)
    {
        return std::nullopt;
    }

    Field const* fields = result->Fetch();
    BotAccountInfo info;
    info.id = fields[0].Get<std::uint32_t>();
    info.name = fields[1].Get<std::string>();
    info.flags = fields[2].Get<std::uint32_t>();
    info.expansion = fields[3].Get<std::uint8_t>();
    info.muteTime = fields[4].Get<int64>();
    info.locale = LocaleConstant(fields[5].Get<std::uint8_t>());
    info.recruiter = fields[6].Get<std::uint32_t>();
    info.totalTime = fields[7].Get<std::uint32_t>();
    info.security = AccountTypes(fields[8].Get<std::uint8_t>());
    return info;
}

std::optional<std::uint64_t> LoadShellCharacterGuidForAccount(std::uint32_t accountId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT guid FROM characters WHERE account = {} ORDER BY guid ASC LIMIT 1",
        accountId);
    if (!result)
        return std::nullopt;
    return (*result)[0].Get<std::uint64_t>();
}

std::optional<LedgerShellBinding> ResolveLedgerShellBinding(
    std::uint32_t identityId)
{
    integration::SqlBotShellRuntimeRepository shellRuntimeRepo;
    if (std::optional<model::BotShellRuntimeRecord> runtime =
            shellRuntimeRepo.FindByIdentity(identityId))
    {
        if (runtime->shellAccountId != 0 && runtime->shellCharacterGuid != 0)
            return LedgerShellBinding{
                runtime->shellAccountId,
                runtime->shellCharacterGuid,
                false };
    }

    integration::SqlBotIdentityRepository identityRepo;
    std::optional<integration::BotIdentityRecord> identity =
        identityRepo.FindById(identityId);
    if (!identity || identity->shellAccountId == 0 || identity->shellCharacterGuid == 0)
        return std::nullopt;

    return LedgerShellBinding{
        identity->shellAccountId,
        identity->shellCharacterGuid,
        false };
}

std::optional<LedgerShellBinding> LeaseLedgerShellBinding(std::uint32_t identityId)
{
    integration::SqlBotIdentityRepository identityRepo;
    std::optional<integration::BotIdentityRecord> identity = identityRepo.FindById(identityId);
    if (!identity)
        return std::nullopt;

    integration::SqlBotShellRuntimeRepository shellRuntimeRepo;
    QueryResult availableAccounts = LoginDatabase.Query(
        "SELECT p.account_id "
        "FROM living_world_bot_account_pool p "
        "LEFT JOIN account a ON a.id = p.account_id "
        "WHERE p.is_enabled = 1 AND p.is_available = 1 "
        "AND a.username LIKE 'LedRes\\_%' ESCAPE '\\' "
        "ORDER BY p.account_id ASC");
    if (!availableAccounts)
        return std::nullopt;

    do
    {
        Field const* fields = availableAccounts->Fetch();
        if (!fields)
            continue;
        std::uint32_t const accountId = fields[0].Get<std::uint32_t>();
        std::optional<std::uint64_t> characterGuid = LoadShellCharacterGuidForAccount(accountId);
        if (!characterGuid || *characterGuid == 0)
            continue;

        if (shellRuntimeRepo.FindByShell(accountId, *characterGuid))
            continue;

        if (CharacterDatabase.Query(
                "SELECT id FROM living_world_bot_identity "
                "WHERE shell_account_id = {} AND shell_character_guid = {} LIMIT 1",
                accountId,
                *characterGuid))
        {
            continue;
        }

        std::uint32_t const nextShellStateVersion =
            std::max<std::uint32_t>(1u, identity->shellStateVersion + 1u);
        MarkBotAccountReserved(accountId, identityId);
        identityRepo.UpdateShellState(
            identityId,
            accountId,
            *characterGuid,
            nextShellStateVersion,
            "rehydrate");

        model::BotShellRuntimeRecord runtimeRecord;
        runtimeRecord.identityId = identityId;
        runtimeRecord.shellAccountId = accountId;
        runtimeRecord.shellCharacterGuid = *characterGuid;
        runtimeRecord.isMaterialized = false;
        runtimeRecord.shellStateVersion = nextShellStateVersion;
        shellRuntimeRepo.Upsert(runtimeRecord);
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_shell_runtime "
            "SET leased_at = NOW(), last_sync_at = NULL, last_dismissed_at = NULL "
            "WHERE identity_id = {}",
            identityId);

        return LedgerShellBinding{ accountId, *characterGuid, true };
    } while (availableAccounts->NextRow());

    return std::nullopt;
}

void ReleaseLedgerShellBinding(std::uint32_t identityId, LedgerShellBinding const& binding)
{
    integration::SqlBotIdentityRepository identityRepo;
    integration::SqlBotShellRuntimeRepository shellRuntimeRepo;

    identityRepo.UpdateShellState(identityId, 0, 0, 0, "");
    shellRuntimeRepo.RemoveByIdentity(identityId);
    if (binding.accountId != 0)
        ReleaseBotAccountReservation(binding.accountId);
}
} // namespace

BotSessionSpawnResult BotSessionFactory::SpawnBotPlayer(
    ObjectGuid characterGuid,
    ObjectGuid ownerCharacterGuid)
{
    BotSessionSpawnResult result;
    if (!characterGuid.IsPlayer() || !ownerCharacterGuid.IsPlayer())
    {
        result.status = BotSessionSpawnStatus::InvalidCharacterGuid;
        return result;
    }

    std::optional<std::uint32_t> accountId =
        ReserveBotAccount(ownerCharacterGuid);
    if (!accountId)
    {
        result.status = BotSessionSpawnStatus::NoAvailableBotAccount;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] SpawnBotPlayer failed to reserve bot account "
            "for ownerGuid={} characterGuid={}",
            ownerCharacterGuid.GetCounter(),
            characterGuid.GetCounter());
        return result;
    }

    std::optional<BotAccountInfo> account = LoadBotAccountInfo(*accountId);
    if (!account)
    {
        result.status = BotSessionSpawnStatus::BotAccountNotFound;
        result.botAccountId = *accountId;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] SpawnBotPlayer reserved bot account {} but "
            "account lookup failed for ownerGuid={} characterGuid={}",
            *accountId,
            ownerCharacterGuid.GetCounter(),
            characterGuid.GetCounter());
        return result;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] SpawnBotPlayer reserved bot account {} ('{}') "
        "for ownerGuid={} characterGuid={}",
        account->id,
        account->name,
        ownerCharacterGuid.GetCounter(),
        characterGuid.GetCounter());

    return SpawnBotPlayerOnAccount(
        account->id,
        characterGuid,
        ownerCharacterGuid);
}

BotSessionSpawnResult BotSessionFactory::SpawnBotPlayerOnAccount(
    std::uint32_t botAccountId,
    ObjectGuid characterGuid,
    ObjectGuid ownerCharacterGuid)
{
    BotSessionSpawnResult result;
    if (!characterGuid.IsPlayer() || !ownerCharacterGuid.IsPlayer())
    {
        result.status = BotSessionSpawnStatus::InvalidCharacterGuid;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] SpawnBotPlayerOnAccount invalid guids "
            "botAccountId={} ownerGuid={} characterGuid={}",
            botAccountId,
            ownerCharacterGuid.GetCounter(),
            characterGuid.GetCounter());
        return result;
    }

    std::optional<BotAccountInfo> account = LoadBotAccountInfo(botAccountId);
    if (!account)
    {
        result.status = BotSessionSpawnStatus::BotAccountNotFound;
        result.botAccountId = botAccountId;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] SpawnBotPlayerOnAccount failed account lookup "
            "botAccountId={} ownerGuid={} characterGuid={}",
            botAccountId,
            ownerCharacterGuid.GetCounter(),
            characterGuid.GetCounter());
        return result;
    }

    std::string accountName = account->name;
    auto session = new WorldSession(
        account->id,
        std::move(account->name),
        account->flags,
        nullptr,
        account->security,
        account->expansion,
        account->muteTime,
        account->locale,
        account->recruiter,
        false,
        true,
        account->totalTime);
    session->EnableBotMode();
    session->SetBotLoginTarget(characterGuid);

    service::BotPlayerRegistry::Instance().RegisterPendingOwner(
        characterGuid,
        ownerCharacterGuid);

    // Write the owner's current position into the bot character's DB row before
    // the session loads character data. TeleportTo does not work for null-socket
    // bots (it requires a client ack), so pre-seeding the DB is the only
    // reliable way to start the bot near the owner. DirectExecute blocks until
    // the row is committed, guaranteeing the session reads the updated position.
    if (Player* owner = ObjectAccessor::FindPlayer(ownerCharacterGuid))
    {
        float const behindAngle = owner->GetOrientation() + 3.14159265358979323846f;
        CharacterDatabase.DirectExecute(
            "UPDATE characters SET map={}, position_x={}, position_y={}, "
            "position_z={}, orientation={} WHERE guid={}",
            owner->GetMapId(),
            owner->GetPositionX() + 2.0f * std::cos(behindAngle),
            owner->GetPositionY() + 2.0f * std::sin(behindAngle),
            owner->GetPositionZ(),
            owner->GetOrientation(),
            characterGuid.GetCounter());
    }

    result.status = BotSessionSpawnStatus::SpawnQueued;
    result.botAccountId = account->id;
    result.botAccountName = accountName;
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] SpawnBotPlayerOnAccount queueing session "
        "botAccountId={} botAccountName='{}' ownerGuid={} characterGuid={}",
        account->id,
        accountName,
        ownerCharacterGuid.GetCounter(),
        characterGuid.GetCounter());
    sWorldSessionMgr->AddSession(session);
    return result;
}

BotSessionSpawnResult BotSessionFactory::SpawnHostileBotPlayerOnAccount(
    std::uint32_t botAccountId,
    ObjectGuid characterGuid)
{
    BotSessionSpawnResult result;
    if (!characterGuid.IsPlayer())
    {
        result.status = BotSessionSpawnStatus::InvalidCharacterGuid;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorld] SpawnHostileBotPlayerOnAccount invalid guid "
            "botAccountId={} characterGuid={}",
            botAccountId,
            characterGuid.GetCounter());
        return result;
    }

    std::optional<BotAccountInfo> account = LoadBotAccountInfo(botAccountId);
    if (!account)
    {
        result.status = BotSessionSpawnStatus::BotAccountNotFound;
        result.botAccountId = botAccountId;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorld] SpawnHostileBotPlayerOnAccount failed account lookup "
            "botAccountId={} characterGuid={}",
            botAccountId,
            characterGuid.GetCounter());
        return result;
    }

    std::string accountName = account->name;
    auto session = new WorldSession(
        account->id,
        std::move(account->name),
        account->flags,
        nullptr,
        account->security,
        account->expansion,
        account->muteTime,
        account->locale,
        account->recruiter,
        false,
        true,
        account->totalTime);
    session->EnableBotMode();
    session->SetBotLoginTarget(characterGuid);

    // Register with ObjectGuid::Empty as the owner sentinel so OnPlayerLogin
    // recognises this as an ownerless hostile bot and schedules HostileCompanionAI.
    service::BotPlayerRegistry::Instance().RegisterPendingBot(
        characterGuid,
        ObjectGuid::Empty,
        model::BotRuntimeKind::Hostile);

    // No DB position seeding — hostile bots spawn wherever their character
    // record currently places them (set up by the admin at bot creation time).

    result.status = BotSessionSpawnStatus::SpawnQueued;
    result.botAccountId = account->id;
    result.botAccountName = accountName;
    LOG_INFO(
        "server.worldserver",
        "[LivingWorld] SpawnHostileBotPlayerOnAccount queuing session "
        "botAccountId={} botAccountName='{}' characterGuid={}",
        account->id,
        accountName,
        characterGuid.GetCounter());
    sWorldSessionMgr->AddSession(session);
    return result;
}

BotSessionSpawnResult BotSessionFactory::SpawnLedgerShellIdentity(
    std::uint32_t identityId)
{
    BotSessionSpawnResult result;

    std::optional<LedgerShellBinding> binding =
        ResolveLedgerShellBinding(identityId);
    if (!binding)
    {
        binding = LeaseLedgerShellBinding(identityId);
        if (!binding)
        {
            result.status = BotSessionSpawnStatus::NoAssignedShell;
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorldDebug] SpawnLedgerShellIdentity identityId={} failed: no available shell lease.",
                identityId);
            return result;
        }
    }

    std::string hydrateFailureReason;
    service::BotLedgerShellHydratorService hydrator;
    if (!hydrator.RehydrateIdentity(
            identityId,
            binding->accountId,
            binding->characterGuid,
            &hydrateFailureReason))
    {
        result.status = BotSessionSpawnStatus::PreparationFailed;
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] SpawnLedgerShellIdentity identityId={} failed during rehydrate: {}.",
            identityId,
            hydrateFailureReason);
        if (binding->leasedThisCall)
            ReleaseLedgerShellBinding(identityId, *binding);
        return result;
    }

    std::uint32_t const botAccountId = binding->accountId;
    ObjectGuid const characterGuid =
        ObjectGuid::Create<HighGuid::Player>(binding->characterGuid);
    if (!characterGuid.IsPlayer())
    {
        result.status = BotSessionSpawnStatus::InvalidCharacterGuid;
        if (binding->leasedThisCall)
            ReleaseLedgerShellBinding(identityId, *binding);
        return result;
    }

    std::optional<BotAccountInfo> account = LoadBotAccountInfo(botAccountId);
    if (!account)
    {
        result.status = BotSessionSpawnStatus::BotAccountNotFound;
        result.botAccountId = botAccountId;
        if (binding->leasedThisCall)
            ReleaseLedgerShellBinding(identityId, *binding);
        return result;
    }

    MarkBotAccountReserved(botAccountId, identityId);

    std::string accountName = account->name;
    auto session = new WorldSession(
        account->id,
        std::move(account->name),
        account->flags,
        nullptr,
        account->security,
        account->expansion,
        account->muteTime,
        account->locale,
        account->recruiter,
        false,
        true,
        account->totalTime);
    session->EnableBotMode();
    session->SetBotLoginTarget(characterGuid);

    service::BotPlayerRegistry::Instance().RegisterPendingBot(
        characterGuid,
        ObjectGuid::Empty,
        model::BotRuntimeKind::LedgerShell);

    result.status = BotSessionSpawnStatus::SpawnQueued;
    result.botAccountId = account->id;
    result.botAccountName = accountName;
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] SpawnLedgerShellIdentity queued identityId={} botAccountId={} botAccountName='{}' characterGuid={}",
        identityId,
        account->id,
        accountName,
        characterGuid.GetCounter());
    sWorldSessionMgr->AddSession(session);
    return result;
}
} // namespace integration
} // namespace living_world
