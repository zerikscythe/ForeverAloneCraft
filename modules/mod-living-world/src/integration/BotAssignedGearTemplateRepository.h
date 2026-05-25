#pragma once

#include "model/WorldBotAssignedGear.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
class BotAssignedGearTemplateRepository
{
public:
    virtual ~BotAssignedGearTemplateRepository() = default;

    virtual std::vector<model::WorldBotAssignedGearEntry> LoadEndgameStageTemplate(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey,
        std::uint8_t endgameStage,
        std::uint8_t raceId) const = 0;
};
} // namespace integration
} // namespace living_world
