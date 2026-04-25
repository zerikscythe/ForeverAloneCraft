#include "integration/SqlBotAccountPoolRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
std::optional<model::BotAccountLease>
SqlBotAccountPoolRepository::ReserveAccountForSource(
    std::uint32_t,
    std::uint64_t sourceCharacterGuid)
{
    QueryResult reserved = LoginDatabase.Query(
        "SELECT p.account_id, a.username "
        "FROM living_world_bot_account_pool p "
        "INNER JOIN account a ON a.id = p.account_id "
        "WHERE p.reserved_for = {} "
        "LIMIT 1",
        sourceCharacterGuid);
    if (reserved)
    {
        Field const* fields = reserved->Fetch();
        model::BotAccountLease lease;
        lease.accountId = fields[0].Get<std::uint32_t>();
        lease.accountName = fields[1].Get<std::string>();

        LoginDatabase.Execute(
            "UPDATE living_world_bot_account_pool "
            "SET is_available = 0 WHERE account_id = {};",
            lease.accountId);
        return lease;
    }

    QueryResult result = LoginDatabase.Query(
        "SELECT p.account_id, a.username "
        "FROM living_world_bot_account_pool p "
        "INNER JOIN account a ON a.id = p.account_id "
        "WHERE p.is_available = 1 "
        "ORDER BY p.account_id ASC "
        "LIMIT 1");
    if (!result)
    {
        return std::nullopt;
    }

    Field const* fields = result->Fetch();
    model::BotAccountLease lease;
    lease.accountId = fields[0].Get<std::uint32_t>();
    lease.accountName = fields[1].Get<std::string>();

    LoginDatabase.Execute(
        "UPDATE living_world_bot_account_pool "
        "SET is_available = 0, reserved_for = {} WHERE account_id = {};",
        sourceCharacterGuid,
        lease.accountId);
    return lease;
}
} // namespace integration
} // namespace living_world
