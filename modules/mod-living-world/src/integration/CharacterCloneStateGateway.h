#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace living_world
{
namespace integration
{
struct CharacterCloneLoginState
{
    std::string name;
    std::uint16_t atLoginFlags = 0;
    bool loginNameValid = false;
};

class CharacterCloneStateGateway
{
public:
    virtual ~CharacterCloneStateGateway() = default;

    virtual std::optional<CharacterCloneLoginState> LoadCloneLoginState(
        std::uint64_t cloneCharacterGuid) const = 0;

    virtual bool DeleteOfflineCloneCharacter(
        std::uint32_t accountId,
        std::uint64_t cloneCharacterGuid,
        std::string const& cloneCharacterName) const = 0;
};
} // namespace integration
} // namespace living_world