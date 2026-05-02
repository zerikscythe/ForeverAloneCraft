#include "integration/SqlBotAccountPoolRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
std::optional<model::BotAccountLease>
SqlBotAccountPoolRepository::ReserveAccountForSource(
    std::uint32_t sourceAccountId,
    std::uint64_t sourceCharacterGuid)
{
    QueryResult reserved = LoginDatabase.Query(
        "SELECT p.account_id, a.username "
        "FROM living_world_bot_account_pool p "
        "INNER JOIN account a ON a.id = p.account_id "
        "WHERE p.is_enabled = 1 "
        "AND p.assigned_source_account_id = {} "
        "AND p.assigned_source_character_guid = {} "
        "LIMIT 1",
        sourceAccountId,
        sourceCharacterGuid);
    if (reserved)
    {
        Field const* fields = reserved->Fetch();
        model::BotAccountLease lease;
        lease.accountId = fields[0].Get<std::uint32_t>();
        lease.accountName = fields[1].Get<std::string>();
        return lease;
    }

    LoginDatabase.Execute(
        "UPDATE living_world_bot_account_pool "
        "SET assigned_source_account_id = {}, "
        "assigned_source_character_guid = {} "
        "WHERE is_enabled = 1 "
        "AND assigned_source_account_id IS NULL "
        "AND assigned_source_character_guid IS NULL "
        "ORDER BY account_id ASC "
        "LIMIT 1",
        sourceAccountId,
        sourceCharacterGuid);

    QueryResult result = LoginDatabase.Query(
        "SELECT p.account_id, a.username "
        "FROM living_world_bot_account_pool p "
        "INNER JOIN account a ON a.id = p.account_id "
        "WHERE p.is_enabled = 1 "
        "AND p.assigned_source_account_id = {} "
        "AND p.assigned_source_character_guid = {} "
        "LIMIT 1",
        sourceAccountId,
        sourceCharacterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    Field const* fields = result->Fetch();
    model::BotAccountLease lease;
    lease.accountId = fields[0].Get<std::uint32_t>();
    lease.accountName = fields[1].Get<std::string>();
    return lease;
}
} // namespace integration
} // namespace living_world
