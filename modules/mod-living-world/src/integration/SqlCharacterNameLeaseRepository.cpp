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
