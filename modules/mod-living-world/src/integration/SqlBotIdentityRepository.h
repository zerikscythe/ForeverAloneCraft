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
    std::string   loadoutKey;
    std::uint8_t  faction      = 0;
    std::uint32_t displayId    = 0;
    std::uint8_t  gender       = 0;
    std::uint8_t  skin         = 0;
    std::uint8_t  face         = 0;
    std::uint8_t  hairStyle    = 0;
    std::uint8_t  hairColor    = 0;
    std::uint8_t  facialStyle  = 0;
    bool          appearanceResolved = false;
    std::string   displayLoadoutKey;
    std::string   doctrineProfileKey;
    std::uint8_t  level        = 1;
    std::uint8_t  gearTier     = 1;
    std::string   personalityKey = "uninterested";
    bool          hasHerbalism = false;
    bool          hasMining    = false;
    bool          hasFishing   = false;
    std::string   populationRole = "world";
    std::uint32_t reserveCityZoneId = 0;
    std::uint32_t ambientGroupId = 0;
    std::uint32_t ambientGroupLeaderIdentityId = 0;
    std::string   ambientGroupRole;
    std::uint32_t homeZoneId   = 0;
    std::string   homeAnchorPointKey;
    std::string   homeBindPointKey;
    std::uint8_t  genericPotionCharges = 5;
    std::uint32_t sessionCount = 0;
    std::uint64_t totalWorldOnlineMs = 0;
    std::uint64_t worldOnlineMsSinceLevel = 0;
    std::uint64_t postMaxWorldOnlineMs = 0;
    std::uint64_t activeWorldSessionMs = 0;
    std::uint64_t activeWorldSessionBudgetMs = 0;
    std::string   runtimeState;
    std::string   runtimeDetail;
    std::uint32_t shellAccountId = 0;
    std::uint64_t shellCharacterGuid = 0;
    std::uint32_t shellStateVersion = 0;
    std::string   pendingRebuildReason;
    std::string   lastRehydrateAt;
    std::string   lastSessionSourceKind;
    std::string   lastSessionSourceKey;
    std::string   lastTaskFamily;
    std::string   lastTaskActivityKey;
    std::uint32_t lastTaskTargetZoneId = 0;
    std::string   lastQuestHubKey;
    std::uint64_t lastQuestHubElapsedMs = 0;
    bool          gearRefreshPending = false;
    std::uint8_t  lastGearRefreshBand = 0;
    std::uint32_t lastSeenZoneId = 0;
    bool          isRetired    = false;
    bool          isAvailable  = true;
};

class SqlBotIdentityRepository
{
public:
    void EnsureSchema() const;

    // Loads one world-bot identity row by ledger id.
    std::optional<BotIdentityRecord> FindById(std::uint32_t id) const;

    // Loads one world-bot identity row by unique display name.
    std::optional<BotIdentityRecord> FindByName(std::string const& name) const;

    // Returns up to `limit` available identities for the given faction.
    // faction=0 returns any faction. This call excludes dedicated reserve
    // populations such as city-only reserve pools.
    std::vector<BotIdentityRecord> LoadAvailable(
        std::uint8_t faction,
        std::uint32_t limit,
        std::uint8_t minLevel = 0,
        std::uint8_t maxLevel = 0) const;

    // Returns up to `limit` available city-reserve identities for the given
    // city zone and faction. faction=0 returns any faction.
    std::vector<BotIdentityRecord> LoadAvailableReserveForCity(
        std::uint32_t reserveCityZoneId,
        std::uint8_t faction,
        std::uint32_t limit) const;

    // Returns all currently available members for one ambient group, ordered
    // with the declared leader first when present.
    std::vector<BotIdentityRecord> LoadAvailableAmbientGroup(
        std::uint32_t ambientGroupId) const;

    // Marks the identity as active and increments session_count.
    // Call immediately after the creature is spawned.
    void MarkActive(std::uint32_t id) const;

    // Marks the identity as available again and records where it was last seen.
    // Call when the creature despawns.
    void MarkAvailable(std::uint32_t id, std::uint32_t lastSeenZoneId) const;

    // Persists spawn-time invisible-gear refresh state after assigned gear has
    // been generated or confirmed for the current refresh band.
    void UpdateGearRefreshState(
        std::uint32_t id,
        bool gearRefreshPending,
        std::uint8_t lastGearRefreshBand) const;

    std::vector<BotIdentityRecord> LoadAppearanceUnresolved(
        std::uint32_t limit = 0) const;

    void UpdateAppearance(
        std::uint32_t id,
        std::uint8_t skin,
        std::uint8_t face,
        std::uint8_t hairStyle,
        std::uint8_t hairColor,
        std::uint8_t facialStyle,
        bool appearanceResolved) const;

    void UpdateShellState(
        std::uint32_t id,
        std::uint32_t shellAccountId,
        std::uint64_t shellCharacterGuid,
        std::uint32_t shellStateVersion,
        std::string const& pendingRebuildReason = "") const;

    void MarkShellRehydrated(
        std::uint32_t id,
        std::uint32_t shellStateVersion) const;

    // Updates a currently active world bot's lightweight runtime ledger fields
    // so external tools can observe session duration and last active zone.
    void UpdateActiveRuntimeState(
        std::uint32_t id,
        std::uint32_t zoneId,
        std::uint64_t activeWorldSessionMs,
        std::string const& runtimeState,
        std::string const& runtimeDetail,
        std::string const& currentTaskActivityKey = "",
        std::string const& currentQuestHubKey = "",
        std::uint64_t currentQuestHubElapsedMs = 0) const;

    // Persists a world bot's fake generic potion stock for session carryover.
    void UpdateGenericPotionCharges(
        std::uint32_t id,
        std::uint8_t genericPotionCharges) const;

    // Finalizes one counted world session, applying online-time progression and
    // retirement rules before returning the identity to the available pool.
    void CompleteWorldSession(
        std::uint32_t id,
        std::uint32_t lastSeenZoneId,
        std::uint64_t sessionWorldOnlineMs,
        std::string const& lastSessionSourceKind = "",
        std::string const& lastSessionSourceKey = "",
        std::string const& lastTaskFamily = "",
        std::uint32_t lastTaskTargetZoneId = 0,
        std::string const& lastTaskActivityKey = "",
        std::string const& lastQuestHubKey = "",
        std::uint64_t lastQuestHubElapsedMs = 0) const;

    // On worldserver startup, reset any stale active creature-bot sessions that
    // were left marked active by a prior shutdown/crash.
    std::uint32_t RecoverStaleActiveSessions() const;
};

} // namespace integration
} // namespace living_world
