#include "service/AccountAltItemRecoveryService.h"

#include <algorithm>

namespace living_world
{
namespace service
{
model::AccountAltItemRecoveryPlan AccountAltItemRecoveryService::BuildRecoveryPlan(
    model::AccountAltSanityCheckResult const& sanityCheck) const
{
    model::AccountAltItemRecoveryPlan plan;

    if (!sanityCheck.passed)
    {
        plan.kind = model::AccountAltItemRecoveryPlanKind::ManualReview;
        plan.reason = "item sanity checks failed";
        return plan;
    }

    auto const inventoryItr = std::find(
        sanityCheck.safeDomains.begin(),
        sanityCheck.safeDomains.end(),
        model::AccountAltSyncDomain::Inventory);
    auto const bankItr = std::find(
        sanityCheck.safeDomains.begin(),
        sanityCheck.safeDomains.end(),
        model::AccountAltSyncDomain::Bank);
    if (inventoryItr != sanityCheck.safeDomains.end() ||
        bankItr != sanityCheck.safeDomains.end())
    {
        plan.kind = model::AccountAltItemRecoveryPlanKind::Blocked;
        plan.reason = "inventory/bank differs but bag-domain sync is not enabled yet";
        if (inventoryItr != sanityCheck.safeDomains.end())
        {
            plan.domainsToSync.push_back(model::AccountAltSyncDomain::Inventory);
        }
        if (bankItr != sanityCheck.safeDomains.end())
        {
            plan.domainsToSync.push_back(model::AccountAltSyncDomain::Bank);
        }
        return plan;
    }

    auto const equipmentItr = std::find(
        sanityCheck.safeDomains.begin(),
        sanityCheck.safeDomains.end(),
        model::AccountAltSyncDomain::Equipment);
    if (equipmentItr != sanityCheck.safeDomains.end())
    {
        plan.kind = model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource;
        plan.domainsToSync.push_back(model::AccountAltSyncDomain::Equipment);
        plan.reason = "equipment differs and item sanity checks passed";
        return plan;
    }

    plan.kind = model::AccountAltItemRecoveryPlanKind::NoAction;
    plan.reason = "item snapshots do not require sync";
    return plan;
}
} // namespace service
} // namespace living_world
