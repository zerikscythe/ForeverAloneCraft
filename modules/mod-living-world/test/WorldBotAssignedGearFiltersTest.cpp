#include "service/WorldBotAssignedGearFilters.h"
#include "service/WorldBotAssignedGearSummary.h"

#include "gtest/gtest.h"

namespace living_world
{
namespace service
{
namespace
{
ItemTemplate MakeItem(
    std::uint32_t inventoryType,
    std::uint32_t itemClass,
    std::uint32_t subClass,
    std::initializer_list<std::pair<std::uint32_t, std::int32_t>> stats = {})
{
    ItemTemplate item{};
    item.InventoryType = inventoryType;
    item.Class = itemClass;
    item.SubClass = subClass;

    std::uint32_t index = 0;
    for (auto const& [statType, statValue] : stats)
    {
        if (index >= MAX_ITEM_PROTO_STATS)
            break;

        item.ItemStat[index].ItemStatType = statType;
        item.ItemStat[index].ItemStatValue = statValue;
        ++index;
    }

    item.StatsCount = index;
    return item;
}
} // namespace

TEST(WorldBotAssignedGearFiltersTest, CasterGateRejectsMeleeCloak)
{
    ItemTemplate meleeCloak = MakeItem(
        INVTYPE_CLOAK,
        ITEM_CLASS_ARMOR,
        ITEM_SUBCLASS_ARMOR_CLOTH,
        {
            { ITEM_MOD_STRENGTH, 12 },
            { ITEM_MOD_STAMINA, 18 },
        });

    EXPECT_FALSE(MatchesAssignedGearStatFamily(
        &meleeCloak,
        DetermineAssignedGearStatFamily(CLASS_MAGE, "Arcane", "DPS")));
}

TEST(WorldBotAssignedGearFiltersTest, CasterGateAcceptsCasterCloak)
{
    ItemTemplate casterCloak = MakeItem(
        INVTYPE_CLOAK,
        ITEM_CLASS_ARMOR,
        ITEM_SUBCLASS_ARMOR_CLOTH,
        {
            { ITEM_MOD_INTELLECT, 15 },
            { ITEM_MOD_SPELL_POWER, 24 },
        });

    EXPECT_TRUE(MatchesAssignedGearStatFamily(
        &casterCloak,
        DetermineAssignedGearStatFamily(CLASS_MAGE, "Arcane", "DPS")));
}

TEST(WorldBotAssignedGearFiltersTest, MageCannotUseShieldOffhand)
{
    ItemTemplate shield = MakeItem(INVTYPE_SHIELD, ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_SHIELD);

    EXPECT_FALSE(IsAssignedGearOffhandCompatible(
        &shield,
        nullptr,
        CLASS_MAGE,
        "Arcane",
        "DPS"));
}

TEST(WorldBotAssignedGearFiltersTest, ElementalShamanCanUseShieldOffhand)
{
    ItemTemplate shield = MakeItem(INVTYPE_SHIELD, ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_SHIELD);

    EXPECT_TRUE(IsAssignedGearOffhandCompatible(
        &shield,
        nullptr,
        CLASS_SHAMAN,
        "Elemental",
        "DPS"));
}

TEST(WorldBotAssignedGearFiltersTest, TwoHandMainhandBlocksOffhand)
{
    ItemTemplate staff = MakeItem(INVTYPE_2HWEAPON, ITEM_CLASS_WEAPON, ITEM_SUBCLASS_WEAPON_STAFF);
    ItemTemplate holdable = MakeItem(INVTYPE_HOLDABLE, ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_MISC,
        {
            { ITEM_MOD_INTELLECT, 10 },
        });

    EXPECT_FALSE(IsAssignedGearOffhandCompatible(
        &holdable,
        &staff,
        CLASS_MAGE,
        "Arcane",
        "DPS"));
    EXPECT_FALSE(IsAssignedGearMainhandCompatible(
        &staff,
        &holdable,
        CLASS_MAGE,
        "Arcane",
        "DPS"));
}

TEST(WorldBotAssignedGearFiltersTest, EnhancementShamanCanUseWeaponOffhand)
{
    ItemTemplate offhandAxe = MakeItem(INVTYPE_WEAPONOFFHAND, ITEM_CLASS_WEAPON, ITEM_SUBCLASS_WEAPON_AXE,
        {
            { ITEM_MOD_AGILITY, 9 },
        });
    ItemTemplate mainhandAxe = MakeItem(INVTYPE_WEAPON, ITEM_CLASS_WEAPON, ITEM_SUBCLASS_WEAPON_AXE);

    EXPECT_TRUE(IsAssignedGearOffhandCompatible(
        &offhandAxe,
        &mainhandAxe,
        CLASS_SHAMAN,
        "Enhancement",
        "DPS"));
}

TEST(WorldBotAssignedGearFiltersTest, ArcaneMageCannotUseWeaponOffhand)
{
    ItemTemplate offhandSword = MakeItem(INVTYPE_WEAPONOFFHAND, ITEM_CLASS_WEAPON, ITEM_SUBCLASS_WEAPON_SWORD);

    EXPECT_FALSE(IsAssignedGearOffhandCompatible(
        &offhandSword,
        nullptr,
        CLASS_MAGE,
        "Arcane",
        "DPS"));
}

TEST(WorldBotAssignedGearSummaryTest, GenericRatingsExpandToAllRelevantBuckets)
{
    model::WorldBotAssignedGearSummary summary;

    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_HIT_RATING, 12);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_CRIT_RATING, 18);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_HASTE_RATING, 24);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_SPELL_POWER, 42);

    EXPECT_EQ(summary.bonusMeleeHitRating, 12);
    EXPECT_EQ(summary.bonusRangedHitRating, 12);
    EXPECT_EQ(summary.bonusSpellHitRating, 12);
    EXPECT_EQ(summary.bonusMeleeCritRating, 18);
    EXPECT_EQ(summary.bonusRangedCritRating, 18);
    EXPECT_EQ(summary.bonusSpellCritRating, 18);
    EXPECT_EQ(summary.bonusMeleeHasteRating, 24);
    EXPECT_EQ(summary.bonusRangedHasteRating, 24);
    EXPECT_EQ(summary.bonusSpellHasteRating, 24);
    EXPECT_EQ(summary.bonusSpellPower, 42);
    EXPECT_EQ(summary.bonusHealingPower, 42);
}

