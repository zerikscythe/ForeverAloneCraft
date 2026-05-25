#pragma once

#include "ObjectGuid.h"
#include "model/BotCombatControlMode.h"
#include "model/BotCombatMode.h"
#include "model/BotRuntimeKind.h"

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

    void RegisterPendingBot(
        ObjectGuid botCharacterGuid,
        ObjectGuid ownerCharacterGuid,
        model::BotRuntimeKind kind);
    void RegisterPendingOwner(ObjectGuid botCharacterGuid, ObjectGuid ownerCharacterGuid);
    std::optional<ObjectGuid> RegisterBotPlayer(Player* botPlayer);
    void UnregisterBotPlayer(Player* botPlayer);

    // Returns all active bots registered for this owner.
    std::vector<Player*> FindBotsForOwner(ObjectGuid ownerCharacterGuid) const;
    // Returns all active registered bots.
    std::vector<Player*> FindAllBots() const;
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

    // Combat control mode is keyed by owner GUID; defaults to Strict so the
    // old owner-assist behavior remains stable until a player opts into smart
    // targeting.
    void SetBotControlMode(
        ObjectGuid ownerCharacterGuid,
        model::BotCombatControlMode mode);
    model::BotCombatControlMode GetBotControlMode(
        ObjectGuid ownerCharacterGuid) const;
    void ClearBotControlMode(ObjectGuid ownerCharacterGuid);

    model::BotRuntimeKind GetBotRuntimeKind(ObjectGuid botCharacterGuid) const;
    void SetBotRuntimeKind(ObjectGuid botCharacterGuid, model::BotRuntimeKind kind);

private:
    mutable std::mutex _mutex;
    std::unordered_map<std::uint64_t, std::uint64_t> _pendingOwnersByBot;
    std::unordered_map<std::uint64_t, model::BotRuntimeKind> _pendingKindsByBot;
    std::unordered_map<std::uint64_t, std::uint64_t> _ownersByBot;
    std::unordered_map<std::uint64_t, model::BotRuntimeKind> _botKinds;
    // owner guid -> list of bot guids
    std::unordered_map<std::uint64_t, std::vector<ObjectGuid>> _botsByOwner;
    std::unordered_map<std::uint64_t, model::BotCombatMode> _botModes;
    std::unordered_map<std::uint64_t, model::BotCombatControlMode> _botControlModes;
};
} // namespace service
} // namespace living_world
