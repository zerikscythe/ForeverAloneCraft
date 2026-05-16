#include "integration/BotCombatDefaultProfileRepository.h"
#include "integration/BotTalentTemplateRepository.h"
#include "integration/BotVirtualLoadoutRepository.h"
#include "model/BotCombatProfile.h"
#include "model/BotTalentTemplate.h"
#include "model/WorldBotPreparedBuild.h"
#include "service/WorldBotPreparationService.h"

#include "SharedDefines.h"
#include "gtest/gtest.h"

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{
bool HasSpell(std::unordered_set<std::uint32_t> const& spells, std::uint32_t spellId)
{
    return spells.find(spellId) != spells.end();
}

struct DefaultProfileQuery
{
    std::string specKey;
    std::string roleKey;
    std::string classKey;
    std::string contextKey;
    std::string variantKey;
};

class FakeDefaultProfileRepository : public integration::BotCombatDefaultProfileRepository
{
public:
    std::vector<model::BotCombatDefaultProfileRecord> profiles;
    mutable std::vector<DefaultProfileQuery> queries;

    std::vector<model::BotCombatDefaultProfileRecord> ListDefaultProfiles() const override
    {
        return profiles;
    }

    std::optional<model::BotCombatDefaultProfileRecord> FindDefaultProfile(
        std::string const& specKey,
        std::string const& roleKey,
        std::string const& classKey,
        std::string const& contextKey,
        std::string const& variantKey) const override
    {
        queries.push_back({ specKey, roleKey, classKey, contextKey, variantKey });

        for (model::BotCombatDefaultProfileRecord const& profile : profiles)
        {
            if (profile.specKey == specKey
                && profile.roleKey == roleKey
                && profile.classKey == classKey
                && profile.contextKey == contextKey
                && profile.variantKey == variantKey)
            {
                return profile;
            }
        }

        if (!variantKey.empty())
        {
            for (model::BotCombatDefaultProfileRecord const& profile : profiles)
            {
                if (profile.specKey == specKey
                    && profile.classKey == classKey
                    && profile.contextKey == contextKey
                    && profile.variantKey == variantKey)
                {
                    return profile;
                }
            }

            for (model::BotCombatDefaultProfileRecord const& profile : profiles)
            {
                if (profile.specKey == specKey
                    && profile.classKey == classKey
                    && profile.contextKey == contextKey
                    && profile.variantKey.empty())
                {
                    return profile;
                }
            }
        }

        return std::nullopt;
    }
};

class FakeTalentTemplateRepository : public integration::BotTalentTemplateRepository
{
public:
    std::vector<model::BotTalentTemplateRecord> templates;

    std::vector<model::BotTalentTemplateRecord> ListTemplates() const override
    {
        return templates;
    }

    std::optional<model::BotTalentTemplateRecord> FindTemplate(std::uint64_t) const override
    {
        return std::nullopt;
    }

    std::optional<model::BotTalentTemplateRecord> FindTemplateForSpec(
        std::string const& specKey,
        std::uint8_t classId,
        std::string const& variantKey) const override
    {
        for (model::BotTalentTemplateRecord const& profile : templates)
        {
            if (profile.specKey == specKey && profile.classId == classId && profile.variantKey == variantKey)
                return profile;
        }

        if (!variantKey.empty())
        {
            for (model::BotTalentTemplateRecord const& profile : templates)
            {
                if (profile.specKey == specKey && profile.classId == classId && profile.variantKey.empty())
                    return profile;
            }
        }

        return std::nullopt;
    }
};

class FakeVirtualLoadoutRepository : public integration::BotVirtualLoadoutRepository
{
public:
    std::vector<model::WorldBotVirtualLoadout> loadouts;

    std::vector<model::WorldBotVirtualLoadout> ListLoadouts() const override
    {
        return loadouts;
    }

    std::optional<model::WorldBotVirtualLoadout> FindLoadout(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey,
        std::uint8_t gearTier) const override
    {
        model::WorldBotVirtualLoadout const* bestMatch = nullptr;
        int bestLoadoutScore = 99;
        int bestSpecScore = 99;

        for (model::WorldBotVirtualLoadout const& loadout : loadouts)
        {
            if (loadout.classId != classId || loadout.gearTier != gearTier)
                continue;

            int const loadoutScore = loadout.loadoutKey == loadoutKey
                ? 0
                : (loadout.loadoutKey.empty() ? 1 : 2);
            int const specScore = loadout.specKey == specKey
                ? 0
                : (loadout.specKey.empty() ? 1 : 2);

            if (loadoutScore >= 2 || specScore >= 2)
                continue;

            if (!bestMatch
                || loadoutScore < bestLoadoutScore
                || (loadoutScore == bestLoadoutScore && specScore < bestSpecScore))
            {
                bestMatch = &loadout;
                bestLoadoutScore = loadoutScore;
                bestSpecScore = specScore;
            }
        }

        if (bestMatch)
            return *bestMatch;

        return std::nullopt;
    }
};
} // namespace

