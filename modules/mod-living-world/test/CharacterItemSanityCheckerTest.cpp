#include "service/CharacterItemSanityChecker.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
model::CharacterItemSnapshotEntry EquipmentItem(
    std::uint64_t itemGuid,
    std::uint8_t slot,
    std::uint32_t itemEntry)
{
    model::CharacterItemSnapshotEntry item;
    item.itemGuid = itemGuid;
    item.slot = slot;
    item.itemEntry = itemEntry;
    item.itemCount = 1;
    item.domain = model::CharacterItemStorageDomain::Equipment;
    return item;
}

model::CharacterItemSnapshotEntry Item(
    std::uint64_t itemGuid,
    std::uint8_t slot,
    std::uint32_t itemEntry,
    model::CharacterItemStorageDomain domain)
{
    model::CharacterItemSnapshotEntry item;
    item.itemGuid = itemGuid;
    item.slot = slot;
    item.itemEntry = itemEntry;
    item.itemCount = 1;
    item.domain = domain;
    return item;
}
} // namespace

TEST(CharacterItemSanityCheckerTest, ApprovesEquipmentDomainWhenOnlyGearDiffers)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;
    source.equipmentItems.push_back(EquipmentItem(1001, 15, 5001));
    clone.equipmentItems.push_back(EquipmentItem(2001, 15, 5002));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    ASSERT_EQ(result.safeDomains.size(), 1u);
    EXPECT_EQ(result.safeDomains[0], model::AccountAltSyncDomain::Equipment);
}

TEST(CharacterItemSanityCheckerTest, PassesWithoutApprovingWhenEquipmentMatches)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;
    source.equipmentItems.push_back(EquipmentItem(1001, 15, 5001));
    clone.equipmentItems.push_back(EquipmentItem(2001, 15, 5001));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.safeDomains.empty());
}

TEST(CharacterItemSanityCheckerTest, RejectsDuplicateItemGuids)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;
    source.inventoryItems.push_back(
        Item(1001, 23, 5001, model::CharacterItemStorageDomain::Inventory));
    source.bankItems.push_back(
        Item(1001, 39, 5002, model::CharacterItemStorageDomain::Bank));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}

TEST(CharacterItemSanityCheckerTest, RejectsUncategorizedItems)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;
    clone.otherItems.push_back(
        Item(1001, 0, 5001, model::CharacterItemStorageDomain::Other));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}

TEST(CharacterItemSanityCheckerTest, RejectsInvalidEquipmentShape)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;
    auto item = EquipmentItem(1001, 30, 5001);
    clone.equipmentItems.push_back(item);

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}
} // namespace service
} // namespace living_world
