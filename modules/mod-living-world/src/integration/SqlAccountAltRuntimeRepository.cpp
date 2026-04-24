#include "integration/SqlAccountAltRuntimeRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

#include <string>

namespace living_world
{
namespace integration
{
namespace
{
std::uint32_t ToDbState(model::AccountAltRuntimeState state)
{
    return static_cast<std::uint32_t>(state);
}

model::AccountAltRuntimeState FromDbState(std::uint8_t state)
{
    switch (state)
    {
        case 0:
            return model::AccountAltRuntimeState::PreparingClone;
        case 1:
            return model::AccountAltRuntimeState::Active;
        case 2:
            return model::AccountAltRuntimeState::SyncingBack;
        case 3:
            return model::AccountAltRuntimeState::Recovering;
        case 4:
            return model::AccountAltRuntimeState::Failed;
        case 5:
            return model::AccountAltRuntimeState::SyncingEquipment;
        default:
            return model::AccountAltRuntimeState::Failed;
    }
}

std::string QuoteSql(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::string NullableGuid(std::uint64_t guid)
{
    return guid == 0 ? "NULL" : std::to_string(guid);
}

model::AccountAltRuntimeRecord BuildRecord(Field const* fields)
{
    model::AccountAltRuntimeRecord runtime;
    runtime.runtimeId = fields[0].Get<std::uint64_t>();
    runtime.sourceAccountId = fields[1].Get<std::uint32_t>();
    runtime.sourceCharacterGuid = fields[2].Get<std::uint64_t>();
    runtime.ownerCharacterGuid = fields[3].Get<std::uint64_t>();
    runtime.cloneAccountId = fields[4].Get<std::uint32_t>();
    runtime.cloneCharacterGuid = fields[5].Get<std::uint64_t>();
    runtime.state = FromDbState(fields[6].Get<std::uint8_t>());
    runtime.sourceCharacterName = fields[7].Get<std::string>();
    runtime.reservedSourceCharacterName = fields[8].Get<std::string>();
    runtime.cloneCharacterName = fields[9].Get<std::string>();
    runtime.sourceSnapshot.level = fields[10].Get<std::uint8_t>();
    runtime.sourceSnapshot.experience = fields[11].Get<std::uint32_t>();
    runtime.sourceSnapshot.money = fields[12].Get<std::uint32_t>();
    runtime.cloneSnapshot.level = fields[13].Get<std::uint8_t>();
    runtime.cloneSnapshot.experience = fields[14].Get<std::uint32_t>();
    runtime.cloneSnapshot.money = fields[15].Get<std::uint32_t>();
    return runtime;
}

char const* RuntimeSelectColumns =
    "runtime_id, source_account_id, source_character_guid, "
    "COALESCE(owner_character_guid, 0), clone_account_id, "
    "COALESCE(clone_character_guid, 0), state, source_character_name, "
    "reserved_source_character_name, clone_character_name, source_level, "
    "source_experience, source_money, clone_level, clone_experience, "
    "clone_money";
} // namespace

std::optional<model::AccountAltRuntimeRecord>
SqlAccountAltRuntimeRepository::FindBySourceCharacter(
    std::uint32_t sourceAccountId,
    std::uint64_t sourceCharacterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT {} FROM living_world_account_alt_runtime "
        "WHERE source_account_id = {} AND source_character_guid = {} "
        "LIMIT 1",
        RuntimeSelectColumns,
        sourceAccountId,
        sourceCharacterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    return BuildRecord(result->Fetch());
}

std::vector<model::AccountAltRuntimeRecord>
SqlAccountAltRuntimeRepository::ListRecoverableForAccount(
    std::uint32_t sourceAccountId) const
{
    std::vector<model::AccountAltRuntimeRecord> runtimes;
    QueryResult result = CharacterDatabase.Query(
        "SELECT {} FROM living_world_account_alt_runtime "
        "WHERE source_account_id = {} "
        "AND state IN (0, 1, 2, 3) "
        "ORDER BY updated_at ASC",
        RuntimeSelectColumns,
        sourceAccountId);
    if (!result)
    {
        return runtimes;
    }

    do
    {
        runtimes.push_back(BuildRecord(result->Fetch()));
    } while (result->NextRow());

    return runtimes;
}

void SqlAccountAltRuntimeRepository::SaveRuntime(
    model::AccountAltRuntimeRecord const& runtime)
{
    CharacterDatabase.Execute(
        "INSERT INTO living_world_account_alt_runtime ("
        "source_account_id, source_character_guid, owner_character_guid, "
        "clone_account_id, clone_character_guid, state, "
        "source_character_name, reserved_source_character_name, "
        "clone_character_name, source_level, source_experience, "
        "source_money, clone_level, clone_experience, clone_money) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "owner_character_guid = VALUES(owner_character_guid), "
        "clone_account_id = VALUES(clone_account_id), "
        "clone_character_guid = VALUES(clone_character_guid), "
        "state = VALUES(state), "
        "source_character_name = VALUES(source_character_name), "
        "reserved_source_character_name = VALUES(reserved_source_character_name), "
        "clone_character_name = VALUES(clone_character_name), "
        "source_level = VALUES(source_level), "
        "source_experience = VALUES(source_experience), "
        "source_money = VALUES(source_money), "
        "clone_level = VALUES(clone_level), "
        "clone_experience = VALUES(clone_experience), "
        "clone_money = VALUES(clone_money)",
        runtime.sourceAccountId,
        runtime.sourceCharacterGuid,
        NullableGuid(runtime.ownerCharacterGuid),
        runtime.cloneAccountId,
        NullableGuid(runtime.cloneCharacterGuid),
        ToDbState(runtime.state),
        QuoteSql(runtime.sourceCharacterName),
        QuoteSql(runtime.reservedSourceCharacterName),
        QuoteSql(runtime.cloneCharacterName),
        static_cast<std::uint32_t>(runtime.sourceSnapshot.level),
        runtime.sourceSnapshot.experience,
        runtime.sourceSnapshot.money,
        static_cast<std::uint32_t>(runtime.cloneSnapshot.level),
        runtime.cloneSnapshot.experience,
        runtime.cloneSnapshot.money);
}
} // namespace integration
} // namespace living_world
