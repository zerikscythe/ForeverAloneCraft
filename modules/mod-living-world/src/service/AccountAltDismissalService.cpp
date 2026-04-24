#include "service/AccountAltDismissalService.h"

#include "service/AccountAltBankSyncExecutor.h"
#include "service/AccountAltEquipmentSyncExecutor.h"
#include "service/AccountAltInventorySyncExecutor.h"
#include "service/AccountAltItemRecoveryService.h"
#include "service/AccountAltSanityChecker.h"
#include "service/AccountAltSyncExecutor.h"
#include "service/CharacterItemSanityChecker.h"

#include <algorithm>

namespace living_world
{
namespace service
{
AccountAltDismissalService::AccountAltDismissalService(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
    integration::CharacterInventorySyncRepository& inventorySyncRepository,
    integration::CharacterBankSyncRepository& bankSyncRepository,
    integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
    integration::CharacterNameLeaseRepository& nameLeaseRepository,
    integration::CharacterProgressSnapshotRepository const& snapshotRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    AccountAltRecoveryService const& recoveryService,
    AccountAltItemRecoveryOptions itemRecoveryOptions)
    : _runtimeRepository(runtimeRepository),
      _itemSnapshotRepository(itemSnapshotRepository),
      _inventorySyncRepository(inventorySyncRepository),
      _bankSyncRepository(bankSyncRepository),
      _equipmentSyncRepository(equipmentSyncRepository),
      _nameLeaseRepository(nameLeaseRepository),
      _snapshotRepository(snapshotRepository),
      _syncRepository(syncRepository),
      _recoveryService(recoveryService),
      _itemRecoveryOptions(itemRecoveryOptions)
{
}

AccountAltDismissalSummary AccountAltDismissalService::DismissClone(
    std::uint64_t cloneCharacterGuid) const
{
    AccountAltDismissalSummary summary;

    std::optional<model::AccountAltRuntimeRecord> runtime =
        _runtimeRepository.FindByCloneCharacter(cloneCharacterGuid);
    if (!runtime)
    {
        summary.reason = "no runtime found";
        return summary;
    }

    summary.runtimeFound = true;

    std::optional<model::CharacterProgressSnapshot> sourceSnapshot =
        _snapshotRepository.LoadSnapshot(runtime->sourceCharacterGuid);
    std::optional<model::CharacterProgressSnapshot> cloneSnapshot =
        _snapshotRepository.LoadSnapshot(runtime->cloneCharacterGuid);
    if (!sourceSnapshot || !cloneSnapshot)
    {
        summary.blocked = true;
        summary.reason = "progress snapshots could not be loaded";
        summary.namesRestored =
            _nameLeaseRepository.RestoreSourceNameLease(*runtime);
        return summary;
    }

    AccountAltSanityChecker checker;
    model::AccountAltSanityCheckResult sanity =
        checker.Check(*sourceSnapshot, *cloneSnapshot);
    model::AccountAltRecoveryPlan recoveryPlan =
        _recoveryService.BuildRecoveryPlan(
            *runtime,
            *sourceSnapshot,
            *cloneSnapshot,
            sanity);

    if (recoveryPlan.kind == model::AccountAltRecoveryPlanKind::SyncCloneToSource)
    {
        AccountAltSyncExecutor executor(_runtimeRepository, _syncRepository);
        if (!executor.Execute(*runtime, *cloneSnapshot, recoveryPlan.domainsToSync))
        {
            summary.manualReviewRequired = true;
            summary.reason = "progress sync execution failed";
        }
        else
        {
            summary.progressSynced = true;
        }
    }
    else if (recoveryPlan.kind == model::AccountAltRecoveryPlanKind::ManualReview)
    {
        summary.manualReviewRequired = true;
        summary.reason = recoveryPlan.reason;
    }
    else if (recoveryPlan.kind == model::AccountAltRecoveryPlanKind::Blocked)
    {
        summary.blocked = true;
        summary.reason = recoveryPlan.reason;
    }

    std::optional<model::CharacterItemSnapshot> sourceItems =
        _itemSnapshotRepository.LoadSnapshot(runtime->sourceCharacterGuid);
    std::optional<model::CharacterItemSnapshot> cloneItems =
        _itemSnapshotRepository.LoadSnapshot(runtime->cloneCharacterGuid);
    if (sourceItems && cloneItems)
    {
        CharacterItemSanityChecker itemChecker;
        model::AccountAltSanityCheckResult itemSanity =
            itemChecker.Check(*sourceItems, *cloneItems);
        AccountAltItemRecoveryService itemRecoveryService(_itemRecoveryOptions);
        model::AccountAltItemRecoveryPlan itemPlan =
            itemRecoveryService.BuildRecoveryPlan(itemSanity);

        if (itemPlan.kind ==
            model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource)
        {
            AccountAltEquipmentSyncExecutor equipmentExecutor(
                _runtimeRepository,
                _equipmentSyncRepository);
            if (!equipmentExecutor.Execute(*runtime, *sourceItems, *cloneItems))
            {
                summary.manualReviewRequired = true;
                if (summary.reason.empty())
                {
                    summary.reason = "equipment sync execution failed";
                }
            }
            else
            {
                summary.equipmentSynced = true;
            }
        }
        else if (itemPlan.kind ==
                 model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource)
        {
            auto const shouldSyncDomain =
                [&](model::AccountAltSyncDomain domain) -> bool
            {
                return std::find(
                           itemPlan.domainsToSync.begin(),
                           itemPlan.domainsToSync.end(),
                           domain) != itemPlan.domainsToSync.end();
            };

            if (shouldSyncDomain(model::AccountAltSyncDomain::Inventory))
            {
                AccountAltInventorySyncExecutor inventoryExecutor(
                    _runtimeRepository,
                    _inventorySyncRepository);
                if (!inventoryExecutor.Execute(*runtime, *sourceItems, *cloneItems))
                {
                    summary.manualReviewRequired = true;
                    if (summary.reason.empty())
                    {
                        summary.reason = "inventory sync execution failed";
                    }
                }
            }

            if (shouldSyncDomain(model::AccountAltSyncDomain::Bank))
            {
                AccountAltBankSyncExecutor bankExecutor(
                    _runtimeRepository,
                    _bankSyncRepository);
                if (!bankExecutor.Execute(*runtime, *sourceItems, *cloneItems))
                {
                    summary.manualReviewRequired = true;
                    if (summary.reason.empty())
                    {
                        summary.reason = "bank sync execution failed";
                    }
                }
            }
        }
        else if (itemPlan.kind ==
                 model::AccountAltItemRecoveryPlanKind::ManualReview)
        {
            summary.manualReviewRequired = true;
            if (summary.reason.empty())
            {
                summary.reason = itemPlan.reason;
            }
        }
        else if (itemPlan.kind == model::AccountAltItemRecoveryPlanKind::Blocked)
        {
            summary.blocked = true;
            if (summary.reason.empty())
            {
                summary.reason = itemPlan.reason;
            }
        }
    }
    else if (!summary.blocked && !summary.manualReviewRequired)
    {
        summary.blocked = true;
        summary.reason = "item snapshots could not be loaded";
    }

    summary.namesRestored = _nameLeaseRepository.RestoreSourceNameLease(*runtime);
    return summary;
}
} // namespace service
} // namespace living_world
