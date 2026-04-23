#include "service/CharacterItemSnapshotClassifier.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
TEST(CharacterItemSnapshotClassifierTest, ClassifiesEquipmentRootSlots)
{
    CharacterItemSnapshotClassifier classifier;
    auto domain = classifier.RootDomainForSlot(17);

    ASSERT_TRUE(domain.has_value());
    EXPECT_EQ(*domain, model::CharacterItemStorageDomain::Equipment);
}

TEST(CharacterItemSnapshotClassifierTest, ClassifiesInventoryRootSlots)
{
    CharacterItemSnapshotClassifier classifier;
    auto domain = classifier.RootDomainForSlot(23);

    ASSERT_TRUE(domain.has_value());
    EXPECT_EQ(*domain, model::CharacterItemStorageDomain::Inventory);
}

TEST(CharacterItemSnapshotClassifierTest, ClassifiesBankRootSlots)
{
    CharacterItemSnapshotClassifier classifier;
    auto domain = classifier.RootDomainForSlot(67);

    ASSERT_TRUE(domain.has_value());
    EXPECT_EQ(*domain, model::CharacterItemStorageDomain::Bank);
}

TEST(CharacterItemSnapshotClassifierTest, TreatsNestedEquipmentBagAsInventory)
{
    CharacterItemSnapshotClassifier classifier;
    EXPECT_EQ(
        classifier.NestedDomainForContainer(
            model::CharacterItemStorageDomain::Equipment),
        model::CharacterItemStorageDomain::Inventory);
}

TEST(CharacterItemSnapshotClassifierTest, TreatsNestedBankBagAsBank)
{
    CharacterItemSnapshotClassifier classifier;
    EXPECT_EQ(
        classifier.NestedDomainForContainer(
            model::CharacterItemStorageDomain::Bank),
        model::CharacterItemStorageDomain::Bank);
}
} // namespace service
} // namespace living_world
