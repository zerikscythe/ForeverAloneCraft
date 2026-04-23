#pragma once

#include "model/CharacterItemSnapshot.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace integration
{
class CharacterItemSnapshotRepository
{
public:
    virtual ~CharacterItemSnapshotRepository() = default;

    virtual std::optional<model::CharacterItemSnapshot> LoadSnapshot(
        std::uint64_t characterGuid) const = 0;
};
} // namespace integration
} // namespace living_world
