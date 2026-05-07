#include "service/BotTalentApplicator.h"
#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotTalentPreferenceRepository.h"
#include "integration/BotTalentTemplateRepository.h"
#include "model/AccountAltRuntime.h"
#include "model/BotTalentTemplate.h"
#include "gtest/gtest.h"

// Tests for BotTalentApplicator that exercise the repository-interaction
// logic without requiring real Player or DBC objects.
// Methods that touch Player* (ApplyTemplate, ReapplyTemplate, and the
// auto-detect branch of ApplyPreferredTemplate) require AzerothCore and are
// validated through in-game testing instead.

namespace living_world
{
namespace service
{
namespace
{

// ── fake repositories ─────────────────────────────────────────────────────────

class FakeTemplateRepository final : public integration::BotTalentTemplateRepository
{
public:
    std::optional<model::BotTalentTemplateRecord> storedTemplate;
    mutable std::uint64_t lastFindTemplateId = 0;
    mutable std::string   lastFindSpecKey;
    mutable std::uint8_t  lastFindClassId = 0;

    std::vector<model::BotTalentTemplateRecord> ListTemplates() const override
    {
        return {};
    }

    std::optional<model::BotTalentTemplateRecord> FindTemplate(
        std::uint64_t templateId) const override
    {
        lastFindTemplateId = templateId;
        return storedTemplate;
    }

    std::optional<model::BotTalentTemplateRecord> FindTemplateForSpec(
        std::string const& specKey,
        std::uint8_t classId) const override
    {
        lastFindSpecKey = specKey;
        lastFindClassId = classId;
        return storedTemplate;
    }
};

class FakePreferenceRepository final : public integration::BotTalentPreferenceRepository
{
public:
    std::optional<model::BotTalentPreference> storedPref;
    mutable std::uint64_t lastGetGuid = 0;
    model::BotTalentPreference savedPref{};
    bool saveCalled = false;

    std::optional<model::BotTalentPreference> GetPreference(
        std::uint64_t sourceCharGuid) const override
    {
        lastGetGuid = sourceCharGuid;
        return storedPref;
    }

    void SavePreference(model::BotTalentPreference const& pref) override
    {
        savedPref   = pref;
        saveCalled  = true;
    }

    void ClearPreference(std::uint64_t) override {}
};

class FakeAltRuntimeRepository final : public integration::AccountAltRuntimeRepository
{
public:
    bool isAlt = false;

    std::optional<model::AccountAltRuntimeRecord>
    FindBySourceCharacter(std::uint32_t, std::uint64_t) const override
    {
        return std::nullopt;
    }

    std::optional<model::AccountAltRuntimeRecord>
    FindByCloneCharacter(std::uint64_t) const override
    {
        if (!isAlt)
            return std::nullopt;
        model::AccountAltRuntimeRecord rec;
        return rec;
    }

    std::vector<model::AccountAltRuntimeRecord>
    ListRecoverableForAccount(std::uint32_t) const override
    {
        return {};
    }

    void SaveRuntime(model::AccountAltRuntimeRecord const&) override {}
    void DeleteRuntime(std::uint64_t) override {}
};

struct Fixture
{
    FakeTemplateRepository    templateRepo;
    FakePreferenceRepository  preferenceRepo;
    FakeAltRuntimeRepository  altRuntimeRepo;

    BotTalentApplicator Make()
    {
        return BotTalentApplicator(templateRepo, preferenceRepo, altRuntimeRepo);
    }
};

// ── ApplyPreferredTemplate — repository-level behaviour ───────────────────────

TEST(BotTalentApplicatorTest, ReturnsFalseWhenPinnedTemplateIdIsNotFound)
{
    // Preference has a non-zero templateId but the template repo has no match.
    // The code returns false before touching the Player pointer.
    Fixture f;
    model::BotTalentPreference pref;
    pref.sourceCharacterGuid = 42;
    pref.templateId          = 99; // non-zero → pinned
    pref.autoApplyOnLevel    = true;
    f.preferenceRepo.storedPref = pref;
    f.templateRepo.storedTemplate = std::nullopt; // not found

    auto applicator = f.Make();
    bool const result = applicator.ApplyPreferredTemplate(nullptr, 42);

    EXPECT_FALSE(result);
    EXPECT_EQ(f.templateRepo.lastFindTemplateId, 99u);
}

TEST(BotTalentApplicatorTest, LooksUpPreferenceUsingSuppliedSourceCharGuid)
{
    // Verifies the correct GUID is forwarded to the preference repository.
    Fixture f;
    f.preferenceRepo.storedPref = std::nullopt;

    // With no preference and null bot, auto-detect branch would dereference
    // bot — skip calling it by accepting that the function returns false.
    // We only care that the preference repo received the right GUID.
    // To avoid the bot dereference we inject a pinned templateId that is
    // missing from the template repo, so the function exits early.
    model::BotTalentPreference pref;
    pref.sourceCharacterGuid = 77;
    pref.templateId          = 1; // pinned, but not found
    f.preferenceRepo.storedPref = pref;
    f.templateRepo.storedTemplate = std::nullopt;

    auto applicator = f.Make();
    applicator.ApplyPreferredTemplate(nullptr, 77);

    EXPECT_EQ(f.preferenceRepo.lastGetGuid, 77u);
}

TEST(BotTalentApplicatorTest, ForwardsTemplateIdToTemplateRepositoryLookup)
{
    // Verifies the template repo is called with the exact id stored in the
    // preference, not a guessed or default value.
    Fixture f;
    model::BotTalentPreference pref;
    pref.sourceCharacterGuid = 1;
    pref.templateId          = 55;
    f.preferenceRepo.storedPref = pref;
    f.templateRepo.storedTemplate = std::nullopt;

    auto applicator = f.Make();
    applicator.ApplyPreferredTemplate(nullptr, 1);

    EXPECT_EQ(f.templateRepo.lastFindTemplateId, 55u);
}

} // namespace
} // namespace service
} // namespace living_world
