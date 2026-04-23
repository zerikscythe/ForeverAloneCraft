#include "service/CharacterItemSnapshotClassifier.h"

namespace living_world
{
namespace service
{
std::optional<model::CharacterItemStorageDomain>
CharacterItemSnapshotClassifier::RootDomainForSlot(
    std::uint8_t slot) const
{
    if (slot <= 18)
    {
        return model::CharacterItemStorageDomain::Equipment;
    }

    if ((slot >= 19 && slot <= 38) || (slot >= 86 && slot <= 149))
    {
        return model::CharacterItemStorageDomain::Inventory;
    }

    if (slot >= 39 && slot <= 73)
    {
        return model::CharacterItemStorageDomain::Bank;
    }

    return std::nullopt;
}

model::CharacterItemStorageDomain
CharacterItemSnapshotClassifier::NestedDomainForContainer(
    model::CharacterItemStorageDomain containerDomain) const
{
    switch (containerDomain)
    {
        case model::CharacterItemStorageDomain::Equipment:
        case model::CharacterItemStorageDomain::Inventory:
            return model::CharacterItemStorageDomain::Inventory;
        case model::CharacterItemStorageDomain::Bank:
            return model::CharacterItemStorageDomain::Bank;
        case model::CharacterItemStorageDomain::Other:
            return model::CharacterItemStorageDomain::Other;
    }

    return model::CharacterItemStorageDomain::Other;
}
} // namespace service
} // namespace living_world
