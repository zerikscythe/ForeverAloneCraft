#pragma once

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterSpellSyncRepository
{
public:
    virtual ~CharacterSpellSyncRepository() = default;

    // Copies spells learned by the clone that the source does not yet have.
    // INSERT IGNORE so existing source spells are never overwritten.
    // Returns true if the write completed without error.
    virtual bool SyncSpellsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) = 0;
};
} // namespace integration
} // namespace living_world
