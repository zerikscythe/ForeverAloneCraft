#include "service/AccountAltStartupRecoveryService.h"
#include "gtest/gtest.h"

#include <optional>
#include <utility>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{
class FakeRuntimeRepository final
    : public integration::AccountAltRuntimeRepository
{
public:
    std::optional<model::AccountAltRuntimeRecord> FindBySourceCharacter(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid) const override
    {
        for (model::AccountAltRuntimeRecord const& runtime : runtimes)
        {
            if (runtime.sourceAccountId == sourceAccountId &&
                runtime.sourceCharacterGuid == sourceCharacterGuid)
            {
                return runtime;
            }
        }

        return std::nullopt;
    }

    std::vector<model::AccountAltRuntimeRecord> ListRecoverableForAccount(
        std::uint32_t sourceAccountId) const override
    {
        std::vector<model::AccountAltRuntimeRecord> results;
        for (model::AccountAltRuntimeRecord const& runtime : runtimes)
        {
            if (runtime.sourceAccountId == sourceAccountId)
            {
                results.push_back(runtime);
            }
        }

        return results;
    }

    void SaveRuntime(model::AccountAltRuntimeRecord const& runtime) override
    {
        savedRuntimes.push_back(runtime);
    }

    std::vector<model::AccountAltRuntimeRecord> runtimes;
    std::vector<model::AccountAltRuntimeRecord> savedRuntimes;
};

class FakeSnapshotRepository final
    : public integration::CharacterProgressSnapshotRepository
{
public:
    std::optional<model::CharacterProgressSnapshot> LoadSnapshot(
        std::uint64_t characterGuid) const override
    {
        if (characterGuid == sourceGuid)
        {
            return sourceSnapshot;
        }

        if (characterGuid == cloneGuid)
        {
            return cloneSnapshot;
        }

        return std::nullopt;
    }

    std::uint64_t sourceGuid = 9001;
    std::uint64_t cloneGuid = 8001;
    std::optional<model::CharacterProgressSnapshot> sourceSnapshot;
    std::optional<model::CharacterProgressSnapshot> cloneSnapshot;
};

class FakeItemSnapshotRepository final
    : public integration::CharacterItemSnapshotRepository
{
public:
    std::optional<model::CharacterItemSnapshot> LoadSnapshot(
        std::uint64_t characterGuid) const override
    {
        if (characterGuid == sourceGuid)
        {
            return sourceSnapshot;
        }

        if (characterGuid == cloneGuid)
        {
            return cloneSnapshot;
        }

        return std::nullopt;
    }

    std::uint64_t sourceGuid = 9001;
    std::uint64_t cloneGuid = 8001;
    std::optional<model::CharacterItemSnapshot> sourceSnapshot;
    std::optional<model::CharacterItemSnapshot> cloneSnapshot;
};

class FakeEquipmentSyncRepository final
    : public integration::CharacterEquipmentSyncRepository
{
public:
    bool SyncEquipmentToCharacter(
        std::uint64_t,
        model::CharacterItemSnapshot const&,
        std::uint64_t,
        model::CharacterItemSnapshot const&) override
    {
        ++syncCalls;
        return shouldSucceed;
    }

    int syncCalls = 0;
    bool shouldSucceed = true;
};

class FakeSyncRepository final
    : public integration::CharacterProgressSyncRepository
{
public:
    bool SyncProgressToCharacter(
        std::uint64_t characterGuid,
        model::CharacterProgressSnapshot const& snapshot) override
    {
        lastSyncedGuid = characterGuid;
        lastSyncedSnapshot = snapshot;
        ++syncCalls;
        return shouldSucceed;
    }

    std::uint64_t lastSyncedGuid = 0;
    std::optional<model::CharacterProgressSnapshot> lastSyncedSnapshot;
    int syncCalls = 0;
    bool shouldSucceed = true;
};

