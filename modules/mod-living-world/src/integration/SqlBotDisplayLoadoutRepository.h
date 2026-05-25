#pragma once

#include "model/BotShellLedgerTypes.h"

#include <optional>

namespace living_world
{
namespace integration
{

class SqlBotDisplayLoadoutRepository
{
public:
    void EnsureSchema() const;

    std::optional<model::BotDisplayLoadoutRecord> LoadByIdentity(
        std::uint32_t identityId) const;

    void Replace(model::BotDisplayLoadoutRecord const& record) const;
};

} // namespace integration
} // namespace living_world
