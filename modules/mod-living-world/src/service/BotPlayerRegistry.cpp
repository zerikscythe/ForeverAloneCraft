#include "service/BotPlayerRegistry.h"

#include "ObjectAccessor.h"
#include "Player.h"

namespace living_world
{
namespace service
{
BotPlayerRegistry& BotPlayerRegistry::Instance()
{
    static BotPlayerRegistry registry;
    return registry;
}

void BotPlayerRegistry::RegisterPendingOwner(
    ObjectGuid botCharacterGuid,
    ObjectGuid ownerCharacterGuid)
{
    std::lock_guard<std::mutex> guard(_mutex);
    _pendingOwnersByBot[botCharacterGuid.GetCounter()] =
        ownerCharacterGuid.GetCounter();
}

std::optional<ObjectGuid> BotPlayerRegistry::RegisterBotPlayer(Player* botPlayer)
{
    if (!botPlayer)
    {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> guard(_mutex);
    std::uint64_t const botGuid = botPlayer->GetGUID().GetCounter();
    auto const pending = _pendingOwnersByBot.find(botGuid);
    if (pending == _pendingOwnersByBot.end())
    {
        return std::nullopt;
    }

    ObjectGuid ownerGuid =
        ObjectGuid::Create<HighGuid::Player>(pending->second);
    _ownersByBot[botGuid] = pending->second;
    _botsByOwner[pending->second] = botPlayer->GetGUID();
    _pendingOwnersByBot.erase(pending);
    return ownerGuid;
}

void BotPlayerRegistry::UnregisterBotPlayer(Player* botPlayer)
{
    if (!botPlayer)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(_mutex);
    std::uint64_t const botGuid = botPlayer->GetGUID().GetCounter();
    auto const owner = _ownersByBot.find(botGuid);
    if (owner != _ownersByBot.end())
    {
        _botsByOwner.erase(owner->second);
        _ownersByBot.erase(owner);
    }

    _pendingOwnersByBot.erase(botGuid);
}

Player* BotPlayerRegistry::FindBotForOwner(ObjectGuid ownerCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto const itr = _botsByOwner.find(ownerCharacterGuid.GetCounter());
    if (itr == _botsByOwner.end())
    {
        return nullptr;
    }

    return ObjectAccessor::FindPlayer(itr->second);
}

std::optional<ObjectGuid> BotPlayerRegistry::FindOwnerForBot(
    ObjectGuid botCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto const itr = _ownersByBot.find(botCharacterGuid.GetCounter());
    if (itr == _ownersByBot.end())
    {
        return std::nullopt;
    }

    return ObjectGuid::Create<HighGuid::Player>(itr->second);
}
} // namespace service
} // namespace living_world
