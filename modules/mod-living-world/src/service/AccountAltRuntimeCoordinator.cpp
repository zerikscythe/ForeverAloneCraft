#include "service/AccountAltRuntimeCoordinator.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"
#include "service/AccountAltEquipmentSyncExecutor.h"
#include "service/AccountAltBankSyncExecutor.h"
#include "service/AccountAltInventorySyncExecutor.h"
#include "service/AccountAltItemRecoveryService.h"
#include "service/AccountAltSanityChecker.h"
#include "service/AccountAltSyncExecutor.h"
#include "service/CharacterItemSanityChecker.h"

#include <algorithm>

#include <algorithm>
#include <utility>

namespace living_world
{
namespace service
{
namespace
{
AccountAltSpawnDecision BuildBlockedDecision(std::string reason)
{
    AccountAltSpawnDecision decision;
    decision.kind = AccountAltSpawnDecisionKind::Blocked;
    decision.reason = std::move(reason);
    return decision;
}

AccountAltSpawnDecision BuildManualReviewDecision(std::string reason)
{
    AccountAltSpawnDecision decision;
    decision.kind = AccountAltSpawnDecisionKind::ManualReviewRequired;
    decision.reason = std::move(reason);
    return decision;
}

bool CloneRequiresRefresh(
    integration::CharacterCloneLoginState const& state)
{
    return !state.loginNameValid ||
        (state.atLoginFlags & AT_LOGIN_RENAME) != 0;
}

bool SourceProgressIsAhead(
    model::CharacterProgressSnapshot const& source,
    model::CharacterProgressSnapshot const& clone)
{
    if (source.level != clone.level)
        return source.level > clone.level;
    if (source.experience != clone.experience)
        return source.experience > clone.experience;
    if (source.money != clone.money)
        return source.money > clone.money;
    if (source.completedQuestCount != clone.completedQuestCount)
        return source.completedQuestCount > clone.completedQuestCount;
    if (source.achievementCount != clone.achievementCount)
        return source.achievementCount > clone.achievementCount;
    return source.totalReputationStanding > clone.totalReputationStanding;
}
} // namespace

AccountAltRuntimeCoordinator::AccountAltRuntimeCoordinator(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::BotAccountPoolRepository& botAccountPoolRepository,
    integration::CharacterCloneMaterializer& cloneMaterializer,
    integration::CharacterCloneStateGateway const& cloneStateGateway,
    integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
    integration::CharacterInventorySyncRepository& inventorySyncRepository,
    integration::CharacterBankSyncRepository& bankSyncRepository,
    integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
    integration::CharacterProgressSnapshotRepository const& snapshotRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    integration::CharacterReputationSyncRepository& reputationSyncRepository,
    integration::CharacterQuestSyncRepository& questSyncRepository,
    integration::CharacterAchievementSyncRepository& achievementSyncRepository,
    integration::CharacterSkillSyncRepository& skillSyncRepository,
    integration::CharacterSpellSyncRepository& spellSyncRepository,
    AccountAltRecoveryService const& recoveryService,
    AccountAltItemRecoveryOptions itemRecoveryOptions)
    : _runtimeRepository(runtimeRepository),
      _cloneMaterializer(cloneMaterializer),
      _cloneStateGateway(cloneStateGateway),
      _itemSnapshotRepository(itemSnapshotRepository),
      _inventorySyncRepository(inventorySyncRepository),
      _bankSyncRepository(bankSyncRepository),
      _equipmentSyncRepository(equipmentSyncRepository),
      _snapshotRepository(snapshotRepository),
      _syncRepository(syncRepository),
      _reputationSyncRepository(reputationSyncRepository),
      _questSyncRepository(questSyncRepository),
      _achievementSyncRepository(achievementSyncRepository),
      _skillSyncRepository(skillSyncRepository),
      _spellSyncRepository(spellSyncRepository),
      _recoveryService(recoveryService),
      _itemRecoveryOptions(itemRecoveryOptions),
      _runtimeService(runtimeRepository, botAccountPoolRepository)
{
}

AccountAltSpawnDecision AccountAltRuntimeCoordinator::PlanSpawn(
    std::uint32_t sourceAccountId,
    std::uint64_t sourceCharacterGuid,
    std::uint64_t ownerCharacterGuid,
    std::string const& sourceCharacterName) const
{
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] PlanSpawn start sourceAccountId={} sourceGuid={} "
        "ownerGuid={} sourceName='{}'",
        sourceAccountId,
        sourceCharacterGuid,
        ownerCharacterGuid,
        sourceCharacterName);

