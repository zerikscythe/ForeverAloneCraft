#pragma once

#include "model/BotTalentTemplate.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace integration
{
class BotTalentPreferenceRepository
{
public:
    virtual ~BotTalentPreferenceRepository() = default;

    virtual std::optional<model::BotTalentPreference> GetPreference(
        std::uint64_t sourceCharGuid) const = 0;

    virtual void SavePreference(model::BotTalentPreference const& pref) = 0;

    virtual void ClearPreference(std::uint64_t sourceCharGuid) = 0;
};
} // namespace integration
} // namespace living_world
