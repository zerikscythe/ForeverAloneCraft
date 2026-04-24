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

TEST(CharacterItemSanityCheckerTest, ApprovesInventoryDomainWhenBagContentsDiffer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    source.inventoryItems.push_back(
        Item(1001, 20, 5001, model::CharacterItemStorageDomain::Inventory));
    clone.inventoryItems.push_back(
        Item(2001, 20, 5002, model::CharacterItemStorageDomain::Inventory));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    ASSERT_EQ(result.safeDomains.size(), 1u);
    EXPECT_EQ(result.safeDomains[0], model::AccountAltSyncDomain::Inventory);
}

TEST(CharacterItemSanityCheckerTest,
     DoesNotApproveInventoryWhenLogicalBagContentsMatchAcrossDifferentGuids)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    source.inventoryItems.push_back(
        Item(1001, 19, 6501, model::CharacterItemStorageDomain::Inventory));
    clone.inventoryItems.push_back(
        Item(2001, 19, 6501, model::CharacterItemStorageDomain::Inventory));

    auto sourceNested =
        Item(3001, 0, 7001, model::CharacterItemStorageDomain::Inventory);
    sourceNested.containerGuid = 1001;
    source.inventoryItems.push_back(sourceNested);

    auto cloneNested =
        Item(4001, 0, 7001, model::CharacterItemStorageDomain::Inventory);
    cloneNested.containerGuid = 2001;
    clone.inventoryItems.push_back(cloneNested);

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.safeDomains.empty());
}

TEST(CharacterItemSanityCheckerTest, ApprovesBankDomainWhenBankContentsDiffer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    source.bankItems.push_back(
        Item(1001, 39, 5001, model::CharacterItemStorageDomain::Bank));
    clone.bankItems.push_back(
        Item(2001, 39, 5002, model::CharacterItemStorageDomain::Bank));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    ASSERT_EQ(result.safeDomains.size(), 1u);
    EXPECT_EQ(result.safeDomains[0], model::AccountAltSyncDomain::Bank);
}

TEST(CharacterItemSanityCheckerTest, RejectsDuplicateNestedContainerSlots)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    clone.inventoryItems.push_back(
        Item(5000, 19, 7001, model::CharacterItemStorageDomain::Inventory));
    auto first = Item(1001, 0, 5001,
                      model::CharacterItemStorageDomain::Inventory);
    first.containerGuid = 5000;
    clone.inventoryItems.push_back(first);

    auto second = Item(1002, 0, 5002,
                       model::CharacterItemStorageDomain::Inventory);
    second.containerGuid = 5000;
    clone.inventoryItems.push_back(second);

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}

TEST(CharacterItemSanityCheckerTest, RejectsInventoryItemWithMissingContainer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    auto nested =
        Item(1001, 1, 5001, model::CharacterItemStorageDomain::Inventory);
    nested.containerGuid = 9999;
    clone.inventoryItems.push_back(nested);

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}

TEST(CharacterItemSanityCheckerTest, RejectsBankItemInsideInventoryContainer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    clone.inventoryItems.push_back(
        Item(3001, 20, 6001, model::CharacterItemStorageDomain::Inventory));
    auto bankNested = Item(1001, 1, 5001, model::CharacterItemStorageDomain::Bank);
    bankNested.containerGuid = 3001;
    clone.bankItems.push_back(bankNested);

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}

TEST(CharacterItemSanityCheckerTest, RejectsExcessiveContainerSize)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    clone.inventoryItems.push_back(
        Item(5000, 19, 7001, model::CharacterItemStorageDomain::Inventory));
    for (std::uint8_t i = 0; i < 33; ++i)
    {
        auto nested = Item(static_cast<std::uint64_t>(1000 + i), i, 5001,
                           model::CharacterItemStorageDomain::Inventory);
        nested.containerGuid = 5000;
        clone.inventoryItems.push_back(nested);
    }

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
}

TEST(CharacterItemSanityCheckerTest, SetsBagContainersChangedWhenInventoryBagsDiffer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    source.inventoryItems.push_back(
        Item(1001, 19, 6501, model::CharacterItemStorageDomain::Inventory));
    clone.inventoryItems.push_back(
        Item(2001, 19, 6502, model::CharacterItemStorageDomain::Inventory));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.bagContainersChanged);
}

TEST(CharacterItemSanityCheckerTest, SetsBagContainersChangedWhenBankBagsDiffer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    source.bankItems.push_back(
        Item(1001, 67, 6501, model::CharacterItemStorageDomain::Bank));
    clone.bankItems.push_back(
        Item(2001, 67, 6502, model::CharacterItemStorageDomain::Bank));

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.bagContainersChanged);
}

TEST(CharacterItemSanityCheckerTest, DoesNotSetBagContainersChangedWhenOnlyContentsDiffer)
{
    CharacterItemSanityChecker checker;
    model::CharacterItemSnapshot source;
    model::CharacterItemSnapshot clone;

    // Same bag type at slot 19 in both snapshots.
    source.inventoryItems.push_back(
        Item(1001, 19, 6501, model::CharacterItemStorageDomain::Inventory));
    clone.inventoryItems.push_back(
        Item(2001, 19, 6501, model::CharacterItemStorageDomain::Inventory));

    // Different items inside the bags.
    auto srcItem = Item(3001, 0, 7001, model::CharacterItemStorageDomain::Inventory);
    srcItem.containerGuid = 1001;
    source.inventoryItems.push_back(srcItem);

    auto cloneItem = Item(4001, 0, 7002, model::CharacterItemStorageDomain::Inventory);
    cloneItem.containerGuid = 2001;
    clone.inventoryItems.push_back(cloneItem);

    model::AccountAltSanityCheckResult result = checker.Check(source, clone);

    EXPECT_TRUE(result.passed);
    EXPECT_FALSE(result.bagContainersChanged);
}
} // namespace service
} // namespace living_world
