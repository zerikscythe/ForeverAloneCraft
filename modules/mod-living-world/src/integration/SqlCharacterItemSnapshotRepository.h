#pragma once

#include "integration/CharacterItemSnapshotRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterItemSnapshotRepository final
    : public CharacterItemSnapshotRepository
{
public:
    std::optional<model::CharacterItemSnapshot> LoadSnapshot(
        std::uint64_t characterGuid) const override;
};
} // namespace integration
} // namespace living_world
