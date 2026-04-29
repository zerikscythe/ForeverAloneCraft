#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterAchievementSyncRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "integration/CharacterQuestSyncRepository.h"
#include "integration/CharacterReputationSyncRepository.h"
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
        integration::CharacterProgressSyncRepository& syncRepository,
        integration::CharacterReputationSyncRepository& reputationSyncRepository,
        integration::CharacterQuestSyncRepository& questSyncRepository,
        integration::CharacterAchievementSyncRepository& achievementSyncRepository);

    bool Execute(
        model::AccountAltRuntimeRecord const& runtime,
        model::CharacterProgressSnapshot const& cloneSnapshot,
        std::vector<model::AccountAltSyncDomain> const& domainsToSync);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterProgressSyncRepository& _syncRepository;
    integration::CharacterReputationSyncRepository& _reputationSyncRepository;
    integration::CharacterQuestSyncRepository& _questSyncRepository;
    integration::CharacterAchievementSyncRepository& _achievementSyncRepository;
};
} // namespace service
} // namespace living_world
