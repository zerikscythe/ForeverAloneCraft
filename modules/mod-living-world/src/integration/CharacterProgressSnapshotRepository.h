#pragma once

#include "model/AccountAltRuntime.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace integration
{
class CharacterProgressSnapshotRepository
{
public:
    virtual ~CharacterProgressSnapshotRepository() = default;

    virtual std::optional<model::CharacterProgressSnapshot> LoadSnapshot(
        std::uint64_t characterGuid) const = 0;
};
} // namespace integration
} // namespace living_world
