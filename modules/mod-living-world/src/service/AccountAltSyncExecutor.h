#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterAchievementSyncRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "integration/CharacterQuestSyncRepository.h"
#include "integration/CharacterReputationSyncRepository.h"
#include "integration/CharacterSkillSyncRepository.h"
#include "integration/CharacterSpellSyncRepository.h"
#include "model/AccountAltRuntime.h"

#include <vector>

namespace living_world
{
namespace service
{
class AccountAltSyncExecutor
{
public:
    AccountAltSyncExecutor(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterProgressSyncRepository& syncRepository,
        integration::CharacterReputationSyncRepository& reputationSyncRepository,
        integration::CharacterQuestSyncRepository& questSyncRepository,
        integration::CharacterAchievementSyncRepository& achievementSyncRepository,
        integration::CharacterSkillSyncRepository& skillSyncRepository,
        integration::CharacterSpellSyncRepository& spellSyncRepository);

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
    integration::CharacterSkillSyncRepository& _skillSyncRepository;
    integration::CharacterSpellSyncRepository& _spellSyncRepository;
};
} // namespace service
} // namespace living_world
