#include "integration/SqlBotShellRuntimeRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace
{
living_world::model::BotShellRuntimeRecord BuildShellRuntimeRecord(Field const* fields)
{
    living_world::model::BotShellRuntimeRecord record;
    record.identityId = fields[0].Get<std::uint32_t>();
    record.shellAccountId = fields[1].Get<std::uint32_t>();
    record.shellCharacterGuid = fields[2].Get<std::uint64_t>();
    record.isMaterialized = fields[3].Get<bool>();
    record.shellStateVersion = fields[4].Get<std::uint32_t>();
    record.leasedAt = fields[5].IsNull() ? "" : fields[5].Get<std::string>();
    record.lastSyncAt = fields[6].IsNull() ? "" : fields[6].Get<std::string>();
    record.lastDismissedAt = fields[7].IsNull() ? "" : fields[7].Get<std::string>();
    return record;
}
}

namespace living_world
{
namespace integration
{
void SqlBotShellRuntimeRepository::EnsureSchema() const
{
    CharacterDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_shell_runtime ("
        " identity_id INT UNSIGNED NOT NULL,"
        " shell_account_id INT UNSIGNED NOT NULL,"
        " shell_character_guid BIGINT UNSIGNED NOT NULL,"
        " is_materialized TINYINT(1) NOT NULL DEFAULT 0,"
        " shell_state_version INT UNSIGNED NOT NULL DEFAULT 0,"
        " leased_at DATETIME NULL,"
        " last_sync_at DATETIME NULL,"
        " last_dismissed_at DATETIME NULL,"
        " PRIMARY KEY (identity_id),"
        " UNIQUE KEY uq_shell (shell_account_id, shell_character_guid),"
        " KEY idx_materialized (is_materialized, leased_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

std::optional<model::BotShellRuntimeRecord>
SqlBotShellRuntimeRepository::FindByIdentity(std::uint32_t identityId) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT identity_id, shell_account_id, shell_character_guid, is_materialized, shell_state_version, "
        "leased_at, last_sync_at, last_dismissed_at "
        "FROM living_world_bot_shell_runtime WHERE identity_id = {}",
        identityId);
    if (!result)
        return std::nullopt;
    return BuildShellRuntimeRecord(result->Fetch());
}

std::optional<model::BotShellRuntimeRecord>
SqlBotShellRuntimeRepository::FindByShell(
    std::uint32_t shellAccountId,
    std::uint64_t shellCharacterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT identity_id, shell_account_id, shell_character_guid, is_materialized, shell_state_version, "
        "leased_at, last_sync_at, last_dismissed_at "
        "FROM living_world_bot_shell_runtime WHERE shell_account_id = {} AND shell_character_guid = {}",
        shellAccountId,
        shellCharacterGuid);
    if (!result)
        return std::nullopt;
    return BuildShellRuntimeRecord(result->Fetch());
}

void SqlBotShellRuntimeRepository::Upsert(
    model::BotShellRuntimeRecord const& record) const
{
    CharacterDatabase.Execute(
        "REPLACE INTO living_world_bot_shell_runtime "
        "(identity_id, shell_account_id, shell_character_guid, is_materialized, shell_state_version, leased_at, last_sync_at, last_dismissed_at) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {})",
        record.identityId,
        record.shellAccountId,
        record.shellCharacterGuid,
        record.isMaterialized ? 1u : 0u,
        record.shellStateVersion,
        record.leasedAt.empty() ? std::string("NULL") : "'" + record.leasedAt + "'",
        record.lastSyncAt.empty() ? std::string("NULL") : "'" + record.lastSyncAt + "'",
        record.lastDismissedAt.empty() ? std::string("NULL") : "'" + record.lastDismissedAt + "'");
}

void SqlBotShellRuntimeRepository::RemoveByIdentity(std::uint32_t identityId) const
{
    CharacterDatabase.Execute(
        "DELETE FROM living_world_bot_shell_runtime WHERE identity_id = {}",
        identityId);
}
} // namespace integration
} // namespace living_world
