#include "service/AccountAltRecoveryService.h"

namespace living_world
{
namespace service
{
namespace
{
bool CloneProgressIsAhead(
    model::CharacterProgressSnapshot const& source,
    model::CharacterProgressSnapshot const& clone)
{
    if (clone.level != source.level)
    {
        return clone.level > source.level;
    }

    if (clone.experience != source.experience)
    {
        return clone.experience > source.experience;
    }

    return clone.money > source.money;
}
} // namespace

model::AccountAltRecoveryPlan AccountAltRecoveryService::BuildRecoveryPlan(
    model::AccountAltRuntimeRecord const& runtime,
    model::CharacterProgressSnapshot const& currentSourceSnapshot,
    model::CharacterProgressSnapshot const& currentCloneSnapshot,
    model::AccountAltSanityCheckResult const& sanityCheck) const
{
    model::AccountAltRecoveryPlan plan;
    plan.runtime = runtime;
    plan.cloneProgressIsAhead =
        CloneProgressIsAhead(currentSourceSnapshot, currentCloneSnapshot);

    if (runtime.state == model::AccountAltRuntimeState::Failed)
    {
        plan.kind = model::AccountAltRecoveryPlanKind::Blocked;
        plan.reason = "runtime is marked failed";
        return plan;
    }

    if (runtime.cloneCharacterGuid == 0 || runtime.cloneAccountId == 0)
    {
        plan.kind = model::AccountAltRecoveryPlanKind::Blocked;
        plan.reason = "runtime clone identity is incomplete";
        return plan;
    }

    if (!plan.cloneProgressIsAhead)
    {
        plan.kind = model::AccountAltRecoveryPlanKind::ReuseClone;
        plan.reason = "clone is not ahead of the source";
        return plan;
    }

    plan.requiresSanityCheck = true;
    if (!sanityCheck.passed)
    {
        plan.kind = model::AccountAltRecoveryPlanKind::ManualReview;
        plan.reason = "clone is ahead but sanity checks failed";
        return plan;
    }

    plan.kind = model::AccountAltRecoveryPlanKind::SyncCloneToSource;
    plan.domainsToSync = sanityCheck.safeDomains;
    plan.reason = "clone is ahead and sanity checks passed";
    return plan;
}
} // namespace service
} // namespace living_world
