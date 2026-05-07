#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotCombatDefaultProfileRepository.h"
#include "integration/BotCombatProfileRepository.h"
#include "integration/BotCombatProfileSelectionRepository.h"
#include "model/AccountAltRuntime.h"
#include "service/BotCombatDoctrineResolver.h"
#include "service/BotCombatSpecRoleResolver.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
class FakeRuntimeRepository : public integration::AccountAltRuntimeRepository
{
public:
    std::optional<model::AccountAltRuntimeRecord> runtime;

    std::optional<model::AccountAltRuntimeRecord>
    FindBySourceCharacter(std::uint32_t, std::uint64_t) const override
    {
        return std::nullopt;
    }

    std::optional<model::AccountAltRuntimeRecord>
    FindByCloneCharacter(std::uint64_t) const override
    {
        return runtime;
    }

    std::vector<model::AccountAltRuntimeRecord>
    ListRecoverableForAccount(std::uint32_t) const override
    {
        return {};
    }

    void SaveRuntime(model::AccountAltRuntimeRecord const&) override { }
    void DeleteRuntime(std::uint64_t) override { }
};

class FakeProfileRepository : public integration::BotCombatProfileRepository
{
public:
    std::optional<model::BotCombatProfileRecord> profile;
    mutable std::uint32_t lastOwnerAccountId = 0;
    mutable std::uint64_t lastSourceCharacterGuid = 0;
    mutable std::uint8_t lastSlot = 0;

    std::vector<model::BotCombatProfileRecord> ListProfilesForCharacter(
        std::uint32_t,
        std::uint64_t) const override
    {
        return profile ? std::vector<model::BotCombatProfileRecord>{ *profile } : std::vector<model::BotCombatProfileRecord>{};
    }

    std::optional<model::BotCombatProfileRecord> FindProfileForCharacterSlot(
        std::uint32_t ownerAccountId,
        std::uint64_t sourceCharacterGuid,
        std::uint8_t slot) const override
    {
        lastOwnerAccountId = ownerAccountId;
        lastSourceCharacterGuid = sourceCharacterGuid;
        lastSlot = slot;
        return profile;
    }

    void SaveProfile(model::BotCombatProfileRecord const&) override { }
    void DeleteProfile(std::uint64_t) override { }
};

class FakeSelectionRepository : public integration::BotCombatProfileSelectionRepository
{
public:
    std::optional<model::BotCombatRuntimeSelection> selection;
    mutable std::uint64_t lastSourceCharacterGuid = 0;

    std::optional<model::BotCombatRuntimeSelection> FindRuntimeSelection(
        std::uint64_t sourceCharacterGuid) const override
    {
        lastSourceCharacterGuid = sourceCharacterGuid;
        return selection;
    }

    void SaveRuntimeSelection(model::BotCombatRuntimeSelection const&) override { }
};

class FakeDefaultProfileRepository : public integration::BotCombatDefaultProfileRepository
{
public:
    std::optional<model::BotCombatDefaultProfileRecord> profile;
    mutable std::string lastSpecKey;
    mutable std::string lastRoleKey;
    mutable std::string lastClassKey;
    mutable std::string lastContextKey;
    mutable std::vector<std::string> requestedClassKeys;
    mutable std::vector<std::string> requestedContextKeys;

    std::vector<model::BotCombatDefaultProfileRecord> ListDefaultProfiles() const override
    {
        return profile ? std::vector<model::BotCombatDefaultProfileRecord>{ *profile } : std::vector<model::BotCombatDefaultProfileRecord>{};
    }

    std::optional<model::BotCombatDefaultProfileRecord> FindDefaultProfile(
        std::string const& specKey,
        std::string const& roleKey,
        std::string const& classKey,
        std::string const& contextKey) const override
    {
        lastSpecKey = specKey;
        lastRoleKey = roleKey;
        lastClassKey = classKey;
        lastContextKey = contextKey;
        requestedClassKeys.push_back(classKey);
        requestedContextKeys.push_back(contextKey);
        return profile;
    }
};

class FakeSpecRoleResolver : public BotCombatSpecRoleResolver
{
public:
    BotCombatSpecRoleResolutionResult result;

    BotCombatSpecRoleResolutionResult Resolve(
        BotCombatSpecRoleResolutionRequest const&) const override
    {
        return result;
    }
};

