#pragma once

#include "model/BotCombatProfile.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace living_world
{
namespace integration
{
// Aggregate repository contract for player-editable bot combat profiles.
// The service/API layer remains responsible for validating row edits and for
// deciding when blank profiles should fall back to baked-in defaults.
class BotCombatProfileRepository
{
public:
    virtual ~BotCombatProfileRepository() = default;

    virtual std::vector<model::BotCombatProfileRecord> ListProfilesForCharacter(
        std::uint32_t ownerAccountId,
        std::uint64_t sourceCharacterGuid) const = 0;

    virtual std::optional<model::BotCombatProfileRecord> FindProfileForCharacterSlot(
        std::uint32_t ownerAccountId,
        std::uint64_t sourceCharacterGuid,
        std::uint8_t slot) const = 0;

    // Persists the profile aggregate root and any attached entry/action/
    // condition rows transactionally.
    virtual void SaveProfile(
        model::BotCombatProfileRecord const& profile) = 0;

    virtual void DeleteProfile(
        std::uint64_t profileId) = 0;
};
} // namespace integration
} // namespace living_world
