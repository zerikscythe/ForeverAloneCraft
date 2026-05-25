#pragma once

#include "model/BotShellLedgerTypes.h"

#include <optional>

namespace living_world
{
namespace integration
{

class SqlBotRuntimeSnapshotRepository
{
public:
    void EnsureSchema() const;

    std::optional<model::BotRuntimeSnapshotRecord> LoadByIdentity(
        std::uint32_t identityId) const;

    void Upsert(model::BotRuntimeSnapshotRecord const& record) const;
};

} // namespace integration
} // namespace living_world