TEST(WorldBotPreparationServiceTest, ResolvesRoleKeysFromCanonicalSpec)
{
    EXPECT_EQ(WorldBotPreparationService::ResolveRoleKey(CLASS_PALADIN, "Holy"), "HEAL");
    EXPECT_EQ(WorldBotPreparationService::ResolveRoleKey(CLASS_WARRIOR, "Protection"), "TANK");
    EXPECT_EQ(WorldBotPreparationService::ResolveRoleKey(CLASS_MAGE, "Frost"), "DPS");
}

TEST(WorldBotPreparationServiceTest, ComputesAvailableTalentPointsFromLevel)
{
    EXPECT_EQ(WorldBotPreparationService::ComputeAvailableTalentPoints(1), 0u);
    EXPECT_EQ(WorldBotPreparationService::ComputeAvailableTalentPoints(10), 1u);
    EXPECT_EQ(WorldBotPreparationService::ComputeAvailableTalentPoints(80), 71u);
}

TEST(WorldBotPreparationServiceTest, AddsBasicRacialGroundMountSpellsAtEligibleLevels)
{
    integration::BotIdentityRecord humanWarrior;
    humanWarrior.raceId = RACE_HUMAN;
    humanWarrior.classId = CLASS_WARRIOR;
    humanWarrior.level = 20;
    EXPECT_TRUE(HasSpell(WorldBotPreparationService::CollectTravelMobilitySpellIds(humanWarrior), 458u));

    integration::BotIdentityRecord dwarfWarrior;
    dwarfWarrior.raceId = RACE_DWARF;
    dwarfWarrior.classId = CLASS_WARRIOR;
    dwarfWarrior.level = 20;
    EXPECT_TRUE(HasSpell(WorldBotPreparationService::CollectTravelMobilitySpellIds(dwarfWarrior), 6899u));

    integration::BotIdentityRecord trollWarrior;
    trollWarrior.raceId = RACE_TROLL;
    trollWarrior.classId = CLASS_WARRIOR;
    trollWarrior.level = 20;
    EXPECT_TRUE(HasSpell(WorldBotPreparationService::CollectTravelMobilitySpellIds(trollWarrior), 10796u));
}

TEST(WorldBotPreparationServiceTest, DoesNotGrantBasicRacialGroundMountBelowRequiredLevel)
{
    integration::BotIdentityRecord humanWarrior;
    humanWarrior.raceId = RACE_HUMAN;
    humanWarrior.classId = CLASS_WARRIOR;
    humanWarrior.level = 10;

    EXPECT_TRUE(WorldBotPreparationService::CollectTravelMobilitySpellIds(humanWarrior).empty());
}

TEST(WorldBotPreparationServiceTest, PrefersDruidTravelFormInsteadOfRacialMount)
{
    integration::BotIdentityRecord nightElfDruid;
    nightElfDruid.raceId = RACE_NIGHTELF;
    nightElfDruid.classId = CLASS_DRUID;
    nightElfDruid.level = 20;

    auto const spells = WorldBotPreparationService::CollectTravelMobilitySpellIds(nightElfDruid);
    EXPECT_TRUE(HasSpell(spells, 783u));
    EXPECT_FALSE(HasSpell(spells, 10793u));
}

