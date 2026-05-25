#pragma once

#include "ObjectGuid.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace living_world
{
namespace service
{

enum class AmbientGroupPullPhase : std::uint8_t
{
    None = 0,
    Arming,
    Committed,
};

enum class AmbientGroupDistressTier : std::uint8_t
{
    None = 0,
    Alert,
    AssistRequested,
    Urgent,
    Critical,
};

struct AmbientGroupCombatSnapshot
{
    std::uint32_t groupId = 0;

    ObjectGuid partyPrimaryTargetGuid;
    std::uint64_t partyPrimaryUpdatedAtMs = 0;

    ObjectGuid tankAnchorTargetGuid;
    std::uint64_t tankAnchorUpdatedAtMs = 0;

    ObjectGuid distressedAllyGuid;
    ObjectGuid distressTargetGuid;
    AmbientGroupDistressTier distressTier = AmbientGroupDistressTier::None;
    std::uint64_t distressStartedAtMs = 0;
    std::uint64_t distressUpdatedAtMs = 0;

    ObjectGuid aggroClaimedByGuid;
    std::uint64_t aggroClaimUpdatedAtMs = 0;

    ObjectGuid peelTargetGuid;
    ObjectGuid peelClaimedByGuid;
    std::uint64_t peelClaimUpdatedAtMs = 0;
    ObjectGuid peelAssistClaimedByGuid;
    std::uint64_t peelAssistClaimUpdatedAtMs = 0;

    bool stabilizationActive = false;
    std::uint64_t stabilizationUpdatedAtMs = 0;

    AmbientGroupPullPhase leaderPullPhase = AmbientGroupPullPhase::None;
    std::uint64_t leaderPullPhaseUpdatedAtMs = 0;
};

class AmbientGroupCombatStateService
{
public:
    [[nodiscard]] AmbientGroupCombatSnapshot Get(
        std::uint32_t groupId,
        std::uint64_t nowMs) const
    {
        if (groupId == 0)
            return {};

        std::lock_guard<std::mutex> lock(_mutex);
        auto const it = _snapshots.find(groupId);
        if (it == _snapshots.end())
            return {};

        PruneExpiredLocked(it->second, nowMs);
        return it->second;
    }

    [[nodiscard]] bool PublishPartyPrimaryTarget(
        std::uint32_t groupId,
        ObjectGuid targetGuid,
        std::uint64_t nowMs)
    {
        if (groupId == 0 || targetGuid.IsEmpty())
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto& snapshot = _snapshots[groupId];
        bool const changed = snapshot.partyPrimaryTargetGuid != targetGuid;
        snapshot.groupId = groupId;
        snapshot.partyPrimaryTargetGuid = targetGuid;
        snapshot.partyPrimaryUpdatedAtMs = nowMs;
        return changed;
    }

    [[nodiscard]] bool ClaimTankAnchor(
        std::uint32_t groupId,
        ObjectGuid tankGuid,
        ObjectGuid targetGuid,
        std::uint64_t nowMs)
    {
        if (groupId == 0 || tankGuid.IsEmpty() || targetGuid.IsEmpty())
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto& snapshot = _snapshots[groupId];
        bool const changed =
            snapshot.tankAnchorTargetGuid != targetGuid
            || snapshot.aggroClaimedByGuid != tankGuid;
        snapshot.groupId = groupId;
        snapshot.tankAnchorTargetGuid = targetGuid;
        snapshot.tankAnchorUpdatedAtMs = nowMs;
        snapshot.aggroClaimedByGuid = tankGuid;
        snapshot.aggroClaimUpdatedAtMs = nowMs;
        return changed;
    }

    [[nodiscard]] bool PublishDistress(
        std::uint32_t groupId,
        ObjectGuid allyGuid,
        ObjectGuid attackerGuid,
        AmbientGroupDistressTier tier,
        std::uint64_t nowMs)
    {
        if (groupId == 0 || allyGuid.IsEmpty() || attackerGuid.IsEmpty()
            || tier == AmbientGroupDistressTier::None)
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto& snapshot = _snapshots[groupId];
        if (snapshot.distressTier != AmbientGroupDistressTier::None
            && snapshot.distressUpdatedAtMs != 0
            && nowMs <= (snapshot.distressUpdatedAtMs + DistressContinuationMs)
            && static_cast<std::uint8_t>(tier) < static_cast<std::uint8_t>(snapshot.distressTier))
        {
            return false;
        }

        bool const sameAlly = snapshot.distressedAllyGuid == allyGuid;
        bool const continuingThread =
            sameAlly
            && snapshot.distressTier != AmbientGroupDistressTier::None
            && snapshot.distressUpdatedAtMs != 0
            && nowMs <= (snapshot.distressUpdatedAtMs + DistressContinuationMs);
        bool const changed =
            snapshot.distressedAllyGuid != allyGuid
            || snapshot.distressTargetGuid != attackerGuid
            || snapshot.distressTier != tier
            || !snapshot.stabilizationActive;
        bool const newThread =
            !continuingThread
            && (snapshot.distressedAllyGuid != allyGuid
                || snapshot.distressTargetGuid != attackerGuid
                || snapshot.distressTier == AmbientGroupDistressTier::None);
        snapshot.groupId = groupId;
        snapshot.distressedAllyGuid = allyGuid;
        snapshot.distressTargetGuid = attackerGuid;
        snapshot.distressTier = tier;
        if (newThread || snapshot.distressStartedAtMs == 0)
            snapshot.distressStartedAtMs = nowMs;
        snapshot.distressUpdatedAtMs = nowMs;
        snapshot.stabilizationActive = true;
        snapshot.stabilizationUpdatedAtMs = nowMs;
        return changed;
    }

    [[nodiscard]] bool ClearDistress(
        std::uint32_t groupId,
        ObjectGuid allyGuid,
        ObjectGuid attackerGuid)
    {
        if (groupId == 0 || allyGuid.IsEmpty() || attackerGuid.IsEmpty())
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto const it = _snapshots.find(groupId);
        if (it == _snapshots.end())
            return false;

        auto& snapshot = it->second;
        if (snapshot.distressedAllyGuid != allyGuid
            || snapshot.distressTargetGuid != attackerGuid)
        {
            return false;
        }

        bool const changed = !snapshot.distressedAllyGuid.IsEmpty()
            || snapshot.distressTier != AmbientGroupDistressTier::None
            || snapshot.stabilizationActive;
        snapshot.distressedAllyGuid.Clear();
        snapshot.distressTargetGuid.Clear();
        snapshot.distressTier = AmbientGroupDistressTier::None;
        snapshot.distressStartedAtMs = 0;
        snapshot.distressUpdatedAtMs = 0;
        snapshot.stabilizationActive = false;
        snapshot.stabilizationUpdatedAtMs = 0;
        snapshot.peelAssistClaimedByGuid.Clear();
        snapshot.peelAssistClaimUpdatedAtMs = 0;
        return changed;
    }

    [[nodiscard]] bool SetLeaderPullPhase(
        std::uint32_t groupId,
        AmbientGroupPullPhase phase,
        std::uint64_t nowMs)
    {
        if (groupId == 0)
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto& snapshot = _snapshots[groupId];
        bool const changed = snapshot.leaderPullPhase != phase;
        snapshot.groupId = groupId;
        snapshot.leaderPullPhase = phase;
        snapshot.leaderPullPhaseUpdatedAtMs = nowMs;
        return changed;
    }

    [[nodiscard]] bool ClaimPeel(
        std::uint32_t groupId,
        ObjectGuid claimerGuid,
        ObjectGuid targetGuid,
        std::uint64_t nowMs)
    {
        if (groupId == 0 || claimerGuid.IsEmpty() || targetGuid.IsEmpty())
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto& snapshot = _snapshots[groupId];
        PruneExpiredLocked(snapshot, nowMs);

        if (!snapshot.peelClaimedByGuid.IsEmpty()
            && snapshot.peelClaimedByGuid != claimerGuid
            && snapshot.peelTargetGuid == targetGuid)
        {
            return false;
        }

        bool const changed =
            snapshot.peelClaimedByGuid != claimerGuid
            || snapshot.peelTargetGuid != targetGuid;
        snapshot.groupId = groupId;
        if (changed)
        {
            snapshot.peelAssistClaimedByGuid.Clear();
            snapshot.peelAssistClaimUpdatedAtMs = 0;
        }
        snapshot.peelClaimedByGuid = claimerGuid;
        snapshot.peelTargetGuid = targetGuid;
        snapshot.peelClaimUpdatedAtMs = nowMs;

        // Once a DPS peels the target off the healer, the problem should
        // transition into "DPS is now carrying this add" rather than remain a
        // perpetual healer-distress thread.
        if (snapshot.distressTargetGuid == targetGuid
            && snapshot.distressedAllyGuid != claimerGuid)
        {
            snapshot.distressedAllyGuid = claimerGuid;
            snapshot.distressTier = AmbientGroupDistressTier::Alert;
            snapshot.distressStartedAtMs = nowMs;
            snapshot.distressUpdatedAtMs = nowMs;
            snapshot.stabilizationActive = true;
            snapshot.stabilizationUpdatedAtMs = nowMs;
        }
        return changed;
    }

    [[nodiscard]] bool ClearPeel(
        std::uint32_t groupId,
        ObjectGuid claimerGuid,
        ObjectGuid targetGuid)
    {
        if (groupId == 0)
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto const it = _snapshots.find(groupId);
        if (it == _snapshots.end())
            return false;

        auto& snapshot = it->second;
        if ((!claimerGuid.IsEmpty() && snapshot.peelClaimedByGuid != claimerGuid)
            || (!targetGuid.IsEmpty() && snapshot.peelTargetGuid != targetGuid))
        {
            return false;
        }

        bool const changed = !snapshot.peelClaimedByGuid.IsEmpty() || !snapshot.peelTargetGuid.IsEmpty();
        snapshot.peelClaimedByGuid.Clear();
        snapshot.peelTargetGuid.Clear();
        snapshot.peelClaimUpdatedAtMs = 0;
        snapshot.peelAssistClaimedByGuid.Clear();
        snapshot.peelAssistClaimUpdatedAtMs = 0;
        return changed;
    }

    [[nodiscard]] bool ClaimPeelAssist(
        std::uint32_t groupId,
        ObjectGuid claimerGuid,
        ObjectGuid targetGuid,
        std::uint64_t nowMs)
    {
        if (groupId == 0 || claimerGuid.IsEmpty() || targetGuid.IsEmpty())
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto& snapshot = _snapshots[groupId];
        PruneExpiredLocked(snapshot, nowMs);

        if (snapshot.peelTargetGuid != targetGuid || snapshot.peelClaimedByGuid.IsEmpty())
            return false;

        if (!snapshot.peelAssistClaimedByGuid.IsEmpty()
            && snapshot.peelAssistClaimedByGuid != claimerGuid)
        {
            return false;
        }

        bool const changed = snapshot.peelAssistClaimedByGuid != claimerGuid;
        snapshot.groupId = groupId;
        snapshot.peelAssistClaimedByGuid = claimerGuid;
        snapshot.peelAssistClaimUpdatedAtMs = nowMs;
        return changed;
    }

    [[nodiscard]] bool ClearPeelAssist(
        std::uint32_t groupId,
        ObjectGuid claimerGuid)
    {
        if (groupId == 0)
            return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto const it = _snapshots.find(groupId);
        if (it == _snapshots.end())
            return false;

        auto& snapshot = it->second;
        if (!claimerGuid.IsEmpty() && snapshot.peelAssistClaimedByGuid != claimerGuid)
            return false;

        bool const changed = !snapshot.peelAssistClaimedByGuid.IsEmpty();
        snapshot.peelAssistClaimedByGuid.Clear();
        snapshot.peelAssistClaimUpdatedAtMs = 0;
        return changed;
    }

    void Clear(std::uint32_t groupId)
    {
        if (groupId == 0)
            return;

        std::lock_guard<std::mutex> lock(_mutex);
        _snapshots.erase(groupId);
    }

private:
    void PruneExpiredLocked(
        AmbientGroupCombatSnapshot& snapshot,
        std::uint64_t nowMs) const
    {
        if (!snapshot.partyPrimaryTargetGuid.IsEmpty()
            && nowMs > (snapshot.partyPrimaryUpdatedAtMs + PrimaryTargetTtlMs))
        {
            snapshot.partyPrimaryTargetGuid.Clear();
            snapshot.partyPrimaryUpdatedAtMs = 0;
        }

        if (!snapshot.tankAnchorTargetGuid.IsEmpty()
            && nowMs > (snapshot.tankAnchorUpdatedAtMs + TankAnchorTtlMs))
        {
            snapshot.tankAnchorTargetGuid.Clear();
            snapshot.tankAnchorUpdatedAtMs = 0;
        }

        if (!snapshot.distressedAllyGuid.IsEmpty()
            && nowMs > (snapshot.distressUpdatedAtMs + DistressTtlMs))
        {
            snapshot.distressedAllyGuid.Clear();
            snapshot.distressTargetGuid.Clear();
            snapshot.distressTier = AmbientGroupDistressTier::None;
            snapshot.distressStartedAtMs = 0;
            snapshot.distressUpdatedAtMs = 0;
            snapshot.peelAssistClaimedByGuid.Clear();
            snapshot.peelAssistClaimUpdatedAtMs = 0;
        }

        if (!snapshot.aggroClaimedByGuid.IsEmpty()
            && nowMs > (snapshot.aggroClaimUpdatedAtMs + AggroClaimTtlMs))
        {
            snapshot.aggroClaimedByGuid.Clear();
            snapshot.aggroClaimUpdatedAtMs = 0;
        }

        if (!snapshot.peelClaimedByGuid.IsEmpty()
            && nowMs > (snapshot.peelClaimUpdatedAtMs + PeelClaimTtlMs))
        {
            snapshot.peelClaimedByGuid.Clear();
            snapshot.peelTargetGuid.Clear();
            snapshot.peelClaimUpdatedAtMs = 0;
            snapshot.peelAssistClaimedByGuid.Clear();
            snapshot.peelAssistClaimUpdatedAtMs = 0;
        }

        if (!snapshot.peelAssistClaimedByGuid.IsEmpty()
            && nowMs > (snapshot.peelAssistClaimUpdatedAtMs + PeelAssistClaimTtlMs))
        {
            snapshot.peelAssistClaimedByGuid.Clear();
            snapshot.peelAssistClaimUpdatedAtMs = 0;
        }

        if (snapshot.stabilizationActive
            && nowMs > (snapshot.stabilizationUpdatedAtMs + StabilizationTtlMs))
        {
            snapshot.stabilizationActive = false;
            snapshot.stabilizationUpdatedAtMs = 0;
        }

        if (snapshot.leaderPullPhase != AmbientGroupPullPhase::None
            && nowMs > (snapshot.leaderPullPhaseUpdatedAtMs + PullPhaseTtlMs))
        {
            snapshot.leaderPullPhase = AmbientGroupPullPhase::None;
            snapshot.leaderPullPhaseUpdatedAtMs = 0;
        }
    }

    mutable std::mutex _mutex;
    mutable std::unordered_map<std::uint32_t, AmbientGroupCombatSnapshot> _snapshots;

    static constexpr std::uint64_t PrimaryTargetTtlMs = 7000;
    static constexpr std::uint64_t TankAnchorTtlMs = 5000;
    static constexpr std::uint64_t DistressTtlMs = 4500;
    static constexpr std::uint64_t DistressContinuationMs = 2500;
    static constexpr std::uint64_t AggroClaimTtlMs = 4500;
    static constexpr std::uint64_t PeelClaimTtlMs = 4500;
    static constexpr std::uint64_t PeelAssistClaimTtlMs = 4500;
    static constexpr std::uint64_t StabilizationTtlMs = 4500;
    static constexpr std::uint64_t PullPhaseTtlMs = 5000;
};

} // namespace service
} // namespace living_world
