#pragma once

#include <cstdint>
#include <vector>

namespace living_world
{
namespace model
{
enum class CharacterItemStorageDomain
{
    Equipment,
    Inventory,
    Bank,
    Other
};

struct CharacterItemSnapshotEntry
{
    std::uint64_t itemGuid = 0;
    std::uint32_t itemEntry = 0;
    std::uint32_t itemCount = 0;
    std::uint64_t containerGuid = 0;
    std::uint8_t slot = 0;
    CharacterItemStorageDomain domain =
        CharacterItemStorageDomain::Other;
};

struct CharacterItemSnapshot
{
    std::vector<CharacterItemSnapshotEntry> equipmentItems;
    std::vector<CharacterItemSnapshotEntry> inventoryItems;
    std::vector<CharacterItemSnapshotEntry> bankItems;
    std::vector<CharacterItemSnapshotEntry> otherItems;
};
} // namespace model
} // namespace living_world
