#include "service/AccountAltStartupRecoveryService.h"

#include "service/AccountAltSanityChecker.h"
#include "service/AccountAltSyncExecutor.h"

namespace living_world
{
namespace service
{
AccountAltStartupRecoveryService::AccountAltStartupRecoveryService(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterProgressSnapshotRepository const& snapshotRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    AccountAltRecoveryService const& recoveryService)
    : _runtimeRepository(runtimeRepository),
      _snapshotRepository(snapshotRepository),
      _syncRepository(syncRepository),
      _recoveryService(recoveryService)
{
}

AccountAltStartupRecoverySummary
AccountAltStartupRecoveryService::RecoverForAccount(
    std::uint32_t sourceAccountId)
{
    AccountAltStartupRecoverySummary summary;
    AccountAltSanityChecker checker;
    AccountAltSyncExecutor executor(_runtimeRepository, _syncRepository);

    for (model::AccountAltRuntimeRecord const& runtime :
         _runtimeRepository.ListRecoverableForAccount(sourceAccountId))
    {
        ++summary.scanned;

        if (runtime.state == model::AccountAltRuntimeState::Failed)
        {
            ++summary.manualReviewRequired;
            continue;
        }

        if (runtime.cloneCharacterGuid == 0)
        {
            if (runtime.state != model::AccountAltRuntimeState::Active)
            {
                ++summary.blocked;
            }
            continue;
        }

        std::optional<model::CharacterProgressSnapshot> sourceSnapshot =
            _snapshotRepository.LoadSnapshot(runtime.sourceCharacterGuid);
        std::optional<model::CharacterProgressSnapshot> cloneSnapshot =
            _snapshotRepository.LoadSnapshot(runtime.cloneCharacterGuid);
        if (!sourceSnapshot || !cloneSnapshot)
        {
            ++summary.blocked;
            continue;
        }

        model::AccountAltSanityCheckResult sanity =
            checker.Check(*sourceSnapshot, *cloneSnapshot);

        if (runtime.state == model::AccountAltRuntimeState::SyncingBack)
        {
            if (!sanity.passed)
            {
                ++summary.manualReviewRequired;
                continue;
            }

            if (executor.Execute(runtime, *cloneSnapshot, sanity.safeDomains))
            {
                ++summary.recoveredSyncs;
            }
            else
            {
                ++summary.manualReviewRequired;
            }
            continue;
        }

        model::AccountAltRecoveryPlan recoveryPlan =
            _recoveryService.BuildRecoveryPlan(
                runtime,
                *sourceSnapshot,
                *cloneSnapshot,
                sanity);
        switch (recoveryPlan.kind)
        {
            case model::AccountAltRecoveryPlanKind::SyncCloneToSource:
                ++summary.pendingRecovery;
                break;
            case model::AccountAltRecoveryPlanKind::ManualReview:
                ++summary.manualReviewRequired;
                break;
            case model::AccountAltRecoveryPlanKind::Blocked:
                ++summary.blocked;
                break;
            case model::AccountAltRecoveryPlanKind::ReuseClone:
            case model::AccountAltRecoveryPlanKind::NoAction:
                break;
        }
    }

    return summary;
}
} // namespace service
} // namespace living_world
