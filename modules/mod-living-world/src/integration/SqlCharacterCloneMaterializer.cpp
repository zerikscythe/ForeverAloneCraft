#include "integration/SqlCharacterCloneMaterializer.h"

#include "DatabaseEnv.h"
#include "PlayerDump.h"

namespace living_world
{
namespace integration
{
namespace
{
std::string QuoteSql(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
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

    if (runtime.sourceCharacterGuid == 0 || runtime.cloneAccountId == 0)
    {
        result.reason = "runtime clone identity is incomplete";
        return result;
    }

    std::string cloneName = runtime.reservedSourceCharacterName.empty()
        ? runtime.cloneCharacterName
        : runtime.reservedSourceCharacterName;
    if (cloneName.empty())
    {
        result.reason = "clone name is missing";
        return result;
    }

    if (std::optional<CharacterCloneMaterializationResult> existing =
        LoadExistingClone(runtime.cloneAccountId, cloneName))
    {
        return *existing;
    }

    std::string dump;
    DumpReturn dumpStatus =
        PlayerDumpWriter().WriteDumpToString(dump, runtime.sourceCharacterGuid);
    if (dumpStatus != DUMP_SUCCESS)
    {
        result.reason = DumpErrorReason(dumpStatus);
        return result;
    }

    DumpReturn loadStatus = PlayerDumpReader().LoadDumpFromString(
        dump,
        runtime.cloneAccountId,
        cloneName,
        0);
    if (loadStatus != DUMP_SUCCESS)
    {
        result.reason = DumpErrorReason(loadStatus);
        return result;
    }

    std::optional<CharacterCloneMaterializationResult> created =
        LoadExistingClone(runtime.cloneAccountId, cloneName);
    if (!created)
    {
        result.reason = "clone import succeeded but clone lookup failed";
        return result;
    }

    created->reason = "materialized persistent clone character";
    return *created;
}
} // namespace integration
} // namespace living_world
