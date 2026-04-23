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
} // namespace service
} // namespace living_world
