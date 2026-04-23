#include "service/CharacterItemSanityChecker.h"

#include <array>
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

    if (result.failures.empty() &&
        EquipmentDiffers(sourceSnapshot, cloneSnapshot))
    {
        result.safeDomains.push_back(model::AccountAltSyncDomain::Equipment);
    }

    result.passed = result.failures.empty();
    return result;
}
} // namespace service
} // namespace living_world
