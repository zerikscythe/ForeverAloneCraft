#pragma once

#include <cstdint>
#include <string>
#include <string_view>

class Player;
class Unit;

namespace living_world
{
namespace integration
{

// Writes a structured log entry for a bot lifecycle event.
// Writes to both LOG_INFO and living_world_bot_activity_log in acore_characters.
// All calls are fire-and-forget async DB writes — never blocks the world thread.
class BotActivityLog
{
public:
    // Player session bots
    static void Record(
        Player* bot,
        std::string_view eventType,
        std::string_view detail = "");

    // Creature bots — caller supplies identity name and guid since the creature
    // may not have a character record.
    static void Record(
        Unit* bot,
        std::string_view botName,
        std::uint64_t    botIdentityId,
        std::string_view eventType,
        std::string_view detail = "");
};

} // namespace integration
} // namespace living_world
