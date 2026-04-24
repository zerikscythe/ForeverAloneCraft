#include "service/AccountAltItemRecoveryService.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(AccountAltItemRecoveryServiceTest, RequestsEquipmentSyncWhenApproved)
{
    AccountAltItemRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;
    sanity.safeDomains.push_back(model::AccountAltSyncDomain::Equipment);

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource);
    ASSERT_EQ(plan.domainsToSync.size(), 1u);
    EXPECT_EQ(plan.domainsToSync[0], model::AccountAltSyncDomain::Equipment);
}

TEST(AccountAltItemRecoveryServiceTest, RequiresManualReviewWhenSanityFails)
{
    AccountAltItemRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.failures.push_back("duplicate item guid");

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::ManualReview);
}

TEST(AccountAltItemRecoveryServiceTest, ReturnsNoActionWhenEquipmentMatches)
{
    AccountAltItemRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::NoAction);
}

TEST(AccountAltItemRecoveryServiceTest, BlocksInventoryUntilBagSyncExists)
{
    AccountAltItemRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;
    sanity.safeDomains.push_back(model::AccountAltSyncDomain::Inventory);

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::Blocked);
    ASSERT_EQ(plan.domainsToSync.size(), 1u);
    EXPECT_EQ(plan.domainsToSync[0], model::AccountAltSyncDomain::Inventory);
}

TEST(AccountAltItemRecoveryServiceTest, RequestsInventorySyncWhenPolicyEnablesIt)
{
    AccountAltItemRecoveryService service(
        AccountAltItemRecoveryOptions { true, false });
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;
    sanity.safeDomains.push_back(model::AccountAltSyncDomain::Inventory);

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource);
    ASSERT_EQ(plan.domainsToSync.size(), 1u);
    EXPECT_EQ(plan.domainsToSync[0], model::AccountAltSyncDomain::Inventory);
}

TEST(AccountAltItemRecoveryServiceTest, BlocksBankUntilBankSyncExists)
{
    AccountAltItemRecoveryService service;
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;
    sanity.safeDomains.push_back(model::AccountAltSyncDomain::Bank);

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::Blocked);
    ASSERT_EQ(plan.domainsToSync.size(), 1u);
    EXPECT_EQ(plan.domainsToSync[0], model::AccountAltSyncDomain::Bank);
}

TEST(AccountAltItemRecoveryServiceTest, RequestsInventoryAndBankSyncWhenPolicyEnablesBoth)
{
    AccountAltItemRecoveryService service(
        AccountAltItemRecoveryOptions { true, true });
    model::AccountAltSanityCheckResult sanity;
    sanity.passed = true;
    sanity.safeDomains.push_back(model::AccountAltSyncDomain::Inventory);
    sanity.safeDomains.push_back(model::AccountAltSyncDomain::Bank);

    model::AccountAltItemRecoveryPlan plan = service.BuildRecoveryPlan(sanity);

    EXPECT_EQ(plan.kind, model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource);
    ASSERT_EQ(plan.domainsToSync.size(), 2u);
    EXPECT_EQ(plan.domainsToSync[0], model::AccountAltSyncDomain::Inventory);
    EXPECT_EQ(plan.domainsToSync[1], model::AccountAltSyncDomain::Bank);
}
} // namespace service
} // namespace living_world
