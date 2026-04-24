#include "service/AccountAltRuntimeCoordinator.h"
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
        savedRuntime = runtime;
        ++saveCalls;
    }

    void Seed(model::AccountAltRuntimeRecord runtime)
    {
        _runtime = std::move(runtime);
    }

    std::optional<model::AccountAltRuntimeRecord> savedRuntime;
    int saveCalls = 0;

private:
    std::optional<model::AccountAltRuntimeRecord> _runtime;
};

class FakeBotAccountPoolRepository final
    : public integration::BotAccountPoolRepository
{
public:
    std::optional<model::BotAccountLease> ReserveAccountForSource(
        std::uint32_t,
        std::uint64_t) override
    {
        ++reserveCalls;
        return lease;
    }

    std::optional<model::BotAccountLease> lease;
    int reserveCalls = 0;
};

class FakeCloneMaterializer final
    : public integration::CharacterCloneMaterializer
{
public:
    integration::CharacterCloneMaterializationResult MaterializeClone(
        model::AccountAltRuntimeRecord const& runtime) override
    {
        requestedRuntime = runtime;
        ++materializeCalls;
        return result;
    }

    std::optional<model::AccountAltRuntimeRecord> requestedRuntime;
    integration::CharacterCloneMaterializationResult result;
    int materializeCalls = 0;
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

TEST(AccountAltRuntimeCoordinatorTest, NewRuntimeUsesReservedAccount)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    botAccountPoolRepository.lease = model::BotAccountLease { 701, "BOT701" };
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    cloneMaterializer.result.succeeded = true;
    cloneMaterializer.result.cloneCharacterGuid = 8001;
    cloneMaterializer.result.cloneCharacterName = "Tester";
    cloneMaterializer.result.cloneSnapshot = Snapshot(10, 200, 1000);
    cloneMaterializer.result.reason = "materialized persistent clone character";

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingPersistentClone);
    EXPECT_EQ(decision.botAccountId, 701u);
    EXPECT_EQ(decision.spawnCharacterGuid, 8001u);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->ownerCharacterGuid, 42u);
    EXPECT_EQ(runtimeRepository.savedRuntime->cloneCharacterGuid, 8001u);
    EXPECT_EQ(cloneMaterializer.materializeCalls, 1);
}

TEST(AccountAltRuntimeCoordinatorTest,
     ExistingRuntimeWithoutCloneReusesReservedAccount)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    cloneMaterializer.result.succeeded = true;
    cloneMaterializer.result.cloneCharacterGuid = 8001;
    cloneMaterializer.result.cloneCharacterName = "Tester";
    cloneMaterializer.result.cloneSnapshot = Snapshot(10, 200, 1000);
    cloneMaterializer.result.reason = "materialized persistent clone character";

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingPersistentClone);
    EXPECT_EQ(decision.botAccountId, 701u);
    EXPECT_EQ(decision.spawnCharacterGuid, 8001u);
    EXPECT_EQ(runtimeRepository.saveCalls, 1);
    EXPECT_EQ(cloneMaterializer.materializeCalls, 1);
}

TEST(AccountAltRuntimeCoordinatorTest,
     ExistingCloneAheadBeyondCapRequiresManualReview)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(20, 0, 1500);

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind, AccountAltSpawnDecisionKind::ManualReviewRequired);
    EXPECT_EQ(syncRepository.syncCalls, 0);
}

TEST(AccountAltRuntimeCoordinatorTest,
     ExistingCloneAheadWithinCapSyncsAndSpawns)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(12, 500, 1500);
    itemSnapshotRepository.sourceSnapshot = model::CharacterItemSnapshot {};
    itemSnapshotRepository.cloneSnapshot = model::CharacterItemSnapshot {};

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingPersistentClone);
    EXPECT_EQ(syncRepository.syncCalls, 1);
    EXPECT_EQ(syncRepository.lastSyncedGuid, 9001u);
    ASSERT_TRUE(syncRepository.lastSyncedSnapshot);
    EXPECT_EQ(syncRepository.lastSyncedSnapshot->level, 12u);
    EXPECT_EQ(syncRepository.lastSyncedSnapshot->money, 1500u);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->state,
              model::AccountAltRuntimeState::Active);
    EXPECT_EQ(equipmentSyncRepository.syncCalls, 0);
}

TEST(AccountAltRuntimeCoordinatorTest, BlocksWhenCloneMaterializationFails)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    botAccountPoolRepository.lease = model::BotAccountLease { 701, "BOT701" };
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    cloneMaterializer.result.reason = "bot account has too many characters";

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind, AccountAltSpawnDecisionKind::Blocked);
    EXPECT_EQ(decision.reason, "bot account has too many characters");
}

TEST(AccountAltRuntimeCoordinatorTest, ReuseCloneRunsEquipmentRecoveryWhenApproved)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(10, 200, 1000);

    model::CharacterItemSnapshot sourceItems;
    model::CharacterItemSnapshot cloneItems;
    model::CharacterItemSnapshotEntry sourceMainHand;
    sourceMainHand.itemGuid = 1001;
    sourceMainHand.itemEntry = 5001;
    sourceMainHand.itemCount = 1;
    sourceMainHand.slot = 15;
    sourceMainHand.domain = model::CharacterItemStorageDomain::Equipment;
    sourceItems.equipmentItems.push_back(sourceMainHand);

    model::CharacterItemSnapshotEntry cloneMainHand = sourceMainHand;
    cloneMainHand.itemGuid = 2001;
    cloneMainHand.itemEntry = 5002;
    cloneItems.equipmentItems.push_back(cloneMainHand);

    itemSnapshotRepository.sourceSnapshot = sourceItems;
    itemSnapshotRepository.cloneSnapshot = cloneItems;

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingPersistentClone);
    EXPECT_EQ(equipmentSyncRepository.syncCalls, 1);
}

TEST(AccountAltRuntimeCoordinatorTest,
     ReuseCloneKeepsInventoryRecoveryBlockedByDefaultPolicy)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
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

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind, AccountAltSpawnDecisionKind::Blocked);
    EXPECT_EQ(inventorySyncRepository.syncCalls, 0);
}

TEST(AccountAltRuntimeCoordinatorTest,
     ReuseCloneRunsInventoryRecoveryWhenPolicyEnablesIt)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeCloneMaterializer cloneMaterializer;
    FakeItemSnapshotRepository itemSnapshotRepository;
    FakeInventorySyncRepository inventorySyncRepository;
    FakeBankSyncRepository bankSyncRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    FakeSnapshotRepository snapshotRepository;
    FakeSyncRepository syncRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
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

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        cloneMaterializer,
        itemSnapshotRepository,
        inventorySyncRepository,
        bankSyncRepository,
        equipmentSyncRepository,
        snapshotRepository,
        syncRepository,
        recoveryService,
        AccountAltItemRecoveryOptions { true, false });

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingPersistentClone);
    EXPECT_EQ(inventorySyncRepository.syncCalls, 1);
    EXPECT_EQ(bankSyncRepository.syncCalls, 0);
}
} // namespace service
} // namespace living_world
