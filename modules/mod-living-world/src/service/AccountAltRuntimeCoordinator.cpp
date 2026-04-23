#include "service/AccountAltRuntimeCoordinator.h"
#include "service/AccountAltSanityChecker.h"
#include "service/AccountAltSyncExecutor.h"

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
} // namespace

AccountAltRuntimeCoordinator::AccountAltRuntimeCoordinator(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::BotAccountPoolRepository& botAccountPoolRepository,
    integration::CharacterCloneMaterializer& cloneMaterializer,
    integration::CharacterProgressSnapshotRepository const& snapshotRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    AccountAltRecoveryService const& recoveryService)
    : _runtimeRepository(runtimeRepository),
      _cloneMaterializer(cloneMaterializer),
      _snapshotRepository(snapshotRepository),
      _syncRepository(syncRepository),
      _recoveryService(recoveryService),
      _runtimeService(runtimeRepository, botAccountPoolRepository)
{
}

AccountAltSpawnDecision AccountAltRuntimeCoordinator::PlanSpawn(
    std::uint32_t sourceAccountId,
    std::uint64_t sourceCharacterGuid,
    std::uint64_t ownerCharacterGuid,
    std::string const& sourceCharacterName) const
{
    std::optional<model::CharacterProgressSnapshot> sourceSnapshot =
        _snapshotRepository.LoadSnapshot(sourceCharacterGuid);
    if (!sourceSnapshot)
    {
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
            if (runtimeDecision.kind ==
                model::AccountAltRuntimeDecisionKind::BlockedNoBotAccount)
            {
                return BuildBlockedDecision("no bot account is available");
            }

            return BuildBlockedDecision("runtime could not be prepared");
        }

        AccountAltSpawnDecision spawnDecision;
        model::AccountAltRuntimeRecord runtime = *runtimeDecision.runtime;
        integration::CharacterCloneMaterializationResult cloneResult =
            _cloneMaterializer.MaterializeClone(runtime);
        if (!cloneResult.succeeded)
        {
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
        return spawnDecision;
    }

    model::AccountAltRuntimeRecord runtime = *existing;
    runtime.ownerCharacterGuid = ownerCharacterGuid;
    runtime.sourceSnapshot = *sourceSnapshot;

    if (runtime.cloneCharacterGuid == 0)
    {
        integration::CharacterCloneMaterializationResult cloneResult =
            _cloneMaterializer.MaterializeClone(runtime);
        if (!cloneResult.succeeded)
        {
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
        return decision;
    }

    std::optional<model::CharacterProgressSnapshot> cloneSnapshot =
        _snapshotRepository.LoadSnapshot(runtime.cloneCharacterGuid);
    if (!cloneSnapshot)
    {
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

    AccountAltSpawnDecision decision;
    decision.runtime = runtime;
    decision.botAccountId = runtime.cloneAccountId;
    decision.spawnCharacterGuid = runtime.cloneCharacterGuid;
    decision.reason = recoveryPlan.reason;

    switch (recoveryPlan.kind)
    {
        case model::AccountAltRecoveryPlanKind::ReuseClone:
            decision.kind =
                AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
            return decision;
        case model::AccountAltRecoveryPlanKind::SyncCloneToSource:
        {
            AccountAltSyncExecutor executor(_runtimeRepository, _syncRepository);
            if (!executor.Execute(runtime, *cloneSnapshot, recoveryPlan.domainsToSync))
            {
                decision.kind = AccountAltSpawnDecisionKind::ManualReviewRequired;
                decision.reason = "sync execution failed; manual review required";
                return decision;
            }
            decision.kind = AccountAltSpawnDecisionKind::SpawnUsingPersistentClone;
            decision.reason = "sync succeeded; clone is current";
            return decision;
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
