#include "service/AccountAltSyncExecutor.h"

#include <algorithm>

namespace living_world
{
namespace service
{
AccountAltSyncExecutor::AccountAltSyncExecutor(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterProgressSyncRepository& syncRepository)
    : _runtimeRepository(runtimeRepository),
      _syncRepository(syncRepository)
{
}

bool AccountAltSyncExecutor::Execute(
    model::AccountAltRuntimeRecord const& runtime,
    model::CharacterProgressSnapshot const& cloneSnapshot,
    std::vector<model::AccountAltSyncDomain> const& domainsToSync)
{
    auto hasDomain = [&](model::AccountAltSyncDomain domain)
    {
        return std::find(domainsToSync.begin(), domainsToSync.end(), domain) !=
               domainsToSync.end();
    };

    // Build the target snapshot from the clone, restricted to approved domains.
    // Fields not in domainsToSync keep the source's existing values.
    model::CharacterProgressSnapshot target = runtime.sourceSnapshot;
    if (hasDomain(model::AccountAltSyncDomain::Experience))
    {
        target.level = cloneSnapshot.level;
        target.experience = cloneSnapshot.experience;
    }
    if (hasDomain(model::AccountAltSyncDomain::Money))
    {
        target.money = cloneSnapshot.money;
    }

    // Mark SyncingBack before any mutation. A crash here leaves a recoverable
    // record that the startup pass can retry.
    model::AccountAltRuntimeRecord mutable_runtime = runtime;
    mutable_runtime.state = model::AccountAltRuntimeState::SyncingBack;
    _runtimeRepository.SaveRuntime(mutable_runtime);

    if (!_syncRepository.SyncProgressToCharacter(
            runtime.sourceCharacterGuid, target))
    {
        return false;
    }

    // Sync succeeded — update the persisted source snapshot and clear the
    // SyncingBack flag so the runtime is ready for reuse.
    mutable_runtime.sourceSnapshot = target;
    mutable_runtime.state = model::AccountAltRuntimeState::Active;
    _runtimeRepository.SaveRuntime(mutable_runtime);

    return true;
}
} // namespace service
} // namespace living_world
