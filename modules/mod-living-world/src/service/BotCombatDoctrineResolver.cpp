#include "service/BotCombatDoctrineResolver.h"

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotCombatDefaultProfileRepository.h"
#include "integration/BotCombatProfileRepository.h"
#include "integration/BotCombatProfileSelectionRepository.h"
#include "service/BotCombatSpecRoleResolver.h"

namespace living_world
{
namespace service
{
namespace
{
BotCombatResolvedProfile BuildResolvedProfile(
    model::BotCombatProfileRecord const& profile,
    std::optional<model::BotCombatDefaultProfileRecord> const& defaultProfile)
{
    BotCombatResolvedProfile resolved;
    resolved.settings = profile.settings;
    resolved.interruptEntries = profile.interruptEntries;
    resolved.rotationEntries = profile.rotationEntries;

    if (!profile.HasEntryOverrides() && defaultProfile)
    {
        resolved.interruptEntries = defaultProfile->interruptEntries;
        resolved.rotationEntries = defaultProfile->rotationEntries;
    }

    return resolved;
}

BotCombatResolvedProfile BuildResolvedProfile(
    model::BotCombatDefaultProfileRecord const& defaultProfile)
{
    BotCombatResolvedProfile resolved;
    resolved.settings = defaultProfile.settings;
    resolved.interruptEntries = defaultProfile.interruptEntries;
    resolved.rotationEntries = defaultProfile.rotationEntries;
    return resolved;
}
} // namespace

BotCombatDoctrineResolver::BotCombatDoctrineResolver(
    integration::AccountAltRuntimeRepository const& runtimeRepository,
    integration::BotCombatProfileRepository const& profileRepository,
    integration::BotCombatProfileSelectionRepository const& selectionRepository,
    integration::BotCombatDefaultProfileRepository const& defaultProfileRepository,
    BotCombatSpecRoleResolver const& specRoleResolver)
    : _runtimeRepository(runtimeRepository)
    , _profileRepository(profileRepository)
    , _selectionRepository(selectionRepository)
    , _defaultProfileRepository(defaultProfileRepository)
    , _specRoleResolver(specRoleResolver)
{
}

BotCombatDoctrineResolution BotCombatDoctrineResolver::ResolveForBot(
    std::uint64_t botCharacterGuid,
    std::uint8_t botClassId,
    std::uint32_t ownerAccountId) const
{
    BotCombatDoctrineResolution resolution;
    resolution.botCharacterGuid = botCharacterGuid;
    resolution.sourceCharacterGuid = botCharacterGuid;
    resolution.ownerAccountId = ownerAccountId;

    if (auto runtime = _runtimeRepository.FindByCloneCharacter(
            botCharacterGuid))
    {
        resolution.sourceCharacterGuid = runtime->sourceCharacterGuid;
        ownerAccountId = runtime->sourceAccountId;
        resolution.ownerAccountId = runtime->sourceAccountId;
    }

    if (auto selection = _selectionRepository.FindRuntimeSelection(
            resolution.sourceCharacterGuid))
        resolution.activeProfileSlot = selection->activeProfileSlot;

    auto profile = _profileRepository.FindProfileForCharacterSlot(
        ownerAccountId,
        resolution.sourceCharacterGuid,
        resolution.activeProfileSlot);

    BotCombatSpecRoleResolutionRequest request;
    request.sourceCharacterGuid = resolution.sourceCharacterGuid;
    request.classId = botClassId;
    if (profile)
    {
        request.specOverrideKey = profile->specOverrideKey;
        request.roleOverrideKey = profile->roleOverrideKey;
    }

    BotCombatSpecRoleResolutionResult specRole = _specRoleResolver.Resolve(request);
    if (profile)
    {
        if (!profile->guessedSpecKey.empty())
            specRole.guessedSpecKey = profile->guessedSpecKey;
        if (!profile->guessedRoleKey.empty())
            specRole.guessedRoleKey = profile->guessedRoleKey;
        if (!profile->specOverrideKey || profile->specOverrideKey->empty())
            specRole.effectiveSpecKey = specRole.guessedSpecKey;
        if (!profile->roleOverrideKey || profile->roleOverrideKey->empty())
            specRole.effectiveRoleKey = specRole.guessedRoleKey;
    }

    resolution.guessedSpecKey = specRole.guessedSpecKey;
    resolution.guessedRoleKey = specRole.guessedRoleKey;
    resolution.effectiveSpecKey = specRole.effectiveSpecKey;
    resolution.effectiveRoleKey = specRole.effectiveRoleKey;

    auto defaultProfile = _defaultProfileRepository.FindDefaultProfile(
        specRole.effectiveSpecKey,
        specRole.effectiveRoleKey);
    if (defaultProfile)
        resolution.defaultProfileId = defaultProfile->defaultProfileId;

    if (profile)
    {
        resolution.customProfileId = profile->profileId;
        resolution.profile = BuildResolvedProfile(*profile, defaultProfile);
        resolution.source = profile->HasEntryOverrides()
            ? BotCombatDoctrineSource::CustomProfile
            : (defaultProfile
                ? BotCombatDoctrineSource::CustomProfileWithDefaultFallback
                : BotCombatDoctrineSource::CustomProfile);
        return resolution;
    }

    if (defaultProfile)
    {
        resolution.profile = BuildResolvedProfile(*defaultProfile);
        resolution.source = BotCombatDoctrineSource::DefaultProfile;
        return resolution;
    }

    return resolution;
}
} // namespace service
} // namespace living_world
