#include "service/AccountAltBankSyncExecutor.h"
#include "gtest/gtest.h"

#include <optional>

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

model::AccountAltRuntimeRecord BuildRuntime()
{
    model::AccountAltRuntimeRecord runtime;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    return runtime;
}
} // namespace

TEST(AccountAltBankSyncExecutorTest, MarksSyncingBankThenActiveOnSuccess)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBankSyncRepository bankSyncRepository;
    AccountAltBankSyncExecutor executor(
        runtimeRepository,
        bankSyncRepository);

    model::CharacterItemSnapshot sourceSnapshot;
    model::CharacterItemSnapshot cloneSnapshot;
    bool result = executor.Execute(
        BuildRuntime(),
        sourceSnapshot,
        cloneSnapshot);

    EXPECT_TRUE(result);
    EXPECT_EQ(bankSyncRepository.syncCalls, 1);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->state,
              model::AccountAltRuntimeState::Active);
    EXPECT_EQ(runtimeRepository.saveCalls, 2);
}

TEST(AccountAltBankSyncExecutorTest, LeavesSyncingBankOnFailure)
{
    FakeRuntimeRepository runtimeRepository;
    FakeBankSyncRepository bankSyncRepository;
    bankSyncRepository.shouldSucceed = false;
    AccountAltBankSyncExecutor executor(
        runtimeRepository,
        bankSyncRepository);

    model::CharacterItemSnapshot sourceSnapshot;
    model::CharacterItemSnapshot cloneSnapshot;
    bool result = executor.Execute(
        BuildRuntime(),
        sourceSnapshot,
        cloneSnapshot);

    EXPECT_FALSE(result);
    EXPECT_EQ(bankSyncRepository.syncCalls, 1);
    ASSERT_TRUE(runtimeRepository.savedRuntime);
    EXPECT_EQ(runtimeRepository.savedRuntime->state,
              model::AccountAltRuntimeState::SyncingBank);
    EXPECT_EQ(runtimeRepository.saveCalls, 1);
}
} // namespace service
} // namespace living_world
