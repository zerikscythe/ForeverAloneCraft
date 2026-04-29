#pragma once

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterSkillSyncRepository
{
public:
    virtual ~CharacterSkillSyncRepository() = default;

    // Merges clone skill values into the source: for each skill the clone has,
    // insert it if the source does not (new skill learned) or update the source
    // value to whichever is higher (additive — skills never go down during play).
    // Returns true if the write completed without error.
    virtual bool SyncSkillsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) = 0;
};
} // namespace integration
} // namespace living_world
