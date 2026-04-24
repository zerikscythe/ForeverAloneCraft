#include "service/CharacterItemSanityChecker.h"

#include <array>
#include <unordered_map>
#include <unordered_set>

namespace living_world
{
namespace service
{
namespace
{
bool HasDuplicateItemGuid(model::CharacterItemSnapshot const& snapshot)
{
    std::unordered_set<std::uint64_t> guids;
    auto insertAll = [&](std::vector<model::CharacterItemSnapshotEntry> const& items)
    {
        for (model::CharacterItemSnapshotEntry const& item : items)
        {
            if (item.itemGuid == 0 || !guids.insert(item.itemGuid).second)
            {
                return true;
            }
        }
        return false;
    };

    return insertAll(snapshot.equipmentItems) ||
           insertAll(snapshot.inventoryItems) ||
           insertAll(snapshot.bankItems) ||
           insertAll(snapshot.otherItems);
}

bool HasUncategorizedItems(model::CharacterItemSnapshot const& snapshot)
{
    return !snapshot.otherItems.empty();
}

bool HasInvalidEquipmentShape(model::CharacterItemSnapshot const& snapshot)
{
    std::unordered_set<std::uint8_t> slots;
    for (model::CharacterItemSnapshotEntry const& item : snapshot.equipmentItems)
    {
        if (item.containerGuid != 0 || item.slot > 18 || item.itemEntry == 0 ||
            item.itemCount == 0 || !slots.insert(item.slot).second)
        {
            return true;
        }
    }

    return false;
}

std::unordered_map<std::uint64_t, model::CharacterItemSnapshotEntry const*>
BuildItemIndex(model::CharacterItemSnapshot const& snapshot)
{
    std::unordered_map<std::uint64_t, model::CharacterItemSnapshotEntry const*> index;

    auto addAll = [&](std::vector<model::CharacterItemSnapshotEntry> const& items)
    {
        for (model::CharacterItemSnapshotEntry const& item : items)
        {
            index[item.itemGuid] = &item;
        }
    };

    addAll(snapshot.equipmentItems);
    addAll(snapshot.inventoryItems);
    addAll(snapshot.bankItems);
    addAll(snapshot.otherItems);
    return index;
}

bool IsValidRootInventorySlot(std::uint8_t slot)
{
    return (slot >= 19 && slot <= 38) || (slot >= 86 && slot <= 149);
}

bool IsValidRootBankSlot(std::uint8_t slot)
{
    return slot >= 39 && slot <= 73;
}

bool HasInvalidContainerShape(model::CharacterItemSnapshot const& snapshot)
{
    auto const index = BuildItemIndex(snapshot);

    auto validateItems =
        [&](std::vector<model::CharacterItemSnapshotEntry> const& items,
            model::CharacterItemStorageDomain itemDomain)
    {
        std::unordered_set<std::uint8_t> rootSlots;
        for (model::CharacterItemSnapshotEntry const& item : items)
        {
            if (item.itemEntry == 0 || item.itemCount == 0)
            {
                return true;
            }

            if (item.containerGuid == item.itemGuid)
            {
                return true;
            }

            if (item.containerGuid == 0)
            {
                if (itemDomain == model::CharacterItemStorageDomain::Inventory)
                {
                    if (!IsValidRootInventorySlot(item.slot) ||
                        !rootSlots.insert(item.slot).second)
                    {
                        return true;
                    }
                }
                else if (itemDomain == model::CharacterItemStorageDomain::Bank)
                {
                    if (!IsValidRootBankSlot(item.slot) ||
                        !rootSlots.insert(item.slot).second)
                    {
                        return true;
                    }
                }

                continue;
            }

            auto const itr = index.find(item.containerGuid);
            if (itr == index.end())
            {
                return true;
            }

            model::CharacterItemStorageDomain containerDomain = itr->second->domain;
            if (itemDomain == model::CharacterItemStorageDomain::Inventory)
            {
                if (containerDomain != model::CharacterItemStorageDomain::Inventory &&
                    containerDomain != model::CharacterItemStorageDomain::Equipment)
                {
                    return true;
                }
            }
            else if (itemDomain == model::CharacterItemStorageDomain::Bank)
            {
                if (containerDomain != model::CharacterItemStorageDomain::Bank)
                {
                    return true;
                }
            }
        }

        return false;
    };

    return validateItems(
               snapshot.inventoryItems,
               model::CharacterItemStorageDomain::Inventory) ||
           validateItems(
               snapshot.bankItems,
               model::CharacterItemStorageDomain::Bank);
}

bool EquipmentDiffers(
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    if (sourceSnapshot.equipmentItems.size() != cloneSnapshot.equipmentItems.size())
    {
        return true;
    }

    std::array<std::pair<std::uint32_t, std::uint32_t>, 19> sourceSlots {};
    std::array<std::pair<std::uint32_t, std::uint32_t>, 19> cloneSlots {};

    for (model::CharacterItemSnapshotEntry const& item : sourceSnapshot.equipmentItems)
    {
        sourceSlots[item.slot] = { item.itemEntry, item.itemCount };
    }

    for (model::CharacterItemSnapshotEntry const& item : cloneSnapshot.equipmentItems)
    {
        cloneSlots[item.slot] = { item.itemEntry, item.itemCount };
    }

    return sourceSlots != cloneSlots;
}

template <typename TItems>
std::unordered_map<std::string, std::uint32_t> BuildItemMultiset(TItems const& items)
{
    std::unordered_map<std::string, std::uint32_t> counts;
    for (model::CharacterItemSnapshotEntry const& item : items)
    {
        std::string key = std::to_string(item.containerGuid) + ":" +
                          std::to_string(item.slot) + ":" +
                          std::to_string(item.itemEntry) + ":" +
                          std::to_string(item.itemCount);
        ++counts[key];
    }

    return counts;
}

bool InventoryDiffers(
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    return BuildItemMultiset(sourceSnapshot.inventoryItems) !=
           BuildItemMultiset(cloneSnapshot.inventoryItems);
}

bool BankDiffers(
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    return BuildItemMultiset(sourceSnapshot.bankItems) !=
           BuildItemMultiset(cloneSnapshot.bankItems);
}
} // namespace

model::AccountAltSanityCheckResult CharacterItemSanityChecker::Check(
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot) const
{
    model::AccountAltSanityCheckResult result;

    if (HasDuplicateItemGuid(sourceSnapshot) || HasDuplicateItemGuid(cloneSnapshot))
    {
        result.failures.push_back("item snapshot contains duplicate or missing item guids");
    }

    if (HasUncategorizedItems(sourceSnapshot) || HasUncategorizedItems(cloneSnapshot))
    {
        result.failures.push_back("item snapshot contains uncategorized storage state");
    }

    if (HasInvalidEquipmentShape(sourceSnapshot) || HasInvalidEquipmentShape(cloneSnapshot))
    {
        result.failures.push_back("equipment snapshot has invalid slot/container shape");
    }

    if (HasInvalidContainerShape(sourceSnapshot) || HasInvalidContainerShape(cloneSnapshot))
    {
        result.failures.push_back("inventory/bank snapshot has invalid container shape");
    }

    if (result.failures.empty() &&
        EquipmentDiffers(sourceSnapshot, cloneSnapshot))
    {
        result.safeDomains.push_back(model::AccountAltSyncDomain::Equipment);
    }

    if (result.failures.empty() &&
        InventoryDiffers(sourceSnapshot, cloneSnapshot))
    {
        result.safeDomains.push_back(model::AccountAltSyncDomain::Inventory);
    }

    if (result.failures.empty() &&
        BankDiffers(sourceSnapshot, cloneSnapshot))
    {
        result.safeDomains.push_back(model::AccountAltSyncDomain::Bank);
    }

    result.passed = result.failures.empty();
    return result;
}
} // namespace service
} // namespace living_world
