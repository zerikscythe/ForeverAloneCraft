#include "service/AccountAltStartupRecoveryService.h"

#include "service/AccountAltBankSyncExecutor.h"
#include "service/AccountAltEquipmentSyncExecutor.h"
#include "service/AccountAltInventorySyncExecutor.h"
#include "service/AccountAltItemRecoveryService.h"
#include "service/AccountAltSanityChecker.h"
#include "service/AccountAltSyncExecutor.h"
#include "service/CharacterItemSanityChecker.h"
#include "Log.h"

#include <algorithm>

namespace living_world
{
namespace service
{
namespace
{
char const* ToStateText(model::AccountAltRuntimeState state)
{
    switch (state)
    {
        case model::AccountAltRuntimeState::PreparingClone:
            return "PreparingClone";
        case model::AccountAltRuntimeState::Active:
            return "Active";
        case model::AccountAltRuntimeState::SyncingBack:
            return "SyncingBack";
        case model::AccountAltRuntimeState::Recovering:
            return "Recovering";
        case model::AccountAltRuntimeState::Failed:
            return "Failed";
        case model::AccountAltRuntimeState::SyncingEquipment:
            return "SyncingEquipment";
        case model::AccountAltRuntimeState::SyncingInventory:
            return "SyncingInventory";
        case model::AccountAltRuntimeState::SyncingBank:
            return "SyncingBank";
    }

    return "Unknown";
}

char const* ToRecoveryPlanText(model::AccountAltRecoveryPlanKind kind)
{
    switch (kind)
    {
        case model::AccountAltRecoveryPlanKind::NoAction:
            return "NoAction";
        case model::AccountAltRecoveryPlanKind::ReuseClone:
            return "ReuseClone";
        case model::AccountAltRecoveryPlanKind::SyncCloneToSource:
            return "SyncCloneToSource";
        case model::AccountAltRecoveryPlanKind::ManualReview:
            return "ManualReview";
        case model::AccountAltRecoveryPlanKind::Blocked:
            return "Blocked";
    }

    return "Unknown";
}

char const* ToItemPlanText(model::AccountAltItemRecoveryPlanKind kind)
{
    switch (kind)
    {
        case model::AccountAltItemRecoveryPlanKind::NoAction:
            return "NoAction";
        case model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource:
            return "SyncEquipmentToSource";
        case model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource:
            return "SyncBagDomainsToSource";
        case model::AccountAltItemRecoveryPlanKind::ManualReview:
            return "ManualReview";
        case model::AccountAltItemRecoveryPlanKind::Blocked:
            return "Blocked";
    }

    return "Unknown";
}
} // namespace

AccountAltStartupRecoveryService::AccountAltStartupRecoveryService(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
    integration::CharacterInventorySyncRepository& inventorySyncRepository,
    integration::CharacterBankSyncRepository& bankSyncRepository,
    integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
    integration::CharacterProgressSnapshotRepository const& snapshotRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    AccountAltRecoveryService const& recoveryService,
    AccountAltItemRecoveryOptions itemRecoveryOptions)
    : _runtimeRepository(runtimeRepository),
      _itemSnapshotRepository(itemSnapshotRepository),
      _inventorySyncRepository(inventorySyncRepository),
      _bankSyncRepository(bankSyncRepository),
      _equipmentSyncRepository(equipmentSyncRepository),
      _snapshotRepository(snapshotRepository),
      _syncRepository(syncRepository),
      _recoveryService(recoveryService),
      _itemRecoveryOptions(itemRecoveryOptions)
{
}

AccountAltStartupRecoverySummary
AccountAltStartupRecoveryService::RecoverForAccount(
    std::uint32_t sourceAccountId)
{
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] StartupRecovery start sourceAccountId={}",
        sourceAccountId);

