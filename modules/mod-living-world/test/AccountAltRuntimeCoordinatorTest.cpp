#include "service/AccountAltRuntimeCoordinator.h"
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
    FakeSnapshotRepository snapshotRepository;
    AccountAltRecoveryService recoveryService;

    botAccountPoolRepository.lease = model::BotAccountLease { 701, "BOT701" };
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        snapshotRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingReservedAccount);
    EXPECT_EQ(decision.botAccountId, 701u);
    EXPECT_EQ(decision.spawnCharacterGuid, 9001u);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->ownerCharacterGuid, 42u);
}

TEST(AccountAltRuntimeCoordinatorTest,
     ExistingRuntimeWithoutCloneReusesReservedAccount)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeSnapshotRepository snapshotRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        snapshotRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::SpawnUsingReservedAccount);
    EXPECT_EQ(decision.botAccountId, 701u);
    EXPECT_EQ(runtimeRepository.saveCalls, 1);
}

TEST(AccountAltRuntimeCoordinatorTest, ExistingCloneAheadRequiresManualReview)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBotAccountPoolRepository botAccountPoolRepository;
    FakeSnapshotRepository snapshotRepository;
    AccountAltRecoveryService recoveryService;

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtimeRepository.Seed(runtime);
    snapshotRepository.sourceSnapshot = Snapshot(10, 200, 1000);
    snapshotRepository.cloneSnapshot = Snapshot(11, 0, 1500);

    AccountAltRuntimeCoordinator coordinator(
        runtimeRepository,
        botAccountPoolRepository,
        snapshotRepository,
        recoveryService);

    AccountAltSpawnDecision decision = coordinator.PlanSpawn(
        7,
        9001,
        42,
        "Tester");

    EXPECT_EQ(decision.kind,
              AccountAltSpawnDecisionKind::ManualReviewRequired);
}
} // namespace service
} // namespace living_world
