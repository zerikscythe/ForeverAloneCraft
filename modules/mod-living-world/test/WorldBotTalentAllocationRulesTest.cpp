#include "service/WorldBotTalentAllocationRules.h"

#include "SharedDefines.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
constexpr std::uint32_t MageClassMask = 1u << (CLASS_MAGE - 1);

model::BotTalentTemplateEntry MakeTemplateEntry(
    std::uint64_t entryId,
    std::uint16_t talentId,
    std::uint16_t priority,
    std::uint8_t desiredRank)
{
    model::BotTalentTemplateEntry entry;
    entry.entryId = entryId;
    entry.talentId = talentId;
    entry.priority = priority;
    entry.desiredRank = desiredRank;
    return entry;
}

WorldBotTalentDefinition MakeTalentDefinition(
    std::uint16_t talentId,
    std::uint32_t tabId,
    std::uint8_t row,
    std::array<std::uint32_t, detail::WorldBotMaxTalentRank> rankSpellIds,
    std::uint16_t dependsOnTalentId = 0,
    std::uint8_t dependsOnRequiredRank = 0,
    std::uint32_t classMask = MageClassMask)
{
    WorldBotTalentDefinition definition;
    definition.talentId = talentId;
    definition.talentTabId = tabId;
    definition.classMask = classMask;
    definition.row = row;
    definition.dependsOnTalentId = dependsOnTalentId;
    definition.dependsOnRequiredRank = dependsOnRequiredRank;
    definition.rankSpellIds = rankSpellIds;
    return definition;
}
} // namespace

TEST(WorldBotTalentAllocationRulesTest, AllocatesRanksAcrossPassesUntilDesiredRanksReached)
{
    std::vector<model::BotTalentTemplateEntry> templateEntries = {
        MakeTemplateEntry(1, 100, 0, 3),
        MakeTemplateEntry(2, 200, 1, 2)
    };

    std::vector<WorldBotTalentDefinition> definitions = {
        MakeTalentDefinition(100, 10, 0, { 1001, 1002, 1003, 0, 0 }),
        MakeTalentDefinition(200, 10, 0, { 2001, 2002, 0, 0, 0 })
    };

    std::uint8_t allocatedPoints = 0;
    std::vector<model::WorldBotPreparedTalentEntry> const allocated =
        AllocateWorldBotTalents(templateEntries, definitions, MageClassMask, 5, allocatedPoints);

    ASSERT_EQ(allocated.size(), 2u);
    EXPECT_EQ(allocatedPoints, 5u);
    EXPECT_EQ(allocated[0].talentId, 100u);
    EXPECT_EQ(allocated[0].allocatedRank, 3u);
    EXPECT_EQ(allocated[0].grantedSpellId, 1003u);
    EXPECT_EQ(allocated[1].talentId, 200u);
    EXPECT_EQ(allocated[1].allocatedRank, 2u);
    EXPECT_EQ(allocated[1].grantedSpellId, 2002u);
}

TEST(WorldBotTalentAllocationRulesTest, RespectsTierPointRequirementBeforeAllocatingHigherRowTalent)
{
    std::vector<model::BotTalentTemplateEntry> templateEntries = {
        MakeTemplateEntry(1, 100, 0, 5),
        MakeTemplateEntry(2, 200, 1, 1)
    };

    std::vector<WorldBotTalentDefinition> definitions = {
        MakeTalentDefinition(100, 10, 0, { 1001, 1002, 1003, 1004, 1005 }),
        MakeTalentDefinition(200, 10, 1, { 2001, 0, 0, 0, 0 })
    };

    std::uint8_t allocatedPoints = 0;
    std::vector<model::WorldBotPreparedTalentEntry> const allocated =
        AllocateWorldBotTalents(templateEntries, definitions, MageClassMask, 6, allocatedPoints);

    ASSERT_EQ(allocated.size(), 2u);
    EXPECT_EQ(allocatedPoints, 6u);
    EXPECT_EQ(allocated[0].allocatedRank, 5u);
    EXPECT_EQ(allocated[1].allocatedRank, 1u);
    EXPECT_EQ(allocated[1].grantedSpellId, 2001u);
}

TEST(WorldBotTalentAllocationRulesTest, RespectsTalentDependencyRankRequirement)
{
    std::vector<model::BotTalentTemplateEntry> templateEntries = {
        MakeTemplateEntry(1, 100, 0, 2),
        MakeTemplateEntry(2, 200, 1, 1)
    };

    std::vector<WorldBotTalentDefinition> definitions = {
        MakeTalentDefinition(100, 10, 0, { 1001, 1002, 0, 0, 0 }),
        MakeTalentDefinition(200, 10, 0, { 2001, 0, 0, 0, 0 }, 100, 2)
    };

    std::uint8_t allocatedPoints = 0;
    std::vector<model::WorldBotPreparedTalentEntry> const allocated =
        AllocateWorldBotTalents(templateEntries, definitions, MageClassMask, 3, allocatedPoints);

    ASSERT_EQ(allocated.size(), 2u);
    EXPECT_EQ(allocated[0].allocatedRank, 2u);
    EXPECT_EQ(allocated[1].allocatedRank, 1u);
}

TEST(WorldBotTalentAllocationRulesTest, SkipsTalentsForWrongClassMask)
{
    std::vector<model::BotTalentTemplateEntry> templateEntries = {
        MakeTemplateEntry(1, 100, 0, 3)
    };

    std::vector<WorldBotTalentDefinition> definitions = {
        MakeTalentDefinition(100, 10, 0, { 1001, 1002, 1003, 0, 0 }, 0, 0, 1u << (CLASS_WARRIOR - 1))
    };

    std::uint8_t allocatedPoints = 0;
    std::vector<model::WorldBotPreparedTalentEntry> const allocated =
        AllocateWorldBotTalents(templateEntries, definitions, MageClassMask, 3, allocatedPoints);

    EXPECT_TRUE(allocated.empty());
    EXPECT_EQ(allocatedPoints, 0u);
}
} // namespace service
} // namespace living_world