TEST(BotCombatDoctrineResolverTest, BlankProfileFallsBackToDefaultEntries)
{
    FakeRuntimeRepository runtimeRepository;
    FakeProfileRepository profileRepository;
    FakeSelectionRepository selectionRepository;
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeSpecRoleResolver specRoleResolver;
    BotContextService contextService;

    model::BotCombatProfileRecord customProfile;
    customProfile.profileId = 1;
    customProfile.slot = 2;
    customProfile.settings.resourceLowWater = 33;
    profileRepository.profile = customProfile;

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 9;
    defaultProfile.settings.resourceLowWater = 44;
    model::BotCombatEntryDefinition entry;
    entry.entryId = 100;
    entry.label = "Default Frostbolt";
    defaultProfile.rotationEntries.push_back(entry);
    defaultProfileRepository.profile = defaultProfile;

    specRoleResolver.result.guessedSpecKey = "Frost";
    specRoleResolver.result.guessedRoleKey = "DPS";
    specRoleResolver.result.effectiveSpecKey = "Frost";
    specRoleResolver.result.effectiveRoleKey = "DPS";

    BotCombatDoctrineResolver resolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        contextService);

    auto const resolved = resolver.ResolveForBot(1234, 8, 77);
    ASSERT_EQ(resolved.profile.settings.resourceLowWater, 33);
    ASSERT_EQ(resolved.profile.rotationEntries.size(), 1u);
    EXPECT_EQ(resolved.profile.rotationEntries[0].entryId, 100u);
    EXPECT_EQ(resolved.source, BotCombatDoctrineSource::CustomProfileWithDefaultFallback);
    EXPECT_EQ(resolved.ownerAccountId, 77u);
    EXPECT_EQ(resolved.sourceCharacterGuid, 1234u);
    EXPECT_EQ(profileRepository.lastOwnerAccountId, 77u);
    EXPECT_EQ(profileRepository.lastSourceCharacterGuid, 1234u);
    EXPECT_EQ(profileRepository.lastSlot, 0u);
}

TEST(BotCombatDoctrineResolverTest, NoProfileUsesDefaultProfile)
{
    FakeRuntimeRepository runtimeRepository;
    FakeProfileRepository profileRepository;
    FakeSelectionRepository selectionRepository;
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeSpecRoleResolver specRoleResolver;
    BotContextService contextService;

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 4;
    defaultProfile.settings.resourceLowWater = 55;
    model::BotCombatEntryDefinition entry;
    entry.entryId = 200;
    entry.label = "Default Action";
    defaultProfile.rotationEntries.push_back(entry);
    defaultProfileRepository.profile = defaultProfile;

    specRoleResolver.result.guessedSpecKey = "Balance";
    specRoleResolver.result.guessedRoleKey = "DPS";
    specRoleResolver.result.effectiveSpecKey = "Balance";
    specRoleResolver.result.effectiveRoleKey = "DPS";

    BotCombatDoctrineResolver resolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        contextService);

    auto const resolved = resolver.ResolveForBot(9999, 11, 55);
    ASSERT_EQ(resolved.profile.settings.resourceLowWater, 55);
    ASSERT_EQ(resolved.profile.rotationEntries.size(), 1u);
    EXPECT_EQ(resolved.profile.rotationEntries[0].entryId, 200u);
    EXPECT_EQ(resolved.source, BotCombatDoctrineSource::DefaultProfile);
    EXPECT_EQ(defaultProfileRepository.lastSpecKey, "Balance");
    EXPECT_EQ(defaultProfileRepository.lastRoleKey, "DPS");
    EXPECT_EQ(defaultProfileRepository.lastClassKey, "Druid");
}

TEST(BotCombatDoctrineResolverTest, DefaultLookupPassesDoctrineClassKey)
{
    FakeRuntimeRepository runtimeRepository;
    FakeProfileRepository profileRepository;
    FakeSelectionRepository selectionRepository;
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeSpecRoleResolver specRoleResolver;
    BotContextService contextService;

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 21;
    defaultProfileRepository.profile = defaultProfile;

    specRoleResolver.result.guessedSpecKey = "Holy";
    specRoleResolver.result.guessedRoleKey = "HEAL";
    specRoleResolver.result.effectiveSpecKey = "Holy";
    specRoleResolver.result.effectiveRoleKey = "HEAL";

    BotCombatDoctrineResolver resolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        contextService);

    auto const resolved = resolver.ResolveForBot(6000, 2, 88);
    EXPECT_EQ(resolved.defaultProfileId.value_or(0), 21u);
    EXPECT_EQ(defaultProfileRepository.lastClassKey, "Paladin");
    ASSERT_EQ(defaultProfileRepository.requestedClassKeys.size(), 1u);
    EXPECT_EQ(defaultProfileRepository.requestedClassKeys[0], "Paladin");
}