TEST(WorldBotPreparationServiceTest, GrantsClassMountsForPaladinsWarlocksAndDeathKnights)
{
    integration::BotIdentityRecord bloodElfPaladin;
    bloodElfPaladin.raceId = RACE_BLOODELF;
    bloodElfPaladin.classId = CLASS_PALADIN;
    bloodElfPaladin.level = 20;
    auto const paladinSpells = WorldBotPreparationService::CollectTravelMobilitySpellIds(bloodElfPaladin);
    EXPECT_TRUE(HasSpell(paladinSpells, 34769u));
    EXPECT_FALSE(HasSpell(paladinSpells, 34795u));

    integration::BotIdentityRecord orcWarlock;
    orcWarlock.raceId = RACE_ORC;
    orcWarlock.classId = CLASS_WARLOCK;
    orcWarlock.level = 20;
    auto const warlockSpells = WorldBotPreparationService::CollectTravelMobilitySpellIds(orcWarlock);
    EXPECT_TRUE(HasSpell(warlockSpells, 5784u));
    EXPECT_FALSE(HasSpell(warlockSpells, 6654u));

    integration::BotIdentityRecord humanDeathKnight;
    humanDeathKnight.raceId = RACE_HUMAN;
    humanDeathKnight.classId = CLASS_DEATH_KNIGHT;
    humanDeathKnight.level = 60;
    auto const deathKnightSpells = WorldBotPreparationService::CollectTravelMobilitySpellIds(humanDeathKnight);
    EXPECT_TRUE(HasSpell(deathKnightSpells, 48778u));
    EXPECT_FALSE(HasSpell(deathKnightSpells, 458u));
}

TEST(WorldBotPreparationServiceTest, ResolvesPreferredVisibleTravelSpellFromTierAndClass)
{
    integration::BotIdentityRecord druid;
    druid.raceId = RACE_NIGHTELF;
    druid.classId = CLASS_DRUID;
    druid.level = 40;
    EXPECT_EQ(
        WorldBotPreparationService::ResolvePreferredTravelMobilitySpellId(
            druid,
            WorldBotTravelCapabilityTier::GroundBasic),
        783u);

    integration::BotIdentityRecord paladin;
    paladin.raceId = RACE_HUMAN;
    paladin.classId = CLASS_PALADIN;
    paladin.level = 60;
    EXPECT_EQ(
        WorldBotPreparationService::ResolvePreferredTravelMobilitySpellId(
            paladin,
            WorldBotTravelCapabilityTier::GroundFast),
        23214u);

    integration::BotIdentityRecord warlock;
    warlock.raceId = RACE_ORC;
    warlock.classId = CLASS_WARLOCK;
    warlock.level = 40;
    EXPECT_EQ(
        WorldBotPreparationService::ResolvePreferredTravelMobilitySpellId(
            warlock,
            WorldBotTravelCapabilityTier::GroundFast),
        23161u);

    integration::BotIdentityRecord warrior;
    warrior.raceId = RACE_TROLL;
    warrior.classId = CLASS_WARRIOR;
    warrior.level = 40;
    EXPECT_EQ(
        WorldBotPreparationService::ResolvePreferredTravelMobilitySpellId(
            warrior,
            WorldBotTravelCapabilityTier::GroundFast),
        10796u);

    EXPECT_TRUE(WorldBotPreparationService::IsTravelFormMobilitySpell(783u));
    EXPECT_FALSE(WorldBotPreparationService::IsTravelFormMobilitySpell(10796u));
}

TEST(WorldBotPreparationServiceTest, FailsCleanlyWhenNoDefaultCombatProfileExists)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    integration::BotIdentityRecord identity;
    identity.id = 77;
    identity.classId = CLASS_PALADIN;
    identity.specKey = "paladin_ret";
    identity.level = 40;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity);
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingDefaultCombatProfile);
    EXPECT_EQ(prepared.canonicalSpecKey, "Retribution");
    EXPECT_EQ(prepared.resolvedRoleKey, "DPS");
}

TEST(WorldBotPreparationServiceTest, FailsCleanlyWhenNoTalentTemplateExists)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 15;
    defaultProfile.displayName = "Holy Priest Default";
    defaultProfile.specKey = "Holy";
    defaultProfile.roleKey = "HEAL";
    defaultProfile.classKey = "Priest";
    defaultProfile.contextKey = "PvE";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    integration::BotIdentityRecord identity;
    identity.id = 88;
    identity.classId = CLASS_PRIEST;
    identity.specKey = "priest_holy";
    identity.level = 60;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity);
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingTalentTemplate);
    EXPECT_EQ(prepared.defaultCombatProfileId, 15u);
    EXPECT_EQ(prepared.canonicalSpecKey, "Holy");
    EXPECT_EQ(prepared.resolvedRoleKey, "HEAL");
}

