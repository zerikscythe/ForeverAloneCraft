#include "service/AccountAltRecoveryService.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
model::AccountAltRuntimeRecord BuildRuntime()
{
    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = 7;
    runtime.sourceCharacterGuid = 9001;
    runtime.ownerCharacterGuid = 42;
    runtime.cloneAccountId = 701;
    runtime.cloneCharacterGuid = 8001;
    runtime.state = model::AccountAltRuntimeState::Active;
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

TEST(AccountAltRecoveryServiceTest, ReusesCloneWhenCloneIsNotAhead)
{
    AccountAltRecoveryService service;
    model::AccountAltRecoveryPlan plan = service.BuildRecoveryPlan(
        BuildRuntime(),
        Snapshot(10, 200, 1000),
        Snapshot(10, 200, 1000),
        model::AccountAltSanityCheckResult {});

    EXPECT_EQ(plan.kind, model::AccountAltRecoveryPlanKind::ReuseClone);
    EXPECT_FALSE(plan.cloneProgressIsAhead);
    EXPECT_FALSE(plan.requiresSanityCheck);
}

TEST(AccountAltRecoveryServiceTest,
     RequiresManualReviewWhenAheadButSanityCheckFails)
{
    AccountAltRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = false;
    sanity.failures.push_back("inventory owner mismatch");

    model::AccountAltRecoveryPlan plan = service.BuildRecoveryPlan(
        BuildRuntime(),
        Snapshot(10, 200, 1000),
        Snapshot(10, 250, 1000),
        sanity);

    EXPECT_EQ(plan.kind, model::AccountAltRecoveryPlanKind::ManualReview);
    EXPECT_TRUE(plan.cloneProgressIsAhead);
    EXPECT_TRUE(plan.requiresSanityCheck);
}

TEST(AccountAltRecoveryServiceTest,
     SyncsOnlySafeDomainsWhenAheadAndSanityCheckPasses)
{
    AccountAltRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;
    sanity.safeDomains = {
        model::AccountAltSyncDomain::Experience,
        model::AccountAltSyncDomain::Money
    };

    model::AccountAltRecoveryPlan plan = service.BuildRecoveryPlan(
        BuildRuntime(),
        Snapshot(10, 200, 1000),
        Snapshot(11, 0, 1500),
        sanity);

    EXPECT_EQ(plan.kind, model::AccountAltRecoveryPlanKind::SyncCloneToSource);
    EXPECT_TRUE(plan.cloneProgressIsAhead);
    ASSERT_EQ(plan.domainsToSync.size(), 2u);
    EXPECT_EQ(plan.domainsToSync[0], model::AccountAltSyncDomain::Experience);
    EXPECT_EQ(plan.domainsToSync[1], model::AccountAltSyncDomain::Money);
}

TEST(AccountAltRecoveryServiceTest, BlocksIncompleteRuntimeIdentity)
{
    AccountAltRecoveryService service;
    model::AccountAltRuntimeRecord runtime = BuildRuntime();
    runtime.cloneCharacterGuid = 0;

    model::AccountAltRecoveryPlan plan = service.BuildRecoveryPlan(
        runtime,
        Snapshot(10, 200, 1000),
        Snapshot(11, 0, 1500),
        model::AccountAltSanityCheckResult {});

    EXPECT_EQ(plan.kind, model::AccountAltRecoveryPlanKind::Blocked);
}
} // namespace service
} // namespace living_world
