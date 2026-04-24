#include "service/AccountAltStartupRecoveryService.h"

#include "service/AccountAltEquipmentSyncExecutor.h"
#include "service/AccountAltItemRecoveryService.h"
#include "service/AccountAltSanityChecker.h"
#include "service/AccountAltSyncExecutor.h"
#include "service/CharacterItemSanityChecker.h"

namespace living_world
{
namespace service
{
AccountAltStartupRecoveryService::AccountAltStartupRecoveryService(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
    integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
    integration::CharacterProgressSnapshotRepository const& snapshotRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    AccountAltRecoveryService const& recoveryService)
    : _runtimeRepository(runtimeRepository),
      _itemSnapshotRepository(itemSnapshotRepository),
      _equipmentSyncRepository(equipmentSyncRepository),
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
    CharacterItemSanityChecker itemChecker;
    AccountAltItemRecoveryService itemRecoveryService;
    AccountAltEquipmentSyncExecutor equipmentExecutor(
        _runtimeRepository,
        _equipmentSyncRepository);

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

        std::optional<model::CharacterItemSnapshot> sourceItems =
            _itemSnapshotRepository.LoadSnapshot(runtime.sourceCharacterGuid);
        std::optional<model::CharacterItemSnapshot> cloneItems =
            _itemSnapshotRepository.LoadSnapshot(runtime.cloneCharacterGuid);
        if (!sourceItems || !cloneItems)
        {
            ++summary.blocked;
            continue;
        }

        model::AccountAltSanityCheckResult itemSanity =
            itemChecker.Check(*sourceItems, *cloneItems);

        if (runtime.state == model::AccountAltRuntimeState::SyncingEquipment)
        {
            model::AccountAltItemRecoveryPlan itemPlan =
                itemRecoveryService.BuildRecoveryPlan(itemSanity);
            if (itemPlan.kind !=
                model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource)
            {
                ++summary.manualReviewRequired;
                continue;
            }

            if (equipmentExecutor.Execute(runtime, *sourceItems, *cloneItems))
            {
                ++summary.recoveredSyncs;
            }
            else
            {
                ++summary.manualReviewRequired;
            }
            continue;
        }

        if (runtime.state == model::AccountAltRuntimeState::SyncingInventory ||
            runtime.state == model::AccountAltRuntimeState::SyncingBank)
        {
            ++summary.blocked;
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

        if (recoveryPlan.kind != model::AccountAltRecoveryPlanKind::ReuseClone)
        {
            continue;
        }

        model::AccountAltItemRecoveryPlan itemPlan =
            itemRecoveryService.BuildRecoveryPlan(itemSanity);
        switch (itemPlan.kind)
        {
            case model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource:
                ++summary.pendingRecovery;
                break;
            case model::AccountAltItemRecoveryPlanKind::ManualReview:
                ++summary.manualReviewRequired;
                break;
            case model::AccountAltItemRecoveryPlanKind::Blocked:
                ++summary.blocked;
                break;
            case model::AccountAltItemRecoveryPlanKind::NoAction:
                break;
        }
    }

    return summary;
}
} // namespace service
} // namespace living_world
