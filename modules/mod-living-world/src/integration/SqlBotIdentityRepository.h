#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{

struct BotIdentityRecord
{
    std::uint32_t id           = 0;
    std::string   name;
    std::uint8_t  raceId       = 0;
    std::uint8_t  classId      = 0;
    std::string   specKey;
    std::uint8_t  faction      = 0;
    std::uint32_t displayId    = 0;
    std::uint8_t  gender       = 0;
    std::uint8_t  level        = 1;
    std::uint8_t  gearTier     = 1;
    bool          hasHerbalism = false;
    bool          hasMining    = false;
    bool          hasFishing   = false;
    std::uint32_t homeZoneId   = 0;
    std::string   homeAnchorPointKey;
    std::string   homeBindPointKey;
    std::uint32_t sessionCount = 0;
    std::uint64_t totalWorldOnlineMs = 0;
    std::uint64_t worldOnlineMsSinceLevel = 0;
    std::uint64_t postMaxWorldOnlineMs = 0;
    std::uint64_t activeWorldSessionMs = 0;
    std::uint32_t lastSeenZoneId = 0;
    bool          isRetired    = false;
};

class SqlBotIdentityRepository
{
public:
    // Returns up to `limit` available identities for the given faction.
    // faction=0 returns any faction.
    std::vector<BotIdentityRecord> LoadAvailable(
        std::uint8_t faction,
        std::uint32_t limit) const;

    // Marks the identity as active and increments session_count.
    // Call immediately after the creature is spawned.
    void MarkActive(std::uint32_t id) const;

    // Marks the identity as available again and records where it was last seen.
    // Call when the creature despawns.
    void MarkAvailable(std::uint32_t id, std::uint32_t lastSeenZoneId) const;

    // Finalizes one counted world session, applying online-time progression and
    // retirement rules before returning the identity to the available pool.
    void CompleteWorldSession(
        std::uint32_t id,
        std::uint32_t lastSeenZoneId,
        std::uint64_t sessionWorldOnlineMs) const;

    // On worldserver startup, reset any stale active creature-bot sessions that
    // were left marked active by a prior shutdown/crash.
    std::uint32_t RecoverStaleActiveSessions() const;
};

} // namespace integration
} // namespace living_world
