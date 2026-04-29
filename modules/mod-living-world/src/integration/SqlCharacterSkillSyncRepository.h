#pragma once

#include "integration/CharacterSkillSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterSkillSyncRepository final
    : public CharacterSkillSyncRepository
{
public:
    bool SyncSkillsFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) override;
};
} // namespace integration
} // namespace living_world
