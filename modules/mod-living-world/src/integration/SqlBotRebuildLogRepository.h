#pragma once

#include "model/BotShellLedgerTypes.h"

namespace living_world
{
namespace integration
{

class SqlBotRebuildLogRepository
{
public:
    void EnsureSchema() const;

    void Append(model::BotRebuildLogEntry const& entry) const;
};

} // namespace integration
} // namespace living_world