TEST(WorldBotAssignedGearSummaryTest, SecondaryRatingsAreCarriedForRuntimeSlices)
{
    model::WorldBotAssignedGearSummary summary;

    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_DEFENSE_SKILL_RATING, 9);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_DODGE_RATING, 10);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_PARRY_RATING, 11);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_BLOCK_RATING, 12);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_BLOCK_VALUE, 35);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_EXPERTISE_RATING, 14);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_ARMOR_PENETRATION_RATING, 15);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_RESILIENCE_RATING, 16);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_MANA_REGENERATION, 17);
    AccumulateWorldBotAssignedGearStat(summary, ITEM_MOD_SPELL_PENETRATION, 18);

    EXPECT_EQ(summary.bonusDefenseSkillRating, 9);
    EXPECT_EQ(summary.bonusDodgeRating, 10);
    EXPECT_EQ(summary.bonusParryRating, 11);
    EXPECT_EQ(summary.bonusBlockRating, 12);
    EXPECT_EQ(summary.bonusBlockValue, 35);
    EXPECT_EQ(summary.bonusExpertiseRating, 14);
    EXPECT_EQ(summary.bonusArmorPenetrationRating, 15);
    EXPECT_EQ(summary.bonusResilienceRating, 16);
    EXPECT_EQ(summary.bonusManaRegen, 17);
    EXPECT_EQ(summary.bonusSpellPenetration, 18);
}
} // namespace service
} // namespace living_world