TEST(BotCombatDoctrineResolverTest, WorldBotLookupPassesDoctrineClassKey)
{
    FakeRuntimeRepository runtimeRepository;
    FakeProfileRepository profileRepository;
    FakeSelectionRepository selectionRepository;
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeSpecRoleResolver specRoleResolver;
    BotContextService contextService;

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 31;
    defaultProfileRepository.profile = defaultProfile;

    BotCombatDoctrineResolver resolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        contextService);

    auto const resolved = resolver.ResolveForWorldBot(7000, 7, "Restoration", "HEAL", "Dungeon");
    EXPECT_EQ(resolved.defaultProfileId.value_or(0), 31u);
    EXPECT_EQ(defaultProfileRepository.lastClassKey, "Shaman");
    ASSERT_EQ(defaultProfileRepository.requestedClassKeys.size(), 1u);
    EXPECT_EQ(defaultProfileRepository.requestedClassKeys[0], "Shaman");
    EXPECT_EQ(defaultProfileRepository.requestedContextKeys[0], "Dungeon");
}

TEST(BotCombatDoctrineResolverTest, RuntimeCloneUsesSourceIdentityForLookup)
{
    FakeRuntimeRepository runtimeRepository;
    FakeProfileRepository profileRepository;
    FakeSelectionRepository selectionRepository;
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeSpecRoleResolver specRoleResolver;
    BotContextService contextService;

    model::AccountAltRuntimeRecord runtime;
    runtime.cloneCharacterGuid = 9000;
    runtime.sourceCharacterGuid = 123;
    runtime.sourceAccountId = 42;
    runtimeRepository.runtime = runtime;

    model::BotCombatRuntimeSelection selection;
    selection.sourceCharacterGuid = 123;
    selection.activeProfileSlot = 6;
    selectionRepository.selection = selection;

    model::BotCombatProfileRecord customProfile;
    customProfile.profileId = 7;
    customProfile.slot = 6;
    customProfile.settings.resourceLowWater = 61;
    model::BotCombatEntryDefinition entry;
    entry.entryId = 300;
    entry.label = "Custom Action";
    customProfile.rotationEntries.push_back(entry);
    profileRepository.profile = customProfile;

    specRoleResolver.result.guessedSpecKey = "Elemental";
    specRoleResolver.result.guessedRoleKey = "DPS";
    specRoleResolver.result.effectiveSpecKey = "Elemental";
    specRoleResolver.result.effectiveRoleKey = "DPS";

    BotCombatDoctrineResolver resolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        contextService);

    auto const resolved = resolver.ResolveForBot(9000, 7, 9);
    ASSERT_EQ(resolved.profile.settings.resourceLowWater, 61);
    ASSERT_EQ(resolved.profile.rotationEntries.size(), 1u);
    EXPECT_EQ(resolved.profile.rotationEntries[0].entryId, 300u);
    EXPECT_EQ(resolved.sourceCharacterGuid, 123u);
    EXPECT_EQ(resolved.ownerAccountId, 42u);
    EXPECT_EQ(resolved.activeProfileSlot, 6u);
    EXPECT_EQ(selectionRepository.lastSourceCharacterGuid, 123u);
    EXPECT_EQ(profileRepository.lastOwnerAccountId, 42u);
    EXPECT_EQ(profileRepository.lastSourceCharacterGuid, 123u);
    EXPECT_EQ(profileRepository.lastSlot, 6u);
}

TEST(BotCombatDoctrineResolverTest, CustomProfileWithEntriesWinsWithoutDefaultFallback)
{
    FakeRuntimeRepository runtimeRepository;
    FakeProfileRepository profileRepository;
    FakeSelectionRepository selectionRepository;
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeSpecRoleResolver specRoleResolver;
    BotContextService contextService;

    model::BotCombatProfileRecord customProfile;
    customProfile.profileId = 88;
    customProfile.settings.resourceLowWater = 70;
    model::BotCombatEntryDefinition entry;
    entry.entryId = 444;
    entry.label = "Custom Rotation";
    customProfile.rotationEntries.push_back(entry);
    profileRepository.profile = customProfile;

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 999;
    model::BotCombatEntryDefinition defaultEntry;
    defaultEntry.entryId = 555;
    defaultEntry.label = "Default Rotation";
    defaultProfile.rotationEntries.push_back(defaultEntry);
    defaultProfileRepository.profile = defaultProfile;

    specRoleResolver.result.guessedSpecKey = "Affliction";
    specRoleResolver.result.guessedRoleKey = "DPS";
    specRoleResolver.result.effectiveSpecKey = "Affliction";
    specRoleResolver.result.effectiveRoleKey = "DPS";

    BotCombatDoctrineResolver resolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        contextService);

    auto const resolved = resolver.ResolveForBot(77, 9, 14);
    EXPECT_EQ(resolved.source, BotCombatDoctrineSource::CustomProfile);
    ASSERT_EQ(resolved.profile.rotationEntries.size(), 1u);
    EXPECT_EQ(resolved.profile.rotationEntries[0].entryId, 444u);
    EXPECT_EQ(resolved.customProfileId.value_or(0), 88u);
    EXPECT_EQ(resolved.defaultProfileId.value_or(0), 999u);
}
} // namespace
} // namespace service
} // namespace living_world
