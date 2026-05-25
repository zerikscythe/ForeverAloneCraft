#pragma once

#include "model/BotShellLedgerTypes.h"

#include <optional>

namespace living_world
{
namespace integration
{

class SqlBotShellRuntimeRepository
{
public:
    void EnsureSchema() const;

    std::optional<model::BotShellRuntimeRecord> FindByIdentity(
        std::uint32_t identityId) const;

    std::optional<model::BotShellRuntimeRecord> FindByShell(
        std::uint32_t shellAccountId,
        std::uint64_t shellCharacterGuid) const;

    void Upsert(model::BotShellRuntimeRecord const& record) const;

    void RemoveByIdentity(std::uint32_t identityId) const;
};

} // namespace integration
} // namespace living_world
