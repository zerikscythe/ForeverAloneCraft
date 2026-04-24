#include "integration/BotSessionFactory.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Log.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include "service/BotPlayerRegistry.h"

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
} // namespace integration
} // namespace living_world
