#include "integration/SqlCharacterInventorySyncRepository.h"

#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "StringFormat.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace living_world
{
namespace integration
{
namespace
{
struct ItemInstanceRow
{
    std::uint32_t itemEntry = 0;
    std::uint64_t ownerGuid = 0;
    std::uint32_t creatorGuid = 0;
    std::uint32_t giftCreatorGuid = 0;
    std::uint32_t count = 0;
    std::uint32_t duration = 0;
    std::string charges;
    std::uint32_t flags = 0;
    std::string enchantments;
    std::int32_t randomPropertyId = 0;
    std::uint32_t durability = 0;
    std::uint32_t playedTime = 0;
    std::string text;
};

std::optional<ItemInstanceRow> LoadItemInstance(std::uint64_t itemGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, "
        "duration, charges, flags, enchantments, randomPropertyId, durability, "
        "playedTime, text "
        "FROM item_instance WHERE guid = {} LIMIT 1",
        itemGuid);
    if (!result)
    {
        return std::nullopt;
    }

    Field const* fields = result->Fetch();
    ItemInstanceRow row;
    row.itemEntry = fields[0].Get<std::uint32_t>();
    row.ownerGuid = fields[1].Get<std::uint64_t>();
    row.creatorGuid = fields[2].Get<std::uint32_t>();
    row.giftCreatorGuid = fields[3].Get<std::uint32_t>();
    row.count = fields[4].Get<std::uint32_t>();
    row.duration = fields[5].Get<std::uint32_t>();
    row.charges = fields[6].Get<std::string>();
    row.flags = fields[7].Get<std::uint32_t>();
    row.enchantments = fields[8].Get<std::string>();
    row.randomPropertyId = fields[9].Get<std::int32_t>();
    row.durability = fields[10].Get<std::uint32_t>();
    row.playedTime = fields[11].Get<std::uint32_t>();
    row.text = fields[12].Get<std::string>();
    return row;
}

bool HasGiftData(std::uint64_t itemGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT item_guid FROM character_gifts WHERE item_guid = {} LIMIT 1",
        itemGuid);
    return result != nullptr;
}

void QuoteString(std::string& value)
{
    CharacterDatabase.EscapeString(value);
}
} // namespace

bool SqlCharacterInventorySyncRepository::SyncInventoryToCharacter(
    std::uint64_t sourceCharacterGuid,
    model::CharacterItemSnapshot const& sourceSnapshot,
    std::uint64_t cloneCharacterGuid,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    if (sourceCharacterGuid == 0 || cloneCharacterGuid == 0)
    {
        return false;
    }

    struct CloneInventoryCopy
    {
        model::CharacterItemSnapshotEntry item;
        ItemInstanceRow row;
    };

    std::vector<CloneInventoryCopy> cloneItems;
    cloneItems.reserve(cloneSnapshot.inventoryItems.size());
    for (model::CharacterItemSnapshotEntry const& item :
         cloneSnapshot.inventoryItems)
    {
        if (HasGiftData(item.itemGuid))
        {
            return false;
        }

        std::optional<ItemInstanceRow> row = LoadItemInstance(item.itemGuid);
        if (!row)
        {
            return false;
        }

        cloneItems.push_back(CloneInventoryCopy { item, *row });
    }

    for (model::CharacterItemSnapshotEntry const& item :
         sourceSnapshot.inventoryItems)
    {
        if (HasGiftData(item.itemGuid))
        {
            return false;
        }
    }

    std::unordered_map<std::uint64_t, std::uint64_t> remappedGuids;
    for (CloneInventoryCopy const& copy : cloneItems)
    {
        remappedGuids.emplace(
            copy.item.itemGuid,
            sObjectMgr->GetGenerator<HighGuid::Item>().Generate());
    }

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (model::CharacterItemSnapshotEntry const& item :
         sourceSnapshot.inventoryItems)
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM character_inventory WHERE guid = {} AND item = {}",
                sourceCharacterGuid,
                item.itemGuid));
    }

    for (model::CharacterItemSnapshotEntry const& item :
         sourceSnapshot.inventoryItems)
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM item_instance WHERE guid = {}",
                item.itemGuid));
    }

    for (CloneInventoryCopy const& copy : cloneItems)
    {
        std::uint64_t const newItemGuid = remappedGuids.at(copy.item.itemGuid);
        std::uint64_t const newBagGuid = copy.item.containerGuid == 0
            ? 0
            : remappedGuids.at(copy.item.containerGuid);

        std::string charges = copy.row.charges;
        std::string enchantments = copy.row.enchantments;
        std::string text = copy.row.text;
        QuoteString(charges);
        QuoteString(enchantments);
        QuoteString(text);

        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "REPLACE INTO item_instance "
                "(itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, "
                "duration, charges, flags, enchantments, randomPropertyId, "
                "durability, playedTime, text, guid) "
                "VALUES ({}, {}, {}, {}, {}, {}, '{}', {}, '{}', {}, {}, {}, '{}', {})",
                copy.row.itemEntry,
                sourceCharacterGuid,
                copy.row.creatorGuid,
                copy.row.giftCreatorGuid,
                copy.row.count,
                copy.row.duration,
                charges,
                copy.row.flags,
                enchantments,
                copy.row.randomPropertyId,
                copy.row.durability,
                copy.row.playedTime,
                text,
                newItemGuid));

        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "REPLACE INTO character_inventory (guid, bag, slot, item) "
                "VALUES ({}, {}, {}, {})",
                sourceCharacterGuid,
                newBagGuid,
                static_cast<std::uint32_t>(copy.item.slot),
                newItemGuid));
    }

    CharacterDatabase.CommitTransaction(trans);
    return true;
}
} // namespace integration
} // namespace living_world