TEST(WorldBotPreparationServiceTest, FallsBackToPveContextBeforeMissingPlayerInfoFailure)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 42;
    defaultProfile.displayName = "Arcane Mage PvE";
    defaultProfile.specKey = "Arcane";
    defaultProfile.roleKey = "DPS";
    defaultProfile.classKey = "Mage";
    defaultProfile.contextKey = "PvE";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    model::BotTalentTemplateRecord talentTemplate;
    talentTemplate.templateId = 99;
    talentTemplate.specKey = "Arcane";
    talentTemplate.classId = CLASS_MAGE;
    talentTemplate.displayName = "Arcane Template";
    talentTemplateRepository.templates.push_back(talentTemplate);

    integration::BotIdentityRecord identity;
    identity.id = 101;
    identity.raceId = 0;
    identity.classId = CLASS_MAGE;
    identity.specKey = "mage_arcane";
    identity.level = 80;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity, "Dungeon");
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingPlayerInfo);
    EXPECT_EQ(prepared.failureReason, "missing_player_info");
    EXPECT_EQ(prepared.contextKey, "PvE");
    EXPECT_EQ(prepared.defaultCombatProfileId, 42u);
    EXPECT_EQ(prepared.defaultCombatProfileName, "Arcane Mage PvE");
    EXPECT_EQ(prepared.talentTemplateId, 99u);
    EXPECT_EQ(prepared.talentTemplateName, "Arcane Template");
    ASSERT_EQ(defaultProfileRepository.queries.size(), 2u);
    EXPECT_EQ(defaultProfileRepository.queries[0].contextKey, "Dungeon");
    EXPECT_EQ(defaultProfileRepository.queries[1].contextKey, "PvE");
}

TEST(WorldBotPreparationServiceTest, MissingPlayerInfoRetainsRequestedContextWhenProfileExists)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 57;
    defaultProfile.displayName = "Restoration Shaman Dungeon";
    defaultProfile.specKey = "Restoration";
    defaultProfile.roleKey = "HEAL";
    defaultProfile.classKey = "Shaman";
    defaultProfile.contextKey = "Dungeon";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    model::BotTalentTemplateRecord talentTemplate;
    talentTemplate.templateId = 7;
    talentTemplate.specKey = "Restoration";
    talentTemplate.classId = CLASS_SHAMAN;
    talentTemplate.displayName = "Restoration Template";
    talentTemplateRepository.templates.push_back(talentTemplate);

    integration::BotIdentityRecord identity;
    identity.id = 202;
    identity.raceId = 0;
    identity.classId = CLASS_SHAMAN;
    identity.specKey = "shaman_resto";
    identity.level = 70;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity, "Dungeon");
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingPlayerInfo);
    EXPECT_EQ(prepared.failureReason, "missing_player_info");
    EXPECT_EQ(prepared.contextKey, "Dungeon");
    EXPECT_EQ(prepared.defaultCombatProfileId, 57u);
    EXPECT_EQ(prepared.talentTemplateId, 7u);
    ASSERT_EQ(defaultProfileRepository.queries.size(), 1u);
    EXPECT_EQ(defaultProfileRepository.queries[0].contextKey, "Dungeon");
}

TEST(WorldBotPreparationServiceTest, PrefersVariantSpecificProfileAndTemplateWhenLoadoutKeyProvided)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 88;
    defaultProfile.displayName = "Feral Cat PvE";
    defaultProfile.specKey = "Feral";
    defaultProfile.roleKey = "DPS";
    defaultProfile.classKey = "Druid";
    defaultProfile.contextKey = "PvE";
    defaultProfile.variantKey = "Druid_Feral_PVE_01";
    defaultProfile.description = "Cat-focused feral PvE loadout";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    model::BotTalentTemplateRecord talentTemplate;
    talentTemplate.templateId = 55;
    talentTemplate.specKey = "Feral";
    talentTemplate.classId = CLASS_DRUID;
    talentTemplate.displayName = "Feral Cat PvE Template";
    talentTemplate.variantKey = "Druid_Feral_PVE_01";
    talentTemplate.description = "Cat DPS talent layout";
    talentTemplateRepository.templates.push_back(talentTemplate);

    integration::BotIdentityRecord identity;
    identity.id = 303;
    identity.raceId = 0;
    identity.classId = CLASS_DRUID;
    identity.specKey = "druid_feral";
    identity.loadoutKey = "Druid_Feral_PVE_01";
    identity.level = 80;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity);
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingPlayerInfo);
    EXPECT_EQ(prepared.requestedLoadoutKey, "Druid_Feral_PVE_01");
    EXPECT_EQ(prepared.resolvedRoleKey, "DPS");
    EXPECT_EQ(prepared.defaultCombatProfileId, 88u);
    EXPECT_EQ(prepared.defaultCombatProfileVariantKey, "Druid_Feral_PVE_01");
    EXPECT_EQ(prepared.defaultCombatProfileDescription, "Cat-focused feral PvE loadout");
    EXPECT_EQ(prepared.talentTemplateId, 55u);
    ASSERT_EQ(defaultProfileRepository.queries.size(), 1u);
    EXPECT_EQ(defaultProfileRepository.queries[0].roleKey, "TANK");
    EXPECT_EQ(defaultProfileRepository.queries[0].variantKey, "Druid_Feral_PVE_01");
    EXPECT_EQ(prepared.talentTemplateVariantKey, "Druid_Feral_PVE_01");
    EXPECT_EQ(prepared.talentTemplateDescription, "Cat DPS talent layout");
}

