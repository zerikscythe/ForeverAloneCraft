#pragma once

#include "integration/CharacterCloneStateGateway.h"

namespace living_world
{
namespace integration
{
class AzerothCharacterCloneStateGateway final
    : public CharacterCloneStateGateway
{
public:
    std::optional<CharacterCloneLoginState> LoadCloneLoginState(
        std::uint64_t cloneCharacterGuid) const override;

    bool DeleteOfflineCloneCharacter(
        std::uint32_t accountId,
        std::uint64_t cloneCharacterGuid,
        std::string const& cloneCharacterName) const override;
};
} // namespace integration
} // namespace living_world