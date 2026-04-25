#pragma once

#include "Chat.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>

class WorldSession;

namespace living_world
{
namespace script
{
enum class PlayerChatLogLevel : std::uint8_t
{
    BareMinimum = 1,
    Detailed = 2,
    Debug = 3,
    Trace = 4
};

constexpr std::uint8_t DefaultPlayerChatLogLevel =
    static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum);
constexpr std::uint8_t MaxPlayerChatLogLevel =
    static_cast<std::uint8_t>(PlayerChatLogLevel::Trace);

inline std::unordered_map<std::uint32_t, std::uint8_t> s_playerChatLogLevels;
inline std::mutex s_playerChatLogLevelsMutex;

inline std::uint8_t ClampPlayerChatLogLevel(std::uint8_t level)
{
    return std::min(level, MaxPlayerChatLogLevel);
}

inline std::uint8_t GetPlayerChatLogLevel(WorldSession const* session)
{
    if (!session)
    {
        return DefaultPlayerChatLogLevel;
    }

    std::lock_guard<std::mutex> guard(s_playerChatLogLevelsMutex);
    auto const itr = s_playerChatLogLevels.find(session->GetAccountId());
    if (itr == s_playerChatLogLevels.end())
    {
        return DefaultPlayerChatLogLevel;
    }

    return itr->second;
}

inline void SetPlayerChatLogLevel(std::uint32_t accountId, std::uint8_t level)
{
    std::lock_guard<std::mutex> guard(s_playerChatLogLevelsMutex);
    s_playerChatLogLevels[accountId] = ClampPlayerChatLogLevel(level);
}

inline bool ShouldSendPlayerChatLog(
    WorldSession const* session,
    std::uint8_t requiredLevel)
{
    return GetPlayerChatLogLevel(session) >=
        ClampPlayerChatLogLevel(requiredLevel);
}

inline char const* DescribePlayerChatLogLevel(std::uint8_t level)
{
    switch (ClampPlayerChatLogLevel(level))
    {
        case static_cast<std::uint8_t>(PlayerChatLogLevel::BareMinimum):
            return "bare minimum";
        case static_cast<std::uint8_t>(PlayerChatLogLevel::Detailed):
            return "detailed";
        case static_cast<std::uint8_t>(PlayerChatLogLevel::Debug):
            return "debug";
        case static_cast<std::uint8_t>(PlayerChatLogLevel::Trace):
            return "trace";
        default:
            break;
    }

    return "unknown";
}

template <typename... Args>
void SendPlayerLog(
    ChatHandler* handler,
    std::uint8_t requiredLevel,
    char const* format,
    Args&&... args)
{
    if (!handler || !ShouldSendPlayerChatLog(
            handler->GetSession(),
            requiredLevel))
    {
        return;
    }

    handler->PSendSysMessage(format, std::forward<Args>(args)...);
}
} // namespace script
} // namespace living_world
