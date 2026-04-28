#include "service/BotPlayerRegistry.h"

#include "ObjectAccessor.h"
#include "Player.h"

#include <algorithm>

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

    std::uint64_t const ownerGuidLow = pending->second;
    ObjectGuid ownerGuid = ObjectGuid::Create<HighGuid::Player>(ownerGuidLow);
    _ownersByBot[botGuid] = ownerGuidLow;

    auto& bots = _botsByOwner[ownerGuidLow];
    // Avoid duplicate registration.
    bool found = false;
    for (ObjectGuid const& g : bots)
        if (g.GetCounter() == botGuid) { found = true; break; }
    if (!found)
        bots.push_back(botPlayer->GetGUID());

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
    auto const ownerItr = _ownersByBot.find(botGuid);
    if (ownerItr != _ownersByBot.end())
    {
        std::uint64_t const ownerGuidLow = ownerItr->second;
        auto botsItr = _botsByOwner.find(ownerGuidLow);
        if (botsItr != _botsByOwner.end())
        {
            auto& bots = botsItr->second;
            bots.erase(
                std::remove_if(bots.begin(), bots.end(),
                    [botGuid](ObjectGuid const& g){ return g.GetCounter() == botGuid; }),
                bots.end());
            if (bots.empty())
                _botsByOwner.erase(botsItr);
        }
        _ownersByBot.erase(ownerItr);
    }

    _pendingOwnersByBot.erase(botGuid);
}

Player* BotPlayerRegistry::FindBotForOwner(ObjectGuid ownerCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto const itr = _botsByOwner.find(ownerCharacterGuid.GetCounter());
    if (itr == _botsByOwner.end() || itr->second.empty())
    {
        return nullptr;
    }

    return ObjectAccessor::FindPlayer(itr->second.front());
}

std::vector<Player*> BotPlayerRegistry::FindBotsForOwner(ObjectGuid ownerCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    std::vector<Player*> result;
    auto const itr = _botsByOwner.find(ownerCharacterGuid.GetCounter());
    if (itr == _botsByOwner.end())
        return result;

    for (ObjectGuid const& guid : itr->second)
    {
        Player* bot = ObjectAccessor::FindPlayer(guid);
        if (bot)
            result.push_back(bot);
    }
    return result;
}

Player* BotPlayerRegistry::FindBotForOwnerByGuid(
    ObjectGuid ownerCharacterGuid,
    ObjectGuid botCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto const itr = _botsByOwner.find(ownerCharacterGuid.GetCounter());
    if (itr == _botsByOwner.end())
        return nullptr;

    for (ObjectGuid const& guid : itr->second)
    {
        if (guid.GetCounter() == botCharacterGuid.GetCounter())
            return ObjectAccessor::FindPlayer(guid);
    }
    return nullptr;
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

std::optional<ObjectGuid> BotPlayerRegistry::FindPendingBotForOwner(
    ObjectGuid ownerCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    for (auto const& [botGuid, pendingOwnerGuid] : _pendingOwnersByBot)
    {
        if (pendingOwnerGuid == ownerCharacterGuid.GetCounter())
        {
            return ObjectGuid::Create<HighGuid::Player>(botGuid);
        }
    }

    return std::nullopt;
}

bool BotPlayerRegistry::IsPendingBotForOwner(
    ObjectGuid ownerCharacterGuid,
    ObjectGuid botCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto const itr = _pendingOwnersByBot.find(botCharacterGuid.GetCounter());
    if (itr == _pendingOwnersByBot.end())
    {
        return false;
    }

    return itr->second == ownerCharacterGuid.GetCounter();
}

bool BotPlayerRegistry::HasPendingBotForOwner(ObjectGuid ownerCharacterGuid) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    for (auto const& [botGuid, pendingOwnerGuid] : _pendingOwnersByBot)
    {
        if (pendingOwnerGuid == ownerCharacterGuid.GetCounter())
            return true;
    }
    return false;
}
} // namespace service
} // namespace living_world