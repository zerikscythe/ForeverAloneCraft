#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "model/AccountAltRuntime.h"

#include <vector>

namespace living_world
{
namespace service
{
// Executes a clone-to-source progress sync inside a SyncingBack state guard.
// The runtime is marked SyncingBack before any write and Active only after the
// write succeeds, so a crash mid-sync leaves a recoverable state rather than
// silently corrupt data.
class AccountAltSyncExecutor
{
public:
    AccountAltSyncExecutor(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterProgressSyncRepository& syncRepository);

    bool Execute(
        model::AccountAltRuntimeRecord const& runtime,
        model::CharacterProgressSnapshot const& cloneSnapshot,
        std::vector<model::AccountAltSyncDomain> const& domainsToSync);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterProgressSyncRepository& _syncRepository;
};
} // namespace service
} // namespace living_world
