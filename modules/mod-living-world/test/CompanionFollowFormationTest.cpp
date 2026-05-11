#include "ai/CompanionFollowFormation.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace ai
{

TEST(CompanionFollowFormationTest, UsesStableRosterIndexBeforeGuidModulo)
{
    CompanionFollowFormationInput input;
    input.formation = model::FollowFormation::Ring;
    input.baseDistance = 2.5f;
    input.slotCount = 7;
    input.botGuid = 33;
    input.ownerBotGuids = { 33, 31, 32, 30 };

    CompanionFollowFormationResult const result = ResolveCompanionFollowFormation(input);
    EXPECT_TRUE(result.usedRosterSlot);
    EXPECT_EQ(result.rosterSize, 4u);
    EXPECT_EQ(result.rosterIndex, 3u);
    EXPECT_EQ(result.slot, 3u);
}

TEST(CompanionFollowFormationTest, ProducesUniqueSlotsForDistinctRosterMembers)
{
    std::vector<std::uint32_t> slots;
    for (std::uint64_t botGuid : { 30ull, 31ull, 32ull, 33ull })
    {
        CompanionFollowFormationInput input;
        input.formation = model::FollowFormation::Ring;
        input.baseDistance = 2.5f;
        input.slotCount = 7;
        input.botGuid = botGuid;
        input.ownerBotGuids = { 30, 31, 32, 33 };

        slots.push_back(ResolveCompanionFollowFormation(input).slot);
    }

    EXPECT_EQ(slots[0], 0u);
    EXPECT_EQ(slots[1], 1u);
    EXPECT_EQ(slots[2], 2u);
    EXPECT_EQ(slots[3], 3u);
}

TEST(CompanionFollowFormationTest, FallsBackToGuidModuloWhenBotMissingFromRoster)
{
    CompanionFollowFormationInput input;
    input.formation = model::FollowFormation::Ring;
    input.baseDistance = 2.5f;
    input.slotCount = 7;
    input.botGuid = 33;
    input.ownerBotGuids = { 30, 31, 32 };

    CompanionFollowFormationResult const result = ResolveCompanionFollowFormation(input);
    EXPECT_FALSE(result.usedRosterSlot);
    EXPECT_EQ(result.rosterSize, 3u);
    EXPECT_EQ(result.slot, 5u);
}

TEST(CompanionFollowFormationTest, LineFormationStaggersDistanceBySlot)
{
    CompanionFollowFormationInput input;
    input.formation = model::FollowFormation::Line;
    input.baseDistance = 1.0f;
    input.slotCount = 7;
    input.botGuid = 32;
    input.ownerBotGuids = { 30, 31, 32, 33 };

    CompanionFollowFormationResult const result = ResolveCompanionFollowFormation(input);
    EXPECT_TRUE(result.usedRosterSlot);
    EXPECT_EQ(result.slot, 2u);
    EXPECT_NEAR(result.distance, 4.0f, 0.001f);
}

TEST(CompanionFollowFormationTest, RingFormationDistributesAnglesBySlot)
{
    CompanionFollowFormationInput inputA;
    inputA.formation = model::FollowFormation::Ring;
    inputA.baseDistance = 2.5f;
    inputA.slotCount = 7;
    inputA.botGuid = 30;
    inputA.ownerBotGuids = { 30, 31, 32, 33 };

    CompanionFollowFormationInput inputB = inputA;
    inputB.botGuid = 31;

    CompanionFollowFormationResult const resultA = ResolveCompanionFollowFormation(inputA);
    CompanionFollowFormationResult const resultB = ResolveCompanionFollowFormation(inputB);

    EXPECT_NEAR(resultA.distance, 2.5f, 0.001f);
    EXPECT_NEAR(resultB.distance, 2.5f, 0.001f);
    EXPECT_NE(resultA.slot, resultB.slot);
    EXPECT_NEAR(resultB.angle - resultA.angle, 2.0f * 3.14159265358979323846f / 7.0f, 0.001f);
}

} // namespace ai
} // namespace living_world