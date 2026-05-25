#pragma once

#include "integration/BotAssignedGearTemplateRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotAssignedGearTemplateRepository final : public BotAssignedGearTemplateRepository
{
public:
    void EnsureSchema() const;

    std::vector<model::WorldBotAssignedGearEntry> LoadEndgameStageTemplate(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey,
        std::uint8_t endgameStage,
        std::uint8_t raceId) const override;
};
} // namespace integration
} // namespace living_world
