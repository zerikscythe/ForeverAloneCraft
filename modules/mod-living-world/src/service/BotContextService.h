#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace living_world
{
namespace service
{

// Tracks the active combat context for each bot (e.g. "PvE", "PvP").
//
// Context drives which default profile the doctrine resolver picks:
//   "PvE"  → normal questing / dungeon / raid default profile
//   "PvP"  → battleground / world-PvP / duel default profile
//
// Future values such as "Arena", "BG", "RatedBG" can be added without any
// schema change — they just need matching rows in the default profile table.
//
// Set context when a bot's situation changes (enters/leaves a battleground,
// is assigned to a rival guild, etc.). The doctrine cache is NOT invalidated
// here — call ai::InvalidateBotCombatCaches() after Set() if you need the
// new profile to take effect on the very next AI tick.
//
// All public methods are thread-safe.
class BotContextService
{
public:
    // Returns the active context string for the given bot GUID.
    // Defaults to "PvE" if no context has been explicitly set.
    std::string Get(std::uint64_t botGuid) const;

    // Set the active context for a bot.
    void Set(std::uint64_t botGuid, std::string const& contextKey);

    // Reset to default ("PvE"). Call when a bot is dismissed / despawned.
    void Clear(std::uint64_t botGuid);

private:
    mutable std::mutex                           _mutex;
    std::unordered_map<std::uint64_t, std::string> _contexts;
};

} // namespace service
} // namespace living_world