model::AccountAltRuntimeRecord BuildRuntime(model::AccountAltRuntimeState state)
{
    model::AccountAltRuntimeRecord runtime;
    runtime.runtimeId = 101;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = state;
    runtime.sourceSnapshot.level = 10;
    runtime.sourceSnapshot.experience = 200;
    runtime.sourceSnapshot.money = 1000;
    return runtime;
}

model::CharacterProgressSnapshot Snapshot(
    std::uint8_t level,
    std::uint32_t experience,
    std::uint32_t money)
{
    model::CharacterProgressSnapshot snapshot;
    snapshot.level = level;
    snapshot.experience = experience;
    snapshot.money = money;
    return snapshot;
}
} // namespace

TEST(AccountAltStartupRecoveryServiceTest, RetriesInterruptedSyncOnLogin)
{
    FakeRuntimeRepository runtimeRepository;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    runtimeRepository.runtimes.push_back(
        BuildRuntime(model::AccountAltRuntimeState::SyncingBack));
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(12, 500, 1500);
    itemSnapshotRepository.sourceSnapshot = model::CharacterItemSnapshot {};
    itemSnapshotRepository.cloneSnapshot = model::CharacterItemSnapshot {};

    AccountAltStartupRecoveryService service(
        runtimeRepository,
        itemSnapshotRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltStartupRecoverySummary summary = service.RecoverForAccount(7);

    EXPECT_EQ(summary.scanned, 1u);
    EXPECT_EQ(summary.recoveredSyncs, 1u);
    EXPECT_EQ(summary.manualReviewRequired, 0u);
    EXPECT_EQ(syncRepository.syncCalls, 1);
    EXPECT_EQ(syncRepository.lastSyncedGuid, 9001u);
    ASSERT_TRUE(syncRepository.lastSyncedSnapshot);
    EXPECT_EQ(syncRepository.lastSyncedSnapshot->level, 12u);
    ASSERT_EQ(runtimeRepository.savedRuntimes.size(), 2u);
    EXPECT_EQ(runtimeRepository.savedRuntimes.back().state,
              model::AccountAltRuntimeState::Active);
}

TEST(AccountAltStartupRecoveryServiceTest,
     FlagsManualReviewWhenInterruptedSyncLooksImplausible)
{
    FakeRuntimeRepository runtimeRepository;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    runtimeRepository.runtimes.push_back(
        BuildRuntime(model::AccountAltRuntimeState::SyncingBack));
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(20, 0, 1500);
    itemSnapshotRepository.sourceSnapshot = model::CharacterItemSnapshot {};
    itemSnapshotRepository.cloneSnapshot = model::CharacterItemSnapshot {};

    AccountAltStartupRecoveryService service(
        runtimeRepository,
        itemSnapshotRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltStartupRecoverySummary summary = service.RecoverForAccount(7);

    EXPECT_EQ(summary.scanned, 1u);
    EXPECT_EQ(summary.recoveredSyncs, 0u);
    EXPECT_EQ(summary.manualReviewRequired, 1u);
    EXPECT_EQ(syncRepository.syncCalls, 0);
}

TEST(AccountAltStartupRecoveryServiceTest,
     ReportsPendingRecoveryWithoutWritingForActiveCloneAheadRuntime)
{
    FakeRuntimeRepository runtimeRepository;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    runtimeRepository.runtimes.push_back(
        BuildRuntime(model::AccountAltRuntimeState::Active));
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(12, 500, 1500);
    itemSnapshotRepository.sourceSnapshot = model::CharacterItemSnapshot {};
    itemSnapshotRepository.cloneSnapshot = model::CharacterItemSnapshot {};

    AccountAltStartupRecoveryService service(
        runtimeRepository,
        itemSnapshotRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltStartupRecoverySummary summary = service.RecoverForAccount(7);

    EXPECT_EQ(summary.scanned, 1u);
    EXPECT_EQ(summary.pendingRecovery, 1u);
    EXPECT_EQ(summary.recoveredSyncs, 0u);
    EXPECT_EQ(syncRepository.syncCalls, 0);
}
} // namespace service
} // namespace living_world
