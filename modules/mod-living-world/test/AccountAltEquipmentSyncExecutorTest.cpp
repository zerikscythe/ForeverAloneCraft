#include "service/AccountAltEquipmentSyncExecutor.h"
#include "gtest/gtest.h"

#include <optional>
#include <utility>

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
        std::uint32_t,
        std::uint64_t) const override
    {
        return std::nullopt;
    }

    std::optional<model::AccountAltRuntimeRecord> FindByCloneCharacter(
        std::uint64_t) const override
    {
        return std::nullopt;
    }

    std::vector<model::AccountAltRuntimeRecord> ListRecoverableForAccount(
        std::uint32_t) const override
    {
        return {};
    }

    void SaveRuntime(model::AccountAltRuntimeRecord const& runtime) override
    {
        savedRuntime = runtime;
        ++saveCalls;
    }

    std::optional<model::AccountAltRuntimeRecord> savedRuntime;
    int saveCalls = 0;
};

class FakeEquipmentSyncRepository final
    : public integration::CharacterEquipmentSyncRepository
{
public:
    bool SyncEquipmentToCharacter(
        std::uint64_t sourceCharacterGuid,
        model::CharacterItemSnapshot const& sourceSnapshot,
        std::uint64_t cloneCharacterGuid,
        model::CharacterItemSnapshot const& cloneSnapshot) override
    {
        lastSourceGuid = sourceCharacterGuid;
        lastCloneGuid = cloneCharacterGuid;
        lastSourceSnapshot = sourceSnapshot;
        lastCloneSnapshot = cloneSnapshot;
        ++syncCalls;
        return shouldSucceed;
    }

    std::uint64_t lastSourceGuid = 0;
    std::uint64_t lastCloneGuid = 0;
    std::optional<model::CharacterItemSnapshot> lastSourceSnapshot;
    std::optional<model::CharacterItemSnapshot> lastCloneSnapshot;
    int syncCalls = 0;
    bool shouldSucceed = true;
};

model::AccountAltRuntimeRecord BuildRuntime()
{
    model::AccountAltRuntimeRecord runtime;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    return runtime;
}
} // namespace

TEST(AccountAltEquipmentSyncExecutorTest, MarksSyncingEquipmentThenActiveOnSuccess)
{
    FakeRuntimeRepository runtimeRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    AccountAltEquipmentSyncExecutor executor(
        runtimeRepository,
        equipmentSyncRepository);

    model::CharacterItemSnapshot sourceSnapshot;
    model::CharacterItemSnapshot cloneSnapshot;
    bool result = executor.Execute(
        BuildRuntime(),
        sourceSnapshot,
        cloneSnapshot);

    EXPECT_TRUE(result);
    EXPECT_EQ(equipmentSyncRepository.syncCalls, 1);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->state,
              model::AccountAltRuntimeState::Active);
    EXPECT_EQ(runtimeRepository.saveCalls, 2);
}

TEST(AccountAltEquipmentSyncExecutorTest, LeavesSyncingEquipmentOnFailure)
{
    FakeRuntimeRepository runtimeRepository;
    FakeEquipmentSyncRepository equipmentSyncRepository;
    equipmentSyncRepository.shouldSucceed = false;
    AccountAltEquipmentSyncExecutor executor(
        runtimeRepository,
        equipmentSyncRepository);

    model::CharacterItemSnapshot sourceSnapshot;
    model::CharacterItemSnapshot cloneSnapshot;
    bool result = executor.Execute(
        BuildRuntime(),
        sourceSnapshot,
        cloneSnapshot);

    EXPECT_FALSE(result);
    EXPECT_EQ(equipmentSyncRepository.syncCalls, 1);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->state,
              model::AccountAltRuntimeState::SyncingEquipment);
    EXPECT_EQ(runtimeRepository.saveCalls, 1);
}
} // namespace service
} // namespace living_world
