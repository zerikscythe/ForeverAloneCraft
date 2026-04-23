#pragma once

#include "model/AccountAltRuntime.h"

#include <cstdint>

namespace living_world
{
namespace integration
{
// Write-only surface for syncing a progress snapshot back to a character row.
// All methods must be callable from the world thread.
class CharacterProgressSyncRepository
{
public:
    virtual ~CharacterProgressSyncRepository() = default;

    // Overwrites level, XP, and money on the target character row.
    // Returns true if the write was issued without error.
    virtual bool SyncProgressToCharacter(
        std::uint64_t characterGuid,
        model::CharacterProgressSnapshot const& snapshot) = 0;
};
} // namespace integration
} // namespace living_world
