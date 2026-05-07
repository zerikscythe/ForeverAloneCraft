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

    // Spawn a bot with no owner. The bot fights back when attacked using its
    // doctrine but joins no player's group and has no companion relationship.
    static BotSessionSpawnResult SpawnHostileBotPlayerOnAccount(
        std::uint32_t botAccountId,
        ObjectGuid characterGuid);
};
} // namespace integration
} // namespace living_world
