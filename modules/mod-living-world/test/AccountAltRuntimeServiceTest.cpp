#include "service/AccountAltRuntimeService.h"
#include "gtest/gtest.h"

#include <optional>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{
class FakeAccountAltRuntimeRepository
    : public integration::AccountAltRuntimeRepository
{
public:
    void Seed(model::AccountAltRuntimeRecord runtime)
    {
        _runtime = std::move(runtime);
    }

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

    void SaveRuntime(
        model::AccountAltRuntimeRecord const& runtime) override
    {
        savedRuntime = runtime;
        _runtime = runtime;
    }

    void DeleteRuntime(std::uint64_t runtimeId) override
    {
        if (_runtime && _runtime->runtimeId == runtimeId)
        {
            _runtime.reset();
        }
    }

    std::optional<model::AccountAltRuntimeRecord> savedRuntime;

private:
    std::optional<model::AccountAltRuntimeRecord> _runtime;
};

class FakeBotAccountPoolRepository
    : public integration::BotAccountPoolRepository
{
public:
    void AddLease(std::uint32_t accountId, std::string accountName)
    {
        _leases.push_back(model::BotAccountLease {
            accountId, std::move(accountName) });
    }

    std::optional<model::BotAccountLease> ReserveAccountForSource(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid) override
    {
        lastSourceAccountId = sourceAccountId;
        lastSourceCharacterGuid = sourceCharacterGuid;
        ++reserveCalls;

        if (_leases.empty())
        {
            return std::nullopt;
        }

        model::BotAccountLease lease = _leases.front();
        _leases.erase(_leases.begin());
        return lease;
    }

    int reserveCalls = 0;
    std::uint32_t lastSourceAccountId = 0;
    std::uint64_t lastSourceCharacterGuid = 0;

private:
    std::vector<model::BotAccountLease> _leases;
};

model::AccountAltRuntimeRequest BuildRequest()
{
    model::AccountAltRuntimeRequest request;
    request.sourceAccountId = 7;
    request.sourceCharacterGuid = 9001;
    request.ownerCharacterGuid = 42;
    request.sourceCharacterName = "Tester";
    request.sourceSnapshot.level = 10;
    request.sourceSnapshot.experience = 200;
    request.sourceSnapshot.money = 12345;
    return request;
}
} // namespace

TEST(AccountAltRuntimeServiceTest,
     NewRuntimeReservesBotAccountAndWritesPreparingRecord)
{
    FakeAccountAltRuntimeRepository repository;
    FakeBotAccountPoolRepository pool;
    pool.AddLease(700, "FACBOT001");
    AccountAltRuntimeService service(repository, pool);

    model::AccountAltRuntimeDecision decision =
        service.PrepareRuntime(BuildRequest());

    ASSERT_EQ(decision.kind,
              model::AccountAltRuntimeDecisionKind::PrepareNewClone);
    ASSERT_TRUE(decision.runtime);
    ASSERT_TRUE(repository.savedRuntime);
    EXPECT_EQ(pool.reserveCalls, 1);
    EXPECT_EQ(pool.lastSourceAccountId, 7u);
    EXPECT_EQ(pool.lastSourceCharacterGuid, 9001u);
    EXPECT_EQ(decision.runtime->cloneAccountId, 700u);
    EXPECT_EQ(decision.runtime->ownerCharacterGuid, 42u);
    EXPECT_EQ(decision.runtime->sourceCharacterName, "Tester");
    EXPECT_EQ(decision.runtime->cloneCharacterName, "Tester");
    EXPECT_EQ(decision.runtime->reservedSourceCharacterName.size(), 12u);
    EXPECT_EQ(decision.runtime->reservedSourceCharacterName.substr(0, 2),
              "Lw");
    EXPECT_EQ(decision.runtime->state,
              model::AccountAltRuntimeState::PreparingClone);
}

TEST(AccountAltRuntimeServiceTest, ReusesActiveRuntimeWithoutNewLease)
{
    FakeAccountAltRuntimeRepository repository;
    FakeBotAccountPoolRepository pool;
    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
    runtime.cloneSnapshot.level = 10;
    runtime.cloneSnapshot.experience = 200;
    repository.Seed(runtime);
    AccountAltRuntimeService service(repository, pool);

    model::AccountAltRuntimeDecision decision =
        service.PrepareRuntime(BuildRequest());

    ASSERT_EQ(decision.kind,
              model::AccountAltRuntimeDecisionKind::ReuseActiveClone);
    ASSERT_TRUE(decision.runtime);
    EXPECT_EQ(decision.runtime->cloneAccountId, 701u);
    EXPECT_EQ(pool.reserveCalls, 0);
}

TEST(AccountAltRuntimeServiceTest,
     RecoveringRuntimeMarksCloneAuthoritativeWhenCloneProgressIsAhead)
{
    FakeAccountAltRuntimeRepository repository;
    FakeBotAccountPoolRepository pool;
    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::PreparingClone;
    runtime.cloneSnapshot.level = 10;
    runtime.cloneSnapshot.experience = 250;
    repository.Seed(runtime);
    AccountAltRuntimeService service(repository, pool);

    model::AccountAltRuntimeDecision decision =
        service.PrepareRuntime(BuildRequest());

    EXPECT_EQ(decision.kind,
              model::AccountAltRuntimeDecisionKind::RecoverCloneBeforeUse);
    EXPECT_TRUE(decision.cloneProgressIsAuthoritative);
    EXPECT_EQ(pool.reserveCalls, 0);
}

TEST(AccountAltRuntimeServiceTest, RejectsWhenNoBotAccountIsAvailable)
{
    FakeAccountAltRuntimeRepository repository;
    FakeBotAccountPoolRepository pool;
    AccountAltRuntimeService service(repository, pool);

    model::AccountAltRuntimeDecision decision =
        service.PrepareRuntime(BuildRequest());

    EXPECT_EQ(decision.kind,
              model::AccountAltRuntimeDecisionKind::BlockedNoBotAccount);
    EXPECT_FALSE(decision.runtime);
    EXPECT_FALSE(repository.savedRuntime);
    EXPECT_EQ(pool.reserveCalls, 1);
}
} // namespace service
} // namespace living_world
