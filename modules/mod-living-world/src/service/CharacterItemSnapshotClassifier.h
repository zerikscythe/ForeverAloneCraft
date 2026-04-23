#pragma once

#include "model/CharacterItemSnapshot.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace service
{
class CharacterItemSnapshotClassifier
{
public:
    std::optional<model::CharacterItemStorageDomain> RootDomainForSlot(
        std::uint8_t slot) const;

    model::CharacterItemStorageDomain NestedDomainForContainer(
        model::CharacterItemStorageDomain containerDomain) const;
};
} // namespace service
} // namespace living_world
