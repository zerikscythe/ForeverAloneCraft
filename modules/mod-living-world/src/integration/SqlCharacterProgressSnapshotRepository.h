#pragma once

#include "integration/CharacterProgressSnapshotRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterProgressSnapshotRepository final
    : public CharacterProgressSnapshotRepository
{
public:
    std::optional<model::CharacterProgressSnapshot> LoadSnapshot(
        std::uint64_t characterGuid) const override;
};
} // namespace integration
} // namespace living_world