TEST(WorldBotPreparationServiceTest, ResolvesSpecSpecificVirtualLoadoutBeforeClassFallback)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 26;
    defaultProfile.displayName = "Arcane Mage PvE";
    defaultProfile.specKey = "Arcane";
    defaultProfile.roleKey = "DPS";
    defaultProfile.classKey = "Mage";
    defaultProfile.contextKey = "PvE";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    model::BotTalentTemplateRecord talentTemplate;
    talentTemplate.templateId = 20;
    talentTemplate.specKey = "Arcane";
    talentTemplate.classId = CLASS_MAGE;
    talentTemplate.displayName = "Arcane Mage DPS";
    talentTemplateRepository.templates.push_back(talentTemplate);

    model::WorldBotVirtualLoadout classFallback;
    classFallback.loadoutId = 1;
    classFallback.classId = CLASS_MAGE;
    classFallback.gearTier = 1;
    classFallback.displayName = "Mage Tier 1 Class Fallback";
    classFallback.bonusIntellect = 90;
    virtualLoadoutRepository.loadouts.push_back(classFallback);

    model::WorldBotVirtualLoadout specSpecific;
    specSpecific.loadoutId = 2;
    specSpecific.classId = CLASS_MAGE;
    specSpecific.specKey = "Arcane";
    specSpecific.gearTier = 1;
    specSpecific.displayName = "Mage Arcane Tier 1";
    specSpecific.bonusIntellect = 120;
    specSpecific.bonusMana = 800;
    virtualLoadoutRepository.loadouts.push_back(specSpecific);

    integration::BotIdentityRecord identity;
    identity.id = 401;
    identity.raceId = RACE_HUMAN;
    identity.classId = CLASS_MAGE;
    identity.specKey = "mage_arcane";
    identity.gearTier = 1;
    identity.level = 80;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity);
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingPlayerInfo);
    ASSERT_TRUE(prepared.virtualLoadout.has_value());
    EXPECT_EQ(prepared.virtualLoadout->loadoutId, 2u);
    EXPECT_EQ(prepared.virtualLoadout->displayName, "Mage Arcane Tier 1");
    EXPECT_EQ(prepared.virtualLoadout->bonusMana, 800);
}

