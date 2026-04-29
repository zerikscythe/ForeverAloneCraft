#pragma once

#include <cstdint>

namespace living_world
{
namespace integration
{
class CharacterQuestSyncRepository
{
public:
    virtual ~CharacterQuestSyncRepository() = default;

    // Copies all completed (rewarded) quests from the clone to the source.
    // Uses INSERT IGNORE so source quests already present are untouched.
    // Returns true if the write completed without error.
    virtual bool SyncQuestsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) = 0;
};
} // namespace integration
} // namespace living_world