    std::optional<model::CharacterProgressSnapshot> sourceSnapshot =
        _snapshotRepository.LoadSnapshot(sourceCharacterGuid);
    if (!sourceSnapshot)
    {
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] PlanSpawn failed source snapshot load "
            "sourceAccountId={} sourceGuid={}",
            sourceAccountId,
            sourceCharacterGuid);
        return BuildBlockedDecision("source character snapshot could not be loaded");
    }

    std::optional<model::AccountAltRuntimeRecord> existing =
        _runtimeRepository.FindBySourceCharacter(
            sourceAccountId,
            sourceCharacterGuid);
    if (!existing)
    {
        model::AccountAltRuntimeRequest request;
        request.sourceAccountId = sourceAccountId;
        request.sourceCharacterGuid = sourceCharacterGuid;
        request.ownerCharacterGuid = ownerCharacterGuid;
        request.sourceCharacterName = sourceCharacterName;
        request.sourceSnapshot = *sourceSnapshot;

        model::AccountAltRuntimeDecision runtimeDecision =
            _runtimeService.PrepareRuntime(request);
        if (!runtimeDecision.runtime)
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn runtime preparation failed "
                "sourceAccountId={} sourceGuid={} decisionKind={}",
                sourceAccountId,
                sourceCharacterGuid,
                static_cast<std::uint32_t>(runtimeDecision.kind));
            if (runtimeDecision.kind ==
                model::AccountAltRuntimeDecisionKind::BlockedNoBotAccount)
            {
                return BuildBlockedDecision("no bot account is available");
            }

            return BuildBlockedDecision("runtime could not be prepared");
        }

        AccountAltSpawnDecision spawnDecision;
        model::AccountAltRuntimeRecord runtime = *runtimeDecision.runtime;
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] PlanSpawn prepared new runtime runtimeId={} "
            "cloneAccountId={} reservedName='{}' cloneName='{}'",
            runtime.runtimeId,
            runtime.cloneAccountId,
            runtime.reservedSourceCharacterName,
            runtime.cloneCharacterName);
        integration::CharacterCloneMaterializationResult cloneResult =
            _cloneMaterializer.MaterializeClone(runtime);
        if (!cloneResult.succeeded)
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn clone materialization failed "
                "runtimeId={} cloneAccountId={} reason='{}'",
                runtime.runtimeId,
                runtime.cloneAccountId,
                cloneResult.reason);
            return BuildBlockedDecision(cloneResult.reason);
        }

        runtime.cloneCharacterGuid = cloneResult.cloneCharacterGuid;
        runtime.cloneCharacterName = cloneResult.cloneCharacterName;
        runtime.cloneSnapshot = cloneResult.cloneSnapshot;
        runtime.state = model::AccountAltRuntimeState::Active;
        _runtimeRepository.SaveRuntime(runtime);

        spawnDecision.kind =
            AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
        spawnDecision.runtime = runtime;
        spawnDecision.botAccountId = runtime.cloneAccountId;
        spawnDecision.spawnCharacterGuid = runtime.cloneCharacterGuid;
        spawnDecision.reason = cloneResult.reason;
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] PlanSpawn prepared persistent clone "
            "runtimeId={} cloneAccountId={} cloneGuid={} reason='{}'",
            runtime.runtimeId,
            runtime.cloneAccountId,
            runtime.cloneCharacterGuid,
            spawnDecision.reason);
        return spawnDecision;
    }

    model::AccountAltRuntimeRecord runtime = *existing;
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] PlanSpawn found existing runtime runtimeId={} "
        "state={} cloneAccountId={} cloneGuid={}",
        runtime.runtimeId,
        static_cast<std::uint32_t>(runtime.state),
        runtime.cloneAccountId,
        runtime.cloneCharacterGuid);
    runtime.ownerCharacterGuid = ownerCharacterGuid;
    runtime.sourceSnapshot = *sourceSnapshot;

    if (runtime.cloneCharacterGuid != 0)
    {
        std::optional<integration::CharacterCloneLoginState> cloneLoginState =
            _cloneStateGateway.LoadCloneLoginState(runtime.cloneCharacterGuid);
        if (!cloneLoginState)
        {
            LOG_WARN(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn runtimeId={} cloneGuid={} is "
                "missing from characters DB; rematerializing.",
                runtime.runtimeId,
                runtime.cloneCharacterGuid);
            runtime.cloneCharacterGuid = 0;
            runtime.cloneCharacterName = runtime.sourceCharacterName;
        }
        else if (CloneRequiresRefresh(*cloneLoginState))
        {
            LOG_WARN(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn runtimeId={} cloneGuid={} "
                "cloneName='{}' atLogin={} loginNameValid={} requires "
                "refresh before bot login.",
                runtime.runtimeId,
                runtime.cloneCharacterGuid,
                cloneLoginState->name,
                cloneLoginState->atLoginFlags,
                cloneLoginState->loginNameValid);

            if (!_cloneStateGateway.DeleteOfflineCloneCharacter(
                    runtime.cloneAccountId,
                    runtime.cloneCharacterGuid,
                    cloneLoginState->name))
            {
                return BuildBlockedDecision(
                    "clone requires refresh but could not be deleted offline");
            }

            runtime.cloneCharacterGuid = 0;
            runtime.cloneCharacterName = runtime.sourceCharacterName;
            runtime.cloneSnapshot = {};
        }

        if (runtime.cloneCharacterGuid == 0)
        {
            integration::CharacterCloneMaterializationResult cloneResult =
                _cloneMaterializer.MaterializeClone(runtime);
            if (!cloneResult.succeeded)
            {
                LOG_ERROR(
                    "server.worldserver",
                    "[LivingWorldDebug] PlanSpawn clone refresh failed "
                    "runtimeId={} cloneAccountId={} reason='{}'",
                    runtime.runtimeId,
                    runtime.cloneAccountId,
                    cloneResult.reason);
                return BuildBlockedDecision(cloneResult.reason);
            }

            runtime.cloneCharacterGuid = cloneResult.cloneCharacterGuid;
            runtime.cloneCharacterName = cloneResult.cloneCharacterName;
            runtime.cloneSnapshot = cloneResult.cloneSnapshot;
            runtime.state = model::AccountAltRuntimeState::Active;
            _runtimeRepository.SaveRuntime(runtime);

            AccountAltSpawnDecision refreshedDecision;
            refreshedDecision.kind =
                AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
            refreshedDecision.runtime = runtime;
            refreshedDecision.botAccountId = runtime.cloneAccountId;
            refreshedDecision.spawnCharacterGuid = runtime.cloneCharacterGuid;
            refreshedDecision.reason = cloneResult.reason;
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn refreshed clone runtimeId={} "
                "cloneAccountId={} cloneGuid={} cloneName='{}' reason='{}'",
                runtime.runtimeId,
                runtime.cloneAccountId,
                runtime.cloneCharacterGuid,
                runtime.cloneCharacterName,
                refreshedDecision.reason);
            return refreshedDecision;
        }
    }

    if (runtime.cloneCharacterGuid == 0)
    {
        integration::CharacterCloneMaterializationResult cloneResult =
            _cloneMaterializer.MaterializeClone(runtime);
        if (!cloneResult.succeeded)
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn missing-clone materialization failed "
                "runtimeId={} cloneAccountId={} reason='{}'",
                runtime.runtimeId,
                runtime.cloneAccountId,
                cloneResult.reason);
            return BuildBlockedDecision(cloneResult.reason);
        }

        runtime.cloneCharacterGuid = cloneResult.cloneCharacterGuid;
        runtime.cloneCharacterName = cloneResult.cloneCharacterName;
        runtime.cloneSnapshot = cloneResult.cloneSnapshot;
        runtime.state = model::AccountAltRuntimeState::Active;
        _runtimeRepository.SaveRuntime(runtime);

        AccountAltSpawnDecision decision;
        decision.kind = AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
        decision.runtime = runtime;
        decision.botAccountId = runtime.cloneAccountId;
        decision.spawnCharacterGuid = runtime.cloneCharacterGuid;
        decision.reason = cloneResult.reason;
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] PlanSpawn materialized missing clone "
            "runtimeId={} cloneAccountId={} cloneGuid={} reason='{}'",
            runtime.runtimeId,
            runtime.cloneAccountId,
            runtime.cloneCharacterGuid,
            decision.reason);
        return decision;
    }

    std::optional<model::CharacterProgressSnapshot> cloneSnapshot =
        _snapshotRepository.LoadSnapshot(runtime.cloneCharacterGuid);
    if (!cloneSnapshot)
    {
        LOG_ERROR(
            "server.worldserver",
            "[LivingWorldDebug] PlanSpawn failed clone snapshot load "
            "runtimeId={} cloneGuid={}",
            runtime.runtimeId,
            runtime.cloneCharacterGuid);
        return BuildBlockedDecision("clone snapshot could not be loaded");
    }

    AccountAltSanityChecker checker;
    model::AccountAltSanityCheckResult sanity =
        checker.Check(*sourceSnapshot, *cloneSnapshot);

    model::AccountAltRecoveryPlan recoveryPlan =
        _recoveryService.BuildRecoveryPlan(
            runtime,
            *sourceSnapshot,
            *cloneSnapshot,
            sanity);

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] PlanSpawn recovery plan runtimeId={} planKind={} "
        "reason='{}' sanityPassed={} safeDomains={} failures={}",
        runtime.runtimeId,
        static_cast<std::uint32_t>(recoveryPlan.kind),
        recoveryPlan.reason,
        sanity.passed,
        sanity.safeDomains.size(),
        sanity.failures.size());

    AccountAltSpawnDecision decision;
    decision.runtime = runtime;
    decision.botAccountId = runtime.cloneAccountId;
    decision.spawnCharacterGuid = runtime.cloneCharacterGuid;
    decision.reason = recoveryPlan.reason;

    auto applyItemRecovery =
        [&](model::AccountAltRuntimeRecord const& currentRuntime,
            bool cloneAuthoritativeForItems)
        -> AccountAltSpawnDecision
    {
        std::optional<model::CharacterItemSnapshot> sourceItems =
            _itemSnapshotRepository.LoadSnapshot(currentRuntime.sourceCharacterGuid);
        std::optional<model::CharacterItemSnapshot> cloneItems =
            _itemSnapshotRepository.LoadSnapshot(currentRuntime.cloneCharacterGuid);
        if (!sourceItems || !cloneItems)
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn item snapshot load failed "
                "runtimeId={} sourceGuid={} cloneGuid={}",
                currentRuntime.runtimeId,
                currentRuntime.sourceCharacterGuid,
                currentRuntime.cloneCharacterGuid);
            return BuildBlockedDecision("item snapshots could not be loaded");
        }

        CharacterItemSanityChecker itemChecker;
        model::AccountAltSanityCheckResult itemSanity =
            itemChecker.Check(*sourceItems, *cloneItems);
        AccountAltItemRecoveryService itemRecoveryService(_itemRecoveryOptions);
        model::AccountAltItemRecoveryPlan itemPlan =
            itemRecoveryService.BuildRecoveryPlan(itemSanity);

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] PlanSpawn item recovery runtimeId={} planKind={} "
            "reason='{}' sanityPassed={} safeDomains={} failures={}",
            currentRuntime.runtimeId,
            static_cast<std::uint32_t>(itemPlan.kind),
            itemPlan.reason,
            itemSanity.passed,
            itemSanity.safeDomains.size(),
            itemSanity.failures.size());

        switch (itemPlan.kind)
        {
            case model::AccountAltItemRecoveryPlanKind::NoAction:
            {
                AccountAltSpawnDecision itemDecision;
                itemDecision.kind =
                    AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
                itemDecision.runtime = currentRuntime;
                itemDecision.botAccountId = currentRuntime.cloneAccountId;
                itemDecision.spawnCharacterGuid =
                    currentRuntime.cloneCharacterGuid;
                itemDecision.reason = itemPlan.reason;
                return itemDecision;
            }
            case model::AccountAltItemRecoveryPlanKind::SyncEquipmentToSource:
                if (!cloneAuthoritativeForItems)
                {
                    if (!_cloneStateGateway.DeleteOfflineCloneCharacter(
                            currentRuntime.cloneAccountId,
                            currentRuntime.cloneCharacterGuid,
                            currentRuntime.cloneCharacterName))
                    {
                        return BuildBlockedDecision(
                            "source data is newer but stale clone could not be "
                            "deleted for refresh");
                    }

                    model::AccountAltRuntimeRecord refreshedRuntime = currentRuntime;
                    refreshedRuntime.cloneCharacterGuid = 0;
                    refreshedRuntime.cloneCharacterName =
                        refreshedRuntime.sourceCharacterName;
                    refreshedRuntime.cloneSnapshot = {};

                    integration::CharacterCloneMaterializationResult cloneResult =
                        _cloneMaterializer.MaterializeClone(refreshedRuntime);
                    if (!cloneResult.succeeded)
                    {
                        return BuildBlockedDecision(cloneResult.reason);
                    }

                    refreshedRuntime.cloneCharacterGuid = cloneResult.cloneCharacterGuid;
                    refreshedRuntime.cloneCharacterName = cloneResult.cloneCharacterName;
                    refreshedRuntime.cloneSnapshot = cloneResult.cloneSnapshot;
                    refreshedRuntime.state = model::AccountAltRuntimeState::Active;
                    _runtimeRepository.SaveRuntime(refreshedRuntime);

                    AccountAltSpawnDecision itemDecision;
                    itemDecision.kind =
                        AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
                    itemDecision.runtime = refreshedRuntime;
                    itemDecision.botAccountId = refreshedRuntime.cloneAccountId;
                    itemDecision.spawnCharacterGuid =
                        refreshedRuntime.cloneCharacterGuid;
                    itemDecision.reason =
                        "source data is newer; refreshed persistent clone before "
                        "spawn";
                    return itemDecision;
                }

            {
                AccountAltEquipmentSyncExecutor equipmentExecutor(
                    _runtimeRepository,
                    _equipmentSyncRepository);
                if (!equipmentExecutor.Execute(
                        currentRuntime,
                        *sourceItems,
                        *cloneItems))
                {
                    LOG_ERROR(
                        "server.worldserver",
                        "[LivingWorldDebug] PlanSpawn equipment sync failed "
                        "runtimeId={}",
                        currentRuntime.runtimeId);
                    return BuildManualReviewDecision(
                        "equipment sync execution failed");
                }

                AccountAltSpawnDecision itemDecision;
                itemDecision.kind =
                    AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
                itemDecision.runtime = currentRuntime;
                itemDecision.botAccountId = currentRuntime.cloneAccountId;
                itemDecision.spawnCharacterGuid =
                    currentRuntime.cloneCharacterGuid;
                itemDecision.reason = itemPlan.reason;
                return itemDecision;
            }
            case model::AccountAltItemRecoveryPlanKind::SyncBagDomainsToSource:
                if (!cloneAuthoritativeForItems)
                {
                    if (!_cloneStateGateway.DeleteOfflineCloneCharacter(
                            currentRuntime.cloneAccountId,
                            currentRuntime.cloneCharacterGuid,
                            currentRuntime.cloneCharacterName))
                    {
                        return BuildBlockedDecision(
                            "source data is newer but stale clone could not be "
                            "deleted for refresh");
                    }

                    model::AccountAltRuntimeRecord refreshedRuntime = currentRuntime;
                    refreshedRuntime.cloneCharacterGuid = 0;
                    refreshedRuntime.cloneCharacterName =
                        refreshedRuntime.sourceCharacterName;
                    refreshedRuntime.cloneSnapshot = {};

                    integration::CharacterCloneMaterializationResult cloneResult =
                        _cloneMaterializer.MaterializeClone(refreshedRuntime);
                    if (!cloneResult.succeeded)
                    {
                        return BuildBlockedDecision(cloneResult.reason);
                    }

                    refreshedRuntime.cloneCharacterGuid = cloneResult.cloneCharacterGuid;
                    refreshedRuntime.cloneCharacterName = cloneResult.cloneCharacterName;
                    refreshedRuntime.cloneSnapshot = cloneResult.cloneSnapshot;
                    refreshedRuntime.state = model::AccountAltRuntimeState::Active;
                    _runtimeRepository.SaveRuntime(refreshedRuntime);

                    AccountAltSpawnDecision itemDecision;
                    itemDecision.kind =
                        AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
                    itemDecision.runtime = refreshedRuntime;
                    itemDecision.botAccountId = refreshedRuntime.cloneAccountId;
                    itemDecision.spawnCharacterGuid =
                        refreshedRuntime.cloneCharacterGuid;
                    itemDecision.reason =
                        "source data is newer; refreshed persistent clone before "
                        "spawn";
                    return itemDecision;
                }

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
                    if (!inventoryExecutor.Execute(
                            currentRuntime,
                            *sourceItems,
                            *cloneItems))
                    {
                        LOG_ERROR(
                            "server.worldserver",
                            "[LivingWorldDebug] PlanSpawn inventory sync failed "
                            "runtimeId={}",
                            currentRuntime.runtimeId);
                        return BuildManualReviewDecision(
                            "inventory sync execution failed");
                    }
                }

                if (shouldSyncDomain(model::AccountAltSyncDomain::Bank))
                {
                    AccountAltBankSyncExecutor bankExecutor(
                        _runtimeRepository,
                        _bankSyncRepository);
                    if (!bankExecutor.Execute(
                            currentRuntime,
                            *sourceItems,
                            *cloneItems))
                    {
                        LOG_ERROR(
                            "server.worldserver",
                            "[LivingWorldDebug] PlanSpawn bank sync failed "
                            "runtimeId={}",
                            currentRuntime.runtimeId);
                        return BuildManualReviewDecision(
                            "bank sync execution failed");
                    }
                }

                AccountAltSpawnDecision itemDecision;
                itemDecision.kind =
                    AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
                itemDecision.runtime = currentRuntime;
                itemDecision.botAccountId = currentRuntime.cloneAccountId;
                itemDecision.spawnCharacterGuid =
                    currentRuntime.cloneCharacterGuid;
                itemDecision.reason = itemPlan.reason;
                return itemDecision;
            }
            case model::AccountAltItemRecoveryPlanKind::ManualReview:
                return BuildManualReviewDecision(itemPlan.reason);
            case model::AccountAltItemRecoveryPlanKind::Blocked:
                return BuildBlockedDecision(itemPlan.reason);
        }

        return BuildBlockedDecision("unknown item recovery decision");
    };

    switch (recoveryPlan.kind)
    {
        case model::AccountAltRecoveryPlanKind::ReuseClone:
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn reusing clone runtimeId={} cloneGuid={}",
                runtime.runtimeId,
                runtime.cloneCharacterGuid);

            if (SourceProgressIsAhead(*sourceSnapshot, *cloneSnapshot))
            {
                if (!_cloneStateGateway.DeleteOfflineCloneCharacter(
                        runtime.cloneAccountId,
                        runtime.cloneCharacterGuid,
                        runtime.cloneCharacterName))
                {
                    return BuildBlockedDecision(
                        "source progress is newer but stale clone could not be "
                        "deleted for refresh");
                }

                model::AccountAltRuntimeRecord refreshedRuntime = runtime;
                refreshedRuntime.cloneCharacterGuid = 0;
                refreshedRuntime.cloneCharacterName =
                    refreshedRuntime.sourceCharacterName;
                refreshedRuntime.cloneSnapshot = {};

                integration::CharacterCloneMaterializationResult cloneResult =
                    _cloneMaterializer.MaterializeClone(refreshedRuntime);
                if (!cloneResult.succeeded)
                {
                    return BuildBlockedDecision(cloneResult.reason);
                }

                refreshedRuntime.cloneCharacterGuid = cloneResult.cloneCharacterGuid;
                refreshedRuntime.cloneCharacterName = cloneResult.cloneCharacterName;
                refreshedRuntime.cloneSnapshot = cloneResult.cloneSnapshot;
                refreshedRuntime.state = model::AccountAltRuntimeState::Active;
                _runtimeRepository.SaveRuntime(refreshedRuntime);

                AccountAltSpawnDecision refreshedDecision;
                refreshedDecision.kind =
                    AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
                refreshedDecision.runtime = refreshedRuntime;
                refreshedDecision.botAccountId = refreshedRuntime.cloneAccountId;
                refreshedDecision.spawnCharacterGuid =
                    refreshedRuntime.cloneCharacterGuid;
                refreshedDecision.reason =
                    "source progress is newer; refreshed persistent clone before "
                    "spawn";
                return refreshedDecision;
            }

            return applyItemRecovery(runtime, true);
        case model::AccountAltRecoveryPlanKind::SyncCloneToSource:
        {
            AccountAltSyncExecutor executor(
                _runtimeRepository, _syncRepository,
                _reputationSyncRepository, _questSyncRepository,
                _achievementSyncRepository, _skillSyncRepository,
                _spellSyncRepository);
            if (!executor.Execute(runtime, *cloneSnapshot, recoveryPlan.domainsToSync))
            {
                LOG_ERROR(
                    "server.worldserver",
                    "[LivingWorldDebug] PlanSpawn progress sync failed "
                    "runtimeId={} cloneGuid={} domains={}",
                    runtime.runtimeId,
                    runtime.cloneCharacterGuid,
                    recoveryPlan.domainsToSync.size());
                decision.kind = AccountAltSpawnDecisionKind::ManualReviewRequired;
                decision.reason = "sync execution failed; manual review required";
                return decision;
            }
            runtime.sourceSnapshot = *cloneSnapshot;
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] PlanSpawn progress sync succeeded "
                "runtimeId={} cloneGuid={} domains={}",
                runtime.runtimeId,
                runtime.cloneCharacterGuid,
                recoveryPlan.domainsToSync.size());
            return applyItemRecovery(runtime, true);
        }
        case model::AccountAltRecoveryPlanKind::ManualReview:
            decision.kind = AccountAltSpawnDecisionKind::ManualReviewRequired;
            return decision;
        case model::AccountAltRecoveryPlanKind::Blocked:
            decision.kind = AccountAltSpawnDecisionKind::Blocked;
            return decision;
        case model::AccountAltRecoveryPlanKind::NoAction:
            decision.kind = AccountAltSpawnDecisionKind::Blocked;
            decision.reason = "no runtime action was selected";
            return decision;
    }

    decision.kind = AccountAltSpawnDecisionKind::Blocked;
    decision.reason = "unknown runtime spawn decision";
    return decision;
}
} // namespace service
} // namespace living_world
