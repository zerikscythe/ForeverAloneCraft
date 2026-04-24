#include "service/AccountAltDismissalService.h"
#include "gtest/gtest.h"

#include <optional>
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
        if (!_runtime ||
            _runtime->sourceAccountId != sourceAccountId ||
            _runtime->sourceCharacterGuid != sourceCharacterGuid)
        {
            return std::nullopt;
        }

        return _runtime;
    }

    std::optional<model::AccountAltRuntimeRecord> FindByCloneCharacter(
        std::uint64_t cloneCharacterGuid) const override
    {
        if (!_runtime || _runtime->cloneCharacterGuid != cloneCharacterGuid)
        {
            return std::nullopt;
        }

        return _runtime;
    }

    std::vector<model::AccountAltRuntimeRecord> ListRecoverableForAccount(
        std::uint32_t sourceAccountId) const override
    {
        if (!_runtime || _runtime->sourceAccountId != sourceAccountId)
        {
            return {};
        }

        return { *_runtime };
    }

    void SaveRuntime(model::AccountAltRuntimeRecord const& runtime) override
    {
        _runtime = runtime;
        savedRuntimes.push_back(runtime);
    }

    void Seed(model::AccountAltRuntimeRecord runtime)
    {
        _runtime = runtime;
    }

    std::vector<model::AccountAltRuntimeRecord> savedRuntimes;

private:
    std::optional<model::AccountAltRuntimeRecord> _runtime;
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

class FakeInventorySyncRepository final
    : public integration::CharacterInventorySyncRepository
{
public:
    bool SyncInventoryToCharacter(
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

class FakeBankSyncRepository final
    : public integration::CharacterBankSyncRepository
{
public:
    bool SyncBankToCharacter(
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

class FakeNameLeaseRepository final
    : public integration::CharacterNameLeaseRepository
{
public:
    bool RestoreSourceNameLease(
        model::AccountAltRuntimeRecord const&) override
    {
        ++restoreCalls;
        return shouldSucceed;
    }

    int restoreCalls = 0;
    bool shouldSucceed = true;
};

model::AccountAltRuntimeRecord BuildRuntime()
{
    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.sourceCharacterName = "Tester";
    runtime.reservedSourceCharacterName = "Lwtester";
    runtime.cloneCharacterName = "Lwtester";
    runtime.state = model::AccountAltRuntimeState::Active;
    runtime.sourceSnapshot.level = 10;
    runtime.sourceSnapshot.experience = 200;
    runtime.sourceSnapshot.money = 1000;
    runtime.cloneSnapshot = runtime.sourceSnapshot;
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

TEST(AccountAltDismissalServiceTest, SyncsProgressAndRestoresNamesOnDismiss)
{
    FakeRuntimeRepository runtimeRepository;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeNameLeaseRepository nameLeaseRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    runtimeRepository.Seed(BuildRuntime());
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(12, 500, 1500);
    itemSnapshotRepository.sourceSnapshot = model::CharacterItemSnapshot {};
    itemSnapshotRepository.cloneSnapshot = model::CharacterItemSnapshot {};

    AccountAltDismissalService service(
        runtimeRepository,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        nameLeaseRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltDismissalSummary summary = service.DismissClone(8001);

    EXPECT_TRUE(summary.runtimeFound);
    EXPECT_TRUE(summary.progressSynced);
    EXPECT_FALSE(summary.equipmentSynced);
    EXPECT_TRUE(summary.namesRestored);
    EXPECT_FALSE(summary.manualReviewRequired);
    EXPECT_FALSE(summary.blocked);
    EXPECT_EQ(syncRepository.syncCalls, 1);
    EXPECT_EQ(syncRepository.lastSyncedGuid, 9001u);
    EXPECT_EQ(nameLeaseRepository.restoreCalls, 1);
}

TEST(AccountAltDismissalServiceTest, FlagsManualReviewButStillRestoresNames)
{
    FakeRuntimeRepository runtimeRepository;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeNameLeaseRepository nameLeaseRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    runtimeRepository.Seed(BuildRuntime());
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(20, 0, 1500);
    itemSnapshotRepository.sourceSnapshot = model::CharacterItemSnapshot {};
    itemSnapshotRepository.cloneSnapshot = model::CharacterItemSnapshot {};

    AccountAltDismissalService service(
        runtimeRepository,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        nameLeaseRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltDismissalSummary summary = service.DismissClone(8001);

    EXPECT_TRUE(summary.runtimeFound);
    EXPECT_FALSE(summary.progressSynced);
    EXPECT_TRUE(summary.manualReviewRequired);
    EXPECT_TRUE(summary.namesRestored);
    EXPECT_EQ(syncRepository.syncCalls, 0);
    EXPECT_EQ(nameLeaseRepository.restoreCalls, 1);
}

TEST(AccountAltDismissalServiceTest, SyncsInventoryWhenPolicyEnablesIt)
{
    FakeRuntimeRepository runtimeRepository;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeNameLeaseRepository nameLeaseRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    runtimeRepository.Seed(BuildRuntime());
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(10, 200, 1000);

    model::CharacterItemSnapshot sourceItems;
    model::CharacterItemSnapshot cloneItems;
    model::CharacterItemSnapshotEntry sourceBag;
    sourceBag.itemGuid = 1001;
    sourceBag.itemEntry = 5001;
    sourceBag.itemCount = 1;
    sourceBag.slot = 19;
    sourceBag.domain = model::CharacterItemStorageDomain::Inventory;
    sourceItems.inventoryItems.push_back(sourceBag);
    model::CharacterItemSnapshotEntry cloneBag = sourceBag;
    cloneBag.itemGuid = 2001;
    cloneBag.itemEntry = 5002;
    cloneItems.inventoryItems.push_back(cloneBag);
    itemSnapshotRepository.sourceSnapshot = sourceItems;
    itemSnapshotRepository.cloneSnapshot = cloneItems;

    AccountAltDismissalService service(
        runtimeRepository,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        nameLeaseRepository,
        snapshotRepository,
        syncRepository,
        recoveryService,
        AccountAltItemRecoveryOptions { true, false });

    AccountAltDismissalSummary summary = service.DismissClone(8001);

    EXPECT_TRUE(summary.runtimeFound);
    EXPECT_FALSE(summary.manualReviewRequired);
    EXPECT_FALSE(summary.blocked);
    EXPECT_TRUE(summary.namesRestored);
    EXPECT_EQ(inventorySyncRepository.syncCalls, 1);
    EXPECT_EQ(bankSyncRepository.syncCalls, 0);
}
} // namespace service
} // namespace living_world
