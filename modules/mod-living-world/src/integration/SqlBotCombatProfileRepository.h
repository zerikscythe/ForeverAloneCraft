#pragma once

#include "integration/BotCombatProfileRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotCombatProfileRepository final
    : public BotCombatProfileRepository
{
public:
    std::vector<model::BotCombatProfileRecord> ListProfilesForCharacter(
        std::uint32_t ownerAccountId,
        std::uint64_t sourceCharacterGuid) const override;

    std::optional<model::BotCombatProfileRecord> FindProfileForCharacterSlot(
        std::uint32_t ownerAccountId,
        std::uint64_t sourceCharacterGuid,
        std::uint8_t slot) const override;

    void SaveProfile(
        model::BotCombatProfileRecord const& profile) override;

    void DeleteProfile(
        std::uint64_t profileId) override;
};
} // namespace integration
} // namespace living_world
