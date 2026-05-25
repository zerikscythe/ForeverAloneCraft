#pragma once

#include <cstdint>

class Player;

namespace living_world
{
namespace service
{

class BotLedgerShellConsumableService
{
public:
    bool IsLedgerShellBot(Player const* player) const;
    bool PrimeLedgerShellConsumables(Player* player) const;
};

} // namespace service
} // namespace living_world
