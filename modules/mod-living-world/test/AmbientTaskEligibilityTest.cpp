#include "service/AmbientTaskEligibility.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{

TEST(AmbientTaskEligibilityTest, ActivityRejectsMissingRequiredProfession)
{
    model::ActivityEntry entry;
    entry.requiresHerbalism = true;

    AmbientProfessionCapabilities caps;
    EXPECT_FALSE(MeetsProfessionRequirements(entry, caps));

    caps.hasHerbalism = true;
    EXPECT_TRUE(MeetsProfessionRequirements(entry, caps));
}

TEST(AmbientTaskEligibilityTest, TemplateRequiresAllListedProfessions)
{
    model::TaskTemplateEntry entry;
    entry.requiresMining = true;
    entry.requiresFishing = true;

    AmbientProfessionCapabilities caps;
    caps.hasMining = true;
    EXPECT_FALSE(MeetsProfessionRequirements(entry, caps));

    caps.hasFishing = true;
    EXPECT_TRUE(MeetsProfessionRequirements(entry, caps));
}

TEST(AmbientTaskEligibilityTest, PlaylistAllowsNeutralEntriesWithoutProfessions)
{
    model::PlaylistEntrySet entry;
    AmbientProfessionCapabilities caps;

    EXPECT_TRUE(MeetsProfessionRequirements(entry, caps));
}

TEST(AmbientTaskEligibilityTest, PlaylistRejectsFishingWhenCapabilityMissing)
{
    model::PlaylistEntrySet entry;
    entry.requiresFishing = true;

    AmbientProfessionCapabilities caps;
    EXPECT_FALSE(MeetsProfessionRequirements(entry, caps));

    caps.hasFishing = true;
    EXPECT_TRUE(MeetsProfessionRequirements(entry, caps));
}

TEST(AmbientTaskEligibilityTest, MultipleRequirementsCanAllPassTogether)
{
    model::ActivityEntry entry;
    entry.requiresHerbalism = true;
    entry.requiresMining = true;
    entry.requiresFishing = true;

    AmbientProfessionCapabilities caps;
    caps.hasHerbalism = true;
    caps.hasMining = true;
    caps.hasFishing = true;

    EXPECT_TRUE(MeetsProfessionRequirements(entry, caps));
}

} // namespace service
} // namespace living_world