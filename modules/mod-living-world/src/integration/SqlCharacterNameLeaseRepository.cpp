#include "integration/SqlCharacterNameLeaseRepository.h"

#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "World.h"
#include "WorldSessionMgr.h"

#include "StringFormat.h"

#include <optional>
#include <string>

namespace living_world
{
namespace integration
{
namespace
{
std::optional<std::string> LoadCharacterName(std::uint64_t characterGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name FROM characters WHERE guid = {} LIMIT 1",
        characterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    return result->Fetch()[0].Get<std::string>();
}

std::string QuoteSql(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}
} // namespace

bool SqlCharacterNameLeaseRepository::RestoreSourceNameLease(
    model::AccountAltRuntimeRecord const& runtime)
{
    if (runtime.sourceCharacterGuid == 0 || runtime.cloneCharacterGuid == 0 ||
        runtime.sourceCharacterName.empty() ||
        runtime.reservedSourceCharacterName.empty())
    {
        return false;
    }

    ObjectGuid sourceGuid =
        ObjectGuid::Create<HighGuid::Player>(runtime.sourceCharacterGuid);
    ObjectGuid cloneGuid =
        ObjectGuid::Create<HighGuid::Player>(runtime.cloneCharacterGuid);

    if (ObjectAccessor::FindConnectedPlayer(sourceGuid) ||
        sWorldSessionMgr->FindOfflineSessionForCharacterGUID(
            runtime.sourceCharacterGuid))
    {
        return false;
    }

    std::optional<std::string> sourceName =
        LoadCharacterName(runtime.sourceCharacterGuid);
    std::optional<std::string> cloneName =
        LoadCharacterName(runtime.cloneCharacterGuid);
    if (!sourceName || !cloneName)
    {
        return false;
    }

    if (*sourceName == runtime.sourceCharacterName &&
        *cloneName == runtime.reservedSourceCharacterName)
    {
        return true;
    }

    if (*sourceName != runtime.reservedSourceCharacterName ||
        *cloneName != runtime.sourceCharacterName)
    {
        return false;
    }

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "UPDATE characters SET name = {} WHERE guid = {}",
            QuoteSql(runtime.reservedSourceCharacterName),
            runtime.cloneCharacterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "UPDATE characters SET name = {} WHERE guid = {}",
            QuoteSql(runtime.sourceCharacterName),
            runtime.sourceCharacterGuid));

    // Both characters are leaving the lease swap with a clean, valid name.
    // Drop AT_LOGIN_RENAME / AT_LOGIN_FIRST so a future PlanSpawn doesn't see a
    // parked clone as "needs refresh" and force an unnecessary rematerialization
    // (and so the source can never be force-renamed by the client on next login).
    // 0x21 == AT_LOGIN_RENAME (0x01) | AT_LOGIN_FIRST (0x20)
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "UPDATE characters SET at_login = at_login & ~0x21 "
            "WHERE guid IN ({}, {})",
            runtime.cloneCharacterGuid,
            runtime.sourceCharacterGuid));

    if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM character_declinedname WHERE guid IN ({}, {})",
                runtime.sourceCharacterGuid,
                runtime.cloneCharacterGuid));
    }

    CharacterDatabase.CommitTransaction(trans);

    sCharacterCache->UpdateCharacterData(
        cloneGuid,
        runtime.reservedSourceCharacterName);
    sCharacterCache->UpdateCharacterData(
        sourceGuid,
        runtime.sourceCharacterName);
    return true;
}
} // namespace integration
} // namespace living_world
