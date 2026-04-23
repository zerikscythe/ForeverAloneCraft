#include "integration/SqlCharacterItemSnapshotRepository.h"

#include "DatabaseEnv.h"
#include "service/CharacterItemSnapshotClassifier.h"

#include <unordered_map>

namespace living_world
{
namespace integration
{
namespace
{
void AppendEntry(
    model::CharacterItemSnapshot& snapshot,
    model::CharacterItemSnapshotEntry entry)
{
    switch (entry.domain)
    {
        case model::CharacterItemStorageDomain::Equipment:
            snapshot.equipmentItems.push_back(entry);
            break;
        case model::CharacterItemStorageDomain::Inventory:
            snapshot.inventoryItems.push_back(entry);
            break;
        case model::CharacterItemStorageDomain::Bank:
            snapshot.bankItems.push_back(entry);
            break;
        case model::CharacterItemStorageDomain::Other:
            snapshot.otherItems.push_back(entry);
            break;
    }
}
} // namespace

std::optional<model::CharacterItemSnapshot>
SqlCharacterItemSnapshotRepository::LoadSnapshot(
    std::uint64_t characterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT ci.bag, ci.slot, ci.item, ii.itemEntry, ii.count "
        "FROM character_inventory ci "
        "JOIN item_instance ii ON ci.item = ii.guid "
        "WHERE ci.guid = {} "
        "ORDER BY ci.bag ASC, ci.slot ASC",
        characterGuid);
    if (!result)
    {
        return std::nullopt;
    }

    model::CharacterItemSnapshot snapshot;
    service::CharacterItemSnapshotClassifier classifier;
    std::unordered_map<std::uint64_t, model::CharacterItemStorageDomain>
        containerDomains;

    do
    {
        Field const* fields = result->Fetch();
        std::uint64_t const containerGuid = fields[0].Get<std::uint64_t>();
        std::uint8_t const slot = fields[1].Get<std::uint8_t>();

        model::CharacterItemSnapshotEntry entry;
        entry.containerGuid = containerGuid;
        entry.slot = slot;
        entry.itemGuid = fields[2].Get<std::uint64_t>();
        entry.itemEntry = fields[3].Get<std::uint32_t>();
        entry.itemCount = fields[4].Get<std::uint32_t>();

        if (containerGuid == 0)
        {
            std::optional<model::CharacterItemStorageDomain> rootDomain =
                classifier.RootDomainForSlot(slot);
            entry.domain = rootDomain.value_or(
                model::CharacterItemStorageDomain::Other);
        }
        else
        {
            auto const containerItr = containerDomains.find(containerGuid);
            if (containerItr == containerDomains.end())
            {
                entry.domain = model::CharacterItemStorageDomain::Other;
            }
            else
            {
                entry.domain = classifier.NestedDomainForContainer(
                    containerItr->second);
            }
        }

        containerDomains[entry.itemGuid] = entry.domain;
        AppendEntry(snapshot, entry);
    } while (result->NextRow());

    return snapshot;
}
} // namespace integration
} // namespace living_world
