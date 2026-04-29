#pragma once

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterReputationSyncRepository
{
public:
    virtual ~CharacterReputationSyncRepository() = default;

    // Merges clone reputation into source: for each faction, source standing is
    // updated to the higher of its current value and the clone's value. New
    // factions present only on the clone are inserted for the source character.
    // Returns true if the write completed without error.
    virtual bool SyncReputationFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) = 0;
};
} // namespace integration
} // namespace living_world
