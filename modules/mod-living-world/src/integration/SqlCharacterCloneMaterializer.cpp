#include "integration/SqlCharacterCloneMaterializer.h"

#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "PlayerDump.h"
#include "World.h"
#include "WorldSessionMgr.h"

namespace living_world
{
namespace integration
{
namespace
{
struct CharacterNameLeaseState
{
    std::string currentSourceName;
    bool sourceOwnsLiveName = false;
    bool sourceAlreadyParked = false;
};

std::string QuoteSql(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::optional<CharacterNameLeaseState> LoadNameLeaseState(
    model::AccountAltRuntimeRecord const& runtime)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name FROM characters WHERE guid = {} LIMIT 1",
        runtime.sourceCharacterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    CharacterNameLeaseState state;
    state.currentSourceName = result->Fetch()[0].Get<std::string>();
    state.sourceOwnsLiveName =
        state.currentSourceName == runtime.sourceCharacterName;
    state.sourceAlreadyParked =
        state.currentSourceName == runtime.reservedSourceCharacterName;
    return state;
}

std::optional<CharacterCloneMaterializationResult> LoadExistingClone(
    std::uint32_t accountId,
    std::string const& cloneName)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT guid, name, level, xp, money FROM characters "
        "WHERE account = {} AND name = {} LIMIT 1",
        accountId,
        QuoteSql(cloneName));
    if (!result)
    {
        return std::nullopt;
    }

    Field const* fields = result->Fetch();

    CharacterCloneMaterializationResult clone;
    clone.succeeded = true;
    clone.cloneCharacterGuid = fields[0].Get<std::uint64_t>();
    clone.cloneCharacterName = fields[1].Get<std::string>();
    clone.cloneSnapshot.level = fields[2].Get<std::uint8_t>();
    clone.cloneSnapshot.experience = fields[3].Get<std::uint32_t>();
    clone.cloneSnapshot.money = fields[4].Get<std::uint32_t>();
    clone.reason = "reused existing clone character";
    return clone;
}

bool EnsureSourceNameLeased(
    model::AccountAltRuntimeRecord const& runtime,
    std::string& reason)
{
    if (runtime.sourceCharacterGuid == 0)
    {
        reason = "source character guid is missing";
        return false;
    }

    if (runtime.sourceCharacterName.empty())
    {
        reason = "source character name is missing";
        return false;
    }

    if (runtime.reservedSourceCharacterName.empty())
    {
        reason = "reserved parked name is missing";
        return false;
    }

    if (ObjectMgr::CheckPlayerName(runtime.reservedSourceCharacterName, true) !=
        CHAR_NAME_SUCCESS)
    {
        reason = "reserved parked name is invalid";
        return false;
    }

    std::optional<CharacterNameLeaseState> state = LoadNameLeaseState(runtime);
    if (!state)
    {
        reason = "source character could not be found for name leasing";
        return false;
    }

    if (state->sourceAlreadyParked)
    {
        return true;
    }

    if (!state->sourceOwnsLiveName)
    {
        reason = "source character is not in an expected leaseable name state";
        return false;
    }

    ObjectGuid sourceGuid =
        ObjectGuid::Create<HighGuid::Player>(runtime.sourceCharacterGuid);
    if (ObjectAccessor::FindConnectedPlayer(sourceGuid) ||
        sWorldSessionMgr->FindOfflineSessionForCharacterGUID(
            runtime.sourceCharacterGuid))
    {
        reason = "source character is still loaded; cannot lease name";
        return false;
    }

    if (ObjectGuid parkedGuid =
            sCharacterCache->GetCharacterGuidByName(
                runtime.reservedSourceCharacterName);
        parkedGuid && parkedGuid != sourceGuid)
    {
        reason = "reserved parked name is already in use";
        return false;
    }

    CharacterDatabase.Execute(
        "UPDATE characters SET name = {} WHERE guid = {}",
        QuoteSql(runtime.reservedSourceCharacterName),
        runtime.sourceCharacterGuid);

    if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
    {
        CharacterDatabase.Execute(
            "DELETE FROM character_declinedname WHERE guid = {}",
            runtime.sourceCharacterGuid);
    }

    sCharacterCache->UpdateCharacterData(
        sourceGuid,
        runtime.reservedSourceCharacterName);

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] MaterializeClone leased source name runtimeId={} "
        "sourceGuid={} parkedName='{}' liveCloneName='{}'",
        runtime.runtimeId,
        runtime.sourceCharacterGuid,
        runtime.reservedSourceCharacterName,
        runtime.sourceCharacterName);
    return true;
}

std::string DumpErrorReason(DumpReturn status)
{
    switch (status)
    {
        case DUMP_SUCCESS:
            return "clone dump imported";
        case DUMP_FILE_OPEN_ERROR:
            return "clone dump buffer could not be opened";
        case DUMP_TOO_MANY_CHARS:
            return "bot account has too many characters";
        case DUMP_FILE_BROKEN:
            return "clone dump import failed";
        case DUMP_CHARACTER_DELETED:
            return "source character could not be dumped";
    }

    return "unknown clone materialization failure";
}
} // namespace

