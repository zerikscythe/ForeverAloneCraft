#pragma once

#include "ObjectGuid.h"

#include <cstdint>
#include <string>

namespace living_world
{
namespace integration
{
enum class BotSessionSpawnStatus
{
    SpawnQueued,
    NoAvailableBotAccount,
    BotAccountNotFound,
    InvalidCharacterGuid
};

struct BotSessionSpawnResult
{
    BotSessionSpawnStatus status = BotSessionSpawnStatus::NoAvailableBotAccount;
    std::uint32_t botAccountId = 0;
    std::string botAccountName;
};

class BotSessionFactory
{
public:
    static BotSessionSpawnResult SpawnBotPlayerOnAccount(
        std::uint32_t botAccountId,
        ObjectGuid characterGuid,
        ObjectGuid ownerCharacterGuid);

    static BotSessionSpawnResult SpawnBotPlayer(
        ObjectGuid characterGuid,
        ObjectGuid ownerCharacterGuid);
};
} // namespace integration
} // namespace living_world
