#include "service/AccountAltSyncExecutor.h"

#include <algorithm>

namespace living_world
{
namespace service
{
AccountAltSyncExecutor::AccountAltSyncExecutor(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterProgressSyncRepository& syncRepository,
    integration::CharacterReputationSyncRepository& reputationSyncRepository,
    integration::CharacterQuestSyncRepository& questSyncRepository,
    integration::CharacterAchievementSyncRepository& achievementSyncRepository,
    integration::CharacterSkillSyncRepository& skillSyncRepository,
    integration::CharacterSpellSyncRepository& spellSyncRepository)
    : _runtimeRepository(runtimeRepository),
      _syncRepository(syncRepository),
      _reputationSyncRepository(reputationSyncRepository),
      _questSyncRepository(questSyncRepository),
      _achievementSyncRepository(achievementSyncRepository),
      _skillSyncRepository(skillSyncRepository),
      _spellSyncRepository(spellSyncRepository)
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
    if (hasDomain(model::AccountAltSyncDomain::Honor))
    {
        // Forward the clone's honor/kills. The SQL layer uses GREATEST so
        // spending on the bot never reduces the source's honor.
        target.totalHonorPoints = cloneSnapshot.totalHonorPoints;
        target.totalKills = cloneSnapshot.totalKills;
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

    if (hasDomain(model::AccountAltSyncDomain::Reputation))
    {
        _reputationSyncRepository.SyncReputationFromCloneToSource(
            runtime.sourceCharacterGuid, runtime.cloneCharacterGuid);
    }

    if (hasDomain(model::AccountAltSyncDomain::Quests))
    {
        _questSyncRepository.SyncQuestsFromCloneToSource(
            runtime.sourceCharacterGuid, runtime.cloneCharacterGuid);
    }

    if (hasDomain(model::AccountAltSyncDomain::Achievements))
    {
        _achievementSyncRepository.SyncAchievementsFromCloneToSource(
            runtime.sourceCharacterGuid, runtime.cloneCharacterGuid);
    }

    if (hasDomain(model::AccountAltSyncDomain::Skills))
    {
        _skillSyncRepository.SyncSkillsFromCloneToSource(
            runtime.sourceCharacterGuid, runtime.cloneCharacterGuid);
    }

    if (hasDomain(model::AccountAltSyncDomain::Spells))
    {
        _spellSyncRepository.SyncSpellsFromCloneToSource(
            runtime.sourceCharacterGuid, runtime.cloneCharacterGuid);
    }

    // Sync succeeded
    // SyncingBack flag so the runtime is ready for reuse.
    mutable_runtime.sourceSnapshot = target;
    mutable_runtime.state = model::AccountAltRuntimeState::Active;
    _runtimeRepository.SaveRuntime(mutable_runtime);

    return true;
}
} // namespace service
} // namespace living_world
