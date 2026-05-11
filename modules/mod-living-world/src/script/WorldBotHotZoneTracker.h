#pragma once

#include "Player.h"
#include "Timer.h"
#include "WorldSession.h"

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace living_world::script
{
namespace detail
{
struct WorldBotZoneKey
{
    std::uint32_t mapId = 0;
    std::uint32_t zoneId = 0;

    bool operator==(WorldBotZoneKey const& other) const
    {
        return mapId == other.mapId && zoneId == other.zoneId;
    }
};

struct WorldBotZoneKeyHash
{
    std::size_t operator()(WorldBotZoneKey const& key) const noexcept
    {
        return (static_cast<std::size_t>(key.mapId) << 32)
            ^ static_cast<std::size_t>(key.zoneId);
    }
};

struct WorldBotPlayerInterestState
{
    std::uint32_t mapId = 0;
    std::uint32_t zoneId = 0;
    std::uint32_t lastRefreshMs = 0;
    bool inFlight = false;
};

inline std::shared_mutex& WorldBotHotZoneLock()
{
    static std::shared_mutex lock;
    return lock;
}

inline std::unordered_map<WorldBotZoneKey, std::uint32_t, WorldBotZoneKeyHash>& WorldBotHotZones()
{
    static std::unordered_map<WorldBotZoneKey, std::uint32_t, WorldBotZoneKeyHash> zones;
    return zones;
}

inline std::unordered_map<std::uint64_t, WorldBotPlayerInterestState>& WorldBotPlayerInterest()
{
    static std::unordered_map<std::uint64_t, WorldBotPlayerInterestState> interest;
    return interest;
}

struct WorldBotSyntheticInterestState
{
    bool enabled = false;
    std::uint32_t mapId = 0;
    std::uint32_t zoneId = 0;
};

inline WorldBotSyntheticInterestState& WorldBotSyntheticInterest()
{
    static WorldBotSyntheticInterestState state;
    return state;
}

inline std::uint32_t& WorldBotHotZoneCooldownOverrideMs()
{
    static std::uint32_t value = 0;
    return value;
}
} // namespace detail

inline constexpr std::uint32_t WorldBotHotZoneCooldownMs = 10u * 60u * 1000u;
inline constexpr std::uint32_t WorldBotHotZoneRefreshMs = 30u * 1000u;

inline std::uint32_t GetWorldBotHotZoneCooldownMs()
{
    std::shared_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    return detail::WorldBotHotZoneCooldownOverrideMs() != 0
        ? detail::WorldBotHotZoneCooldownOverrideMs()
        : WorldBotHotZoneCooldownMs;
}

inline void SetWorldBotHotZoneCooldownOverrideMs(std::uint32_t cooldownMs)
{
    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    detail::WorldBotHotZoneCooldownOverrideMs() = cooldownMs;
}

inline void SetSyntheticWorldBotInterest(std::uint32_t mapId, std::uint32_t zoneId)
{
    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    detail::WorldBotSyntheticInterestState& state = detail::WorldBotSyntheticInterest();
    state.enabled = mapId != 0;
    state.mapId = mapId;
    state.zoneId = zoneId;
}

inline void ClearSyntheticWorldBotInterest()
{
    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    detail::WorldBotSyntheticInterest() = {};
}

inline bool HasSyntheticWorldBotInterest(std::uint32_t mapId, std::uint32_t zoneId)
{
    std::shared_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    detail::WorldBotSyntheticInterestState const& state = detail::WorldBotSyntheticInterest();
    if (!state.enabled || state.mapId == 0 || state.mapId != mapId)
        return false;

    return state.zoneId == 0 || zoneId == 0 || state.zoneId == zoneId;
}

inline void MarkWorldBotZoneHotLocked(std::uint32_t mapId, std::uint32_t zoneId, std::uint32_t nowMs)
{
    if (mapId == 0 || zoneId == 0)
        return;

    detail::WorldBotHotZones()[detail::WorldBotZoneKey{ mapId, zoneId }] = nowMs;
}

inline void MarkWorldBotZoneHot(std::uint32_t mapId, std::uint32_t zoneId, std::uint32_t nowMs)
{
    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    MarkWorldBotZoneHotLocked(mapId, zoneId, nowMs);
}

inline bool IsWorldBotZoneHot(std::uint32_t mapId, std::uint32_t zoneId, std::uint32_t nowMs = getMSTime())
{
    if (mapId == 0 || zoneId == 0)
        return false;

    std::shared_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    auto const itr = detail::WorldBotHotZones().find(detail::WorldBotZoneKey{ mapId, zoneId });
    if (itr == detail::WorldBotHotZones().end())
        return false;

    std::uint32_t const cooldownMs = detail::WorldBotHotZoneCooldownOverrideMs() != 0
        ? detail::WorldBotHotZoneCooldownOverrideMs()
        : WorldBotHotZoneCooldownMs;
    return getMSTimeDiff(itr->second, nowMs) <= cooldownMs;
}

inline void PruneWorldBotHotZones(std::uint32_t nowMs = getMSTime())
{
    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    auto& hotZones = detail::WorldBotHotZones();
    std::uint32_t const cooldownMs = detail::WorldBotHotZoneCooldownOverrideMs() != 0
        ? detail::WorldBotHotZoneCooldownOverrideMs()
        : WorldBotHotZoneCooldownMs;
    for (auto itr = hotZones.begin(); itr != hotZones.end(); )
    {
        if (getMSTimeDiff(itr->second, nowMs) > cooldownMs)
            itr = hotZones.erase(itr);
        else
            ++itr;
    }
}

inline void ObserveWorldBotPlayerInterest(Player* player, bool forceRefresh = false)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
        return;

    std::uint32_t const nowMs = getMSTime();
    std::uint64_t const guid = player->GetGUID().GetCounter();
    std::uint32_t const mapId = player->GetMapId();
    std::uint32_t const zoneId = player->GetZoneId();
    bool const inFlight = player->IsInFlight();

    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    auto& interest = detail::WorldBotPlayerInterest();
    detail::WorldBotPlayerInterestState& state = interest[guid];

    bool const unchanged = state.mapId == mapId
        && state.zoneId == zoneId
        && state.inFlight == inFlight;
    if (!forceRefresh && unchanged && getMSTimeDiff(state.lastRefreshMs, nowMs) < WorldBotHotZoneRefreshMs)
        return;

    if (inFlight)
    {
        if (!state.inFlight && state.mapId != 0 && state.zoneId != 0)
            MarkWorldBotZoneHotLocked(state.mapId, state.zoneId, nowMs);

        state.mapId = mapId;
        state.zoneId = zoneId;
        state.inFlight = true;
        state.lastRefreshMs = nowMs;
        return;
    }

    if (!state.inFlight && state.mapId != 0 && state.zoneId != 0
        && (state.mapId != mapId || state.zoneId != zoneId))
    {
        MarkWorldBotZoneHotLocked(state.mapId, state.zoneId, nowMs);
    }

    MarkWorldBotZoneHotLocked(mapId, zoneId, nowMs);

    state.mapId = mapId;
    state.zoneId = zoneId;
    state.inFlight = false;
    state.lastRefreshMs = nowMs;
}

inline void ForgetWorldBotPlayerInterest(Player* player)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBotSession())
        return;

    std::uint32_t const nowMs = getMSTime();
    std::uint64_t const guid = player->GetGUID().GetCounter();

    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    auto& interest = detail::WorldBotPlayerInterest();
    auto const itr = interest.find(guid);
    if (itr == interest.end())
        return;

    if (!itr->second.inFlight && itr->second.mapId != 0 && itr->second.zoneId != 0)
        MarkWorldBotZoneHotLocked(itr->second.mapId, itr->second.zoneId, nowMs);

    interest.erase(itr);
}

inline void ClearWorldBotHotZoneStateForTests()
{
    std::unique_lock<std::shared_mutex> lock(detail::WorldBotHotZoneLock());
    detail::WorldBotHotZones().clear();
    detail::WorldBotPlayerInterest().clear();
    detail::WorldBotSyntheticInterest() = {};
    detail::WorldBotHotZoneCooldownOverrideMs() = 0;
}
} // namespace living_world::script