    AccountAltStartupRecoverySummary summary;
    AccountAltSanityChecker checker;
    AccountAltSyncExecutor executor(_runtimeRepository, _syncRepository);
    CharacterItemSanityChecker itemChecker;
    AccountAltItemRecoveryService itemRecoveryService(_itemRecoveryOptions);
    AccountAltEquipmentSyncExecutor equipmentExecutor(
        _runtimeRepository,
        _equipmentSyncRepository);
    AccountAltInventorySyncExecutor inventoryExecutor(
        _runtimeRepository,
        _inventorySyncRepository);
    AccountAltBankSyncExecutor bankExecutor(
        _runtimeRepository,
        _bankSyncRepository);

    for (model::AccountAltRuntimeRecord const& runtime :
         _runtimeRepository.ListRecoverableForAccount(sourceAccountId))
    {
        ++summary.scanned;
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] StartupRecovery runtime runtimeId={} state={} "
            "sourceGuid={} cloneGuid={} sourceName='{}' cloneName='{}'",
            runtime.runtimeId,
            ToStateText(runtime.state),
            runtime.sourceCharacterGuid,
            runtime.cloneCharacterGuid,
            runtime.sourceCharacterName,
            runtime.cloneCharacterName);

        if (runtime.state == model::AccountAltRuntimeState::Failed)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] StartupRecovery runtimeId={} result=ManualReview "
                "reason='runtime is marked failed'",
                runtime.runtimeId);
            ++summary.manualReviewRequired;
            continue;
        }

        if (runtime.cloneCharacterGuid == 0)
        {
            if (runtime.state != model::AccountAltRuntimeState::Active)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} result=Blocked "
                    "reason='runtime clone identity is incomplete' state={}",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
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
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] StartupRecovery runtimeId={} result=Blocked "
                "reason='progress snapshots could not be loaded'",
                runtime.runtimeId);
            ++summary.blocked;
            continue;
        }

        model::AccountAltSanityCheckResult sanity =
            checker.Check(*sourceSnapshot, *cloneSnapshot);

        if (runtime.state == model::AccountAltRuntimeState::SyncingBack)
        {
            if (!sanity.passed)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=ManualReview reason='progress sanity checks failed'",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.manualReviewRequired;
                continue;
            }

            if (executor.Execute(runtime, *cloneSnapshot, sanity.safeDomains))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=RecoveredProgressSync",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.recoveredSyncs;
                ++summary.recoveredProgressSyncs;
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=ManualReview reason='progress sync execution failed'",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
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
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] StartupRecovery runtimeId={} result=Blocked "
                "reason='item snapshots could not be loaded'",
                runtime.runtimeId);
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
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=ManualReview itemPlan={} reason='{}'",
                    runtime.runtimeId,
                    ToStateText(runtime.state),
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason);
                ++summary.manualReviewRequired;
                continue;
            }

            if (equipmentExecutor.Execute(runtime, *sourceItems, *cloneItems))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=RecoveredEquipmentSync",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.recoveredSyncs;
                ++summary.recoveredEquipmentSyncs;
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=ManualReview reason='equipment sync execution failed'",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.manualReviewRequired;
            }
            continue;
        }

        if (runtime.state == model::AccountAltRuntimeState::SyncingInventory)
        {
            model::AccountAltItemRecoveryPlan itemPlan =
                itemRecoveryService.BuildRecoveryPlan(itemSanity);
            if (itemPlan.kind !=
                    model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource ||
                std::find(
                    itemPlan.domainsToSync.begin(),
                    itemPlan.domainsToSync.end(),
                    model::AccountAltSyncDomain::Inventory) ==
                    itemPlan.domainsToSync.end())
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=Blocked itemPlan={} reason='{}'",
                    runtime.runtimeId,
                    ToStateText(runtime.state),
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason);
                ++summary.blocked;
                continue;
            }

            if (inventoryExecutor.Execute(runtime, *sourceItems, *cloneItems))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=RecoveredInventorySync",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.recoveredSyncs;
                ++summary.recoveredInventorySyncs;
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=ManualReview reason='inventory sync execution failed'",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.manualReviewRequired;
            }
            continue;
        }

        if (runtime.state == model::AccountAltRuntimeState::SyncingBank)
        {
            model::AccountAltItemRecoveryPlan itemPlan =
                itemRecoveryService.BuildRecoveryPlan(itemSanity);
            if (itemPlan.kind !=
                    model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource ||
                std::find(
                    itemPlan.domainsToSync.begin(),
                    itemPlan.domainsToSync.end(),
                    model::AccountAltSyncDomain::Bank) ==
                    itemPlan.domainsToSync.end())
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=Blocked itemPlan={} reason='{}'",
                    runtime.runtimeId,
                    ToStateText(runtime.state),
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason);
                ++summary.blocked;
                continue;
            }

            if (bankExecutor.Execute(runtime, *sourceItems, *cloneItems))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=RecoveredBankSync",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
                ++summary.recoveredSyncs;
                ++summary.recoveredBankSyncs;
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} state={} "
                    "result=ManualReview reason='bank sync execution failed'",
                    runtime.runtimeId,
                    ToStateText(runtime.state));
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
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} recoveryPlan={} "
                    "reason='{}' cloneAhead={}",
                    runtime.runtimeId,
                    ToRecoveryPlanText(recoveryPlan.kind),
                    recoveryPlan.reason,
                    recoveryPlan.cloneProgressIsAhead);
                ++summary.pendingRecovery;
                break;
            case model::AccountAltRecoveryPlanKind::ManualReview:
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} recoveryPlan={} "
                    "reason='{}'",
                    runtime.runtimeId,
                    ToRecoveryPlanText(recoveryPlan.kind),
                    recoveryPlan.reason);
                ++summary.manualReviewRequired;
                break;
            case model::AccountAltRecoveryPlanKind::Blocked:
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} recoveryPlan={} "
                    "reason='{}'",
                    runtime.runtimeId,
                    ToRecoveryPlanText(recoveryPlan.kind),
                    recoveryPlan.reason);
                ++summary.blocked;
                break;
            case model::AccountAltRecoveryPlanKind::ReuseClone:
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} recoveryPlan={} "
                    "reason='{}'",
                    runtime.runtimeId,
                    ToRecoveryPlanText(recoveryPlan.kind),
                    recoveryPlan.reason);
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
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} itemPlan={} "
                    "reason='{}'",
                    runtime.runtimeId,
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason);
                ++summary.pendingRecovery;
                break;
            case model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource:
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} itemPlan={} "
                    "reason='{}' bagContainersChanged={}",
                    runtime.runtimeId,
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason,
                    itemSanity.bagContainersChanged);
                ++summary.pendingRecovery;
                break;
            case model::AccountAltItemRecoveryPlanKind::ManualReview:
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} itemPlan={} "
                    "reason='{}'",
                    runtime.runtimeId,
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason);
                ++summary.manualReviewRequired;
                break;
            case model::AccountAltItemRecoveryPlanKind::Blocked:
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] StartupRecovery runtimeId={} itemPlan={} "
                    "reason='{}'",
                    runtime.runtimeId,
                    ToItemPlanText(itemPlan.kind),
                    itemPlan.reason);
                ++summary.blocked;
                break;
            case model::AccountAltItemRecoveryPlanKind::NoAction:
                break;
        }
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] StartupRecovery summary sourceAccountId={} scanned={} "
        "recoveredSyncs={} progress={} equipment={} inventory={} bank={} "
        "pending={} manualReview={} blocked={}",
        sourceAccountId,
        summary.scanned,
        summary.recoveredSyncs,
        summary.recoveredProgressSyncs,
        summary.recoveredEquipmentSyncs,
        summary.recoveredInventorySyncs,
        summary.recoveredBankSyncs,
        summary.pendingRecovery,
        summary.manualReviewRequired,
        summary.blocked);

    return summary;
}
} // namespace service
} // namespace living_world
