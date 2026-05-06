#pragma once

#include "integration/BotTalentPreferenceRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotTalentPreferenceRepository final : public BotTalentPreferenceRepository
{
public:
    void EnsureSchema() const;

    std::optional<model::BotTalentPreference> GetPreference(
        std::uint64_t sourceCharGuid) const override;

    void SavePreference(model::BotTalentPreference const& pref) override;

    void ClearPreference(std::uint64_t sourceCharGuid) override;
};
} // namespace integration
} // namespace living_world