CharacterCloneMaterializationResult
SqlCharacterCloneMaterializer::MaterializeClone(
    model::AccountAltRuntimeRecord const& runtime)
{
    CharacterCloneMaterializationResult result;

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] MaterializeClone start runtimeId={} sourceGuid={} "
        "cloneAccountId={} reservedName='{}' cloneName='{}'",
        runtime.runtimeId,
        runtime.sourceCharacterGuid,
        runtime.cloneAccountId,
        runtime.reservedSourceCharacterName,
        runtime.cloneCharacterName);

    if (runtime.sourceCharacterGuid == 0 || runtime.cloneAccountId == 0)
    {
        result.reason = "runtime clone identity is incomplete";
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone blocked runtimeId={} reason='{}'",
            runtime.runtimeId,
            result.reason);
        return result;
    }

    std::string cloneName = runtime.cloneCharacterName.empty()
        ? runtime.sourceCharacterName
        : runtime.cloneCharacterName;
    if (cloneName.empty())
    {
        result.reason = "clone name is missing";
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone blocked runtimeId={} reason='{}'",
            runtime.runtimeId,
            result.reason);
        return result;
    }

    if (std::optional<CharacterCloneMaterializationResult> existing =
        LoadExistingClone(runtime.cloneAccountId, cloneName))
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone reused existing clone "
            "runtimeId={} cloneGuid={} cloneName='{}' level={} xp={} money={}",
            runtime.runtimeId,
            existing->cloneCharacterGuid,
            existing->cloneCharacterName,
            static_cast<std::uint32_t>(existing->cloneSnapshot.level),
            existing->cloneSnapshot.experience,
            existing->cloneSnapshot.money);
        return *existing;
    }

    if (!runtime.reservedSourceCharacterName.empty())
    {
        if (std::optional<CharacterCloneMaterializationResult> legacyClone =
                LoadExistingClone(
                    runtime.cloneAccountId,
                    runtime.reservedSourceCharacterName))
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] MaterializeClone reused legacy hidden-name "
                "clone runtimeId={} cloneGuid={} cloneName='{}'",
                runtime.runtimeId,
                legacyClone->cloneCharacterGuid,
                legacyClone->cloneCharacterName);
            return *legacyClone;
        }
    }

    if (!EnsureSourceNameLeased(runtime, result.reason))
    {
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone lease failed runtimeId={} "
            "sourceGuid={} reason='{}'",
            runtime.runtimeId,
            runtime.sourceCharacterGuid,
            result.reason);
        return result;
    }

    std::string dump;
    DumpReturn dumpStatus =
        PlayerDumpWriter().WriteDumpToString(dump, runtime.sourceCharacterGuid);
    if (dumpStatus != DUMP_SUCCESS)
    {
        result.reason = DumpErrorReason(dumpStatus);
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone dump export failed "
            "runtimeId={} sourceGuid={} reason='{}'",
            runtime.runtimeId,
            runtime.sourceCharacterGuid,
            result.reason);
        return result;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] MaterializeClone dump export succeeded "
        "runtimeId={} sourceGuid={} bytes={} targetAccountId={} targetName='{}'",
        runtime.runtimeId,
        runtime.sourceCharacterGuid,
        dump.size(),
        runtime.cloneAccountId,
        cloneName);

    DumpReturn loadStatus = PlayerDumpReader().LoadDumpFromString(
        dump,
        runtime.cloneAccountId,
        cloneName,
        0);
    if (loadStatus != DUMP_SUCCESS)
    {
        result.reason = DumpErrorReason(loadStatus);
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone dump import failed "
            "runtimeId={} targetAccountId={} targetName='{}' reason='{}'",
            runtime.runtimeId,
            runtime.cloneAccountId,
            cloneName,
            result.reason);
        return result;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] MaterializeClone dump import succeeded "
        "runtimeId={} targetAccountId={} targetName='{}'",
        runtime.runtimeId,
        runtime.cloneAccountId,
        cloneName);

    std::optional<CharacterCloneMaterializationResult> created =
        LoadExistingClone(runtime.cloneAccountId, cloneName);
    if (!created)
    {
        result.reason = "clone import succeeded but clone lookup failed";
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] MaterializeClone post-import lookup failed "
            "runtimeId={} targetAccountId={} targetName='{}'",
            runtime.runtimeId,
            runtime.cloneAccountId,
            cloneName);
        return result;
    }

    created->reason = "materialized persistent clone character";
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] MaterializeClone created clone runtimeId={} "
        "cloneGuid={} cloneName='{}' level={} xp={} money={}",
        runtime.runtimeId,
        created->cloneCharacterGuid,
        created->cloneCharacterName,
        static_cast<std::uint32_t>(created->cloneSnapshot.level),
        created->cloneSnapshot.experience,
        created->cloneSnapshot.money);
    return *created;
}
} // namespace integration
} // namespace living_world