TEST(WorldBotPreparationServiceTest, PrefersExactVirtualLoadoutKeyBeforeSpecOrClassFallback)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 26;
    defaultProfile.displayName = "Arcane Mage PvE";
    defaultProfile.specKey = "Arcane";
    defaultProfile.roleKey = "DPS";
    defaultProfile.classKey = "Mage";
    defaultProfile.contextKey = "PvE";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    model::BotTalentTemplateRecord talentTemplate;
    talentTemplate.templateId = 20;
    talentTemplate.specKey = "Arcane";
    talentTemplate.classId = CLASS_MAGE;
    talentTemplate.displayName = "Arcane Mage DPS";
    talentTemplateRepository.templates.push_back(talentTemplate);

    model::WorldBotVirtualLoadout classFallback;
    classFallback.loadoutId = 1;
    classFallback.classId = CLASS_MAGE;
    classFallback.gearTier = 1;
    classFallback.displayName = "Mage Tier 1 Class Fallback";
    classFallback.bonusIntellect = 90;
    virtualLoadoutRepository.loadouts.push_back(classFallback);

    model::WorldBotVirtualLoadout specSpecific;
    specSpecific.loadoutId = 2;
    specSpecific.classId = CLASS_MAGE;
    specSpecific.specKey = "Arcane";
    specSpecific.gearTier = 1;
    specSpecific.displayName = "Mage Arcane Tier 1";
    specSpecific.bonusIntellect = 120;
    specSpecific.bonusMana = 800;
    virtualLoadoutRepository.loadouts.push_back(specSpecific);

    model::WorldBotVirtualLoadout exactVariant;
    exactVariant.loadoutId = 3;
    exactVariant.classId = CLASS_MAGE;
    exactVariant.specKey = "Arcane";
    exactVariant.loadoutKey = "Mage_Arcane_PVE_Starter";
    exactVariant.gearTier = 1;
    exactVariant.displayName = "Mage Arcane PvE Starter";
    exactVariant.bonusIntellect = 135;
    exactVariant.bonusMana = 950;
    virtualLoadoutRepository.loadouts.push_back(exactVariant);

    integration::BotIdentityRecord identity;
    identity.id = 402;
    identity.raceId = RACE_HUMAN;
    identity.classId = CLASS_MAGE;
    identity.specKey = "mage_arcane";
    identity.loadoutKey = "Mage_Arcane_PVE_Starter";
    identity.gearTier = 1;
    identity.level = 80;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity);
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingPlayerInfo);
    ASSERT_TRUE(prepared.virtualLoadout.has_value());
    EXPECT_EQ(prepared.virtualLoadout->loadoutId, 3u);
    EXPECT_EQ(prepared.virtualLoadout->displayName, "Mage Arcane PvE Starter");
    EXPECT_EQ(prepared.virtualLoadout->bonusMana, 950);
}

TEST(WorldBotPreparationServiceTest, ResolvesVirtualLoadoutByClassSpecLoadoutAndGearTier)
{
    FakeDefaultProfileRepository defaultProfileRepository;
    FakeTalentTemplateRepository talentTemplateRepository;
    FakeVirtualLoadoutRepository virtualLoadoutRepository;
    WorldBotPreparationService service(defaultProfileRepository, talentTemplateRepository, virtualLoadoutRepository);

    model::BotCombatDefaultProfileRecord defaultProfile;
    defaultProfile.defaultProfileId = 26;
    defaultProfile.displayName = "Arcane Mage PvE";
    defaultProfile.specKey = "Arcane";
    defaultProfile.roleKey = "DPS";
    defaultProfile.classKey = "Mage";
    defaultProfile.contextKey = "PvE";
    defaultProfileRepository.profiles.push_back(defaultProfile);

    model::BotTalentTemplateRecord talentTemplate;
    talentTemplate.templateId = 20;
    talentTemplate.specKey = "Arcane";
    talentTemplate.classId = CLASS_MAGE;
    talentTemplate.displayName = "Arcane Mage DPS";
    talentTemplateRepository.templates.push_back(talentTemplate);

    model::WorldBotVirtualLoadout loadout;
    loadout.loadoutId = 7;
    loadout.classId = CLASS_MAGE;
    loadout.specKey = "Arcane";
    loadout.loadoutKey = "Mage_Arcane_PVE_Starter";
    loadout.gearTier = 2;
    loadout.displayName = "Mage Arcane PvE Starter";
    loadout.bonusIntellect = 42;
    loadout.bonusStamina = 30;
    loadout.bonusMana = 500;
    loadout.bonusArmor = 120;
    virtualLoadoutRepository.loadouts.push_back(loadout);

    integration::BotIdentityRecord identity;
    identity.id = 404;
    identity.raceId = RACE_HUMAN;
    identity.classId = CLASS_MAGE;
    identity.specKey = "mage_arcane";
    identity.loadoutKey = "Mage_Arcane_PVE_Starter";
    identity.gearTier = 2;
    identity.level = 80;

    model::WorldBotPreparedBuild const prepared = service.Prepare(identity);
    EXPECT_FALSE(prepared.IsReady());
    EXPECT_EQ(prepared.status, model::WorldBotPreparationStatus::MissingPlayerInfo);
    ASSERT_TRUE(prepared.virtualLoadout.has_value());
    EXPECT_EQ(prepared.virtualLoadout->loadoutId, 7u);
    EXPECT_EQ(prepared.virtualLoadout->displayName, "Mage Arcane PvE Starter");
    EXPECT_EQ(prepared.virtualLoadout->bonusMana, 500);
    EXPECT_EQ(prepared.virtualLoadout->bonusArmor, 120);
}

} // namespace service
} // namespace living_world
