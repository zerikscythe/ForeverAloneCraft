#include "service/WorldBotCombatSituationBuilder.h"

#include "SharedDefines.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
model::WorldBotPreparedBuild MakeBuild(
    std::uint8_t classId,
    char const* specKey,
    char const* roleKey)
{
    model::WorldBotPreparedBuild build;
    build.classId = classId;
    build.canonicalSpecKey = specKey;
    build.resolvedRoleKey = roleKey;
    return build;
}
} // namespace

TEST(WorldBotCombatSituationBuilderTest, ResolvesTankStyleFromRole)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_WARRIOR, "Protection", "TANK");
    EXPECT_EQ(ResolveWorldBotMovementStyle(build), model::WorldBotMovementStyle::FrontlineTank);
}

TEST(WorldBotCombatSituationBuilderTest, ResolvesHealerStyleFromRole)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_PRIEST, "Holy", "HEAL");
    EXPECT_EQ(ResolveWorldBotMovementStyle(build), model::WorldBotMovementStyle::BacklineHealer);
}

TEST(WorldBotCombatSituationBuilderTest, ResolvesHunterAsMobileRanged)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_HUNTER, "Beast Mastery", "DPS");
    EXPECT_EQ(ResolveWorldBotMovementStyle(build), model::WorldBotMovementStyle::MobileRanged);
}

TEST(WorldBotCombatSituationBuilderTest, ResolvesMageAsTurretCaster)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_MAGE, "Frost", "DPS");
    EXPECT_EQ(ResolveWorldBotMovementStyle(build), model::WorldBotMovementStyle::TurretCaster);
}

TEST(WorldBotCombatSituationBuilderTest, ResolvesRogueAsStickyMelee)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_ROGUE, "Assassination", "DPS");
    EXPECT_EQ(ResolveWorldBotMovementStyle(build), model::WorldBotMovementStyle::StickyMelee);
}

TEST(WorldBotCombatSituationBuilderTest, BuildsCastSafetyForRangedAtSafeDistance)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_MAGE, "Frost", "DPS");
    model::WorldBotCombatSituation const situation = BuildWorldBotCombatSituation(
        build,
        true,
        24.0f,
        90.0f,
        100.0f,
        1u);

    EXPECT_TRUE(situation.isRangedStyle);
    EXPECT_TRUE(situation.canCastSafely);
}

TEST(WorldBotCombatSituationBuilderTest, BuildsUnsafeCastWindowWhenRangedTargetTooClose)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_MAGE, "Frost", "DPS");
    model::WorldBotCombatSituation const situation = BuildWorldBotCombatSituation(
        build,
        true,
        6.0f,
        90.0f,
        100.0f,
        2u);

    EXPECT_TRUE(situation.isRangedStyle);
    EXPECT_FALSE(situation.canCastSafely);
}

TEST(WorldBotCombatSituationBuilderTest, CarriesHazardSnapshotIntoSituation)
{
    model::WorldBotPreparedBuild const build = MakeBuild(CLASS_PRIEST, "Holy", "HEAL");
    model::WorldBotCombatSituation const situation = BuildWorldBotCombatSituation(
        build,
        true,
        18.0f,
        70.0f,
        100.0f,
        1u,
        true,
        0.75f);

    EXPECT_TRUE(situation.hazard.active);
    EXPECT_FLOAT_EQ(situation.hazard.severity, 0.75f);
    EXPECT_EQ(situation.movementStyle, model::WorldBotMovementStyle::BacklineHealer);
}

} // namespace service
} // namespace living_world