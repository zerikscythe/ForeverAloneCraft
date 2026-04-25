#pragma once

#include "ObjectGuid.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

class Player;

namespace living_world
{
namespace service
{
class BotPlayerRegistry
{
public:
    static BotPlayerRegistry& Instance();

    void RegisterPendingOwner(ObjectGuid botCharacterGuid, ObjectGuid ownerCharacterGuid);
    std::optional<ObjectGuid> RegisterBotPlayer(Player* botPlayer);
    void UnregisterBotPlayer(Player* botPlayer);

    Player* FindBotForOwner(ObjectGuid ownerCharacterGuid) const;
    std::optional<ObjectGuid> FindOwnerForBot(ObjectGuid botCharacterGuid) const;
    std::optional<ObjectGuid> FindPendingBotForOwner(ObjectGuid ownerCharacterGuid) const;
    bool IsPendingBotForOwner(
        ObjectGuid ownerCharacterGuid,
        ObjectGuid botCharacterGuid) const;

private:
    mutable std::mutex _mutex;
    std::unordered_map<std::uint64_t, std::uint64_t> _pendingOwnersByBot;
    std::unordered_map<std::uint64_t, std::uint64_t> _ownersByBot;
    std::unordered_map<std::uint64_t, ObjectGuid> _botsByOwner;
};
} // namespace service
} // namespace living_world
