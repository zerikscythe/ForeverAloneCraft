#pragma once

#include "ObjectGuid.h"
#include "model/BotCombatMode.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

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

    // Returns all active bots registered for this owner.
    std::vector<Player*> FindBotsForOwner(ObjectGuid ownerCharacterGuid) const;
    // Returns the specific bot by character guid, if owned by ownerCharacterGuid.
    Player* FindBotForOwnerByGuid(ObjectGuid ownerCharacterGuid, ObjectGuid botCharacterGuid) const;

    std::optional<ObjectGuid> FindOwnerForBot(ObjectGuid botCharacterGuid) const;
    std::optional<ObjectGuid> FindPendingBotForOwner(ObjectGuid ownerCharacterGuid) const;
    bool IsPendingBotForOwner(
        ObjectGuid ownerCharacterGuid,
        ObjectGuid botCharacterGuid) const;
    // Returns true if any bot login is pending for this owner.
    bool HasPendingBotForOwner(ObjectGuid ownerCharacterGuid) const;

    // Combat mode is keyed by owner GUID; defaults to Assist when not set.
    void SetBotMode(ObjectGuid ownerCharacterGuid, model::BotCombatMode mode);
    model::BotCombatMode GetBotMode(ObjectGuid ownerCharacterGuid) const;
    void ClearBotMode(ObjectGuid ownerCharacterGuid);

private:
    mutable std::mutex _mutex;
    std::unordered_map<std::uint64_t, std::uint64_t> _pendingOwnersByBot;
    std::unordered_map<std::uint64_t, std::uint64_t> _ownersByBot;
    // owner guid -> list of bot guids
    std::unordered_map<std::uint64_t, std::vector<ObjectGuid>> _botsByOwner;
    std::unordered_map<std::uint64_t, model::BotCombatMode> _botModes;
};
} // namespace service
} // namespace living_world
