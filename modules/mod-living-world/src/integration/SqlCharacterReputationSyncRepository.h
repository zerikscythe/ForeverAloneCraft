#pragma once

#include "integration/CharacterReputationSyncRepository.h"

namespace living_world
{
namespace integration
{
class SqlCharacterReputationSyncRepository final
    : public CharacterReputationSyncRepository
{
public:
    bool SyncReputationFromCloneToSource(
        std::uint64_t sourceCharacterGuid,
        std::uint64_t cloneCharacterGuid) override;
};
} // namespace integration
} // namespace living_world
