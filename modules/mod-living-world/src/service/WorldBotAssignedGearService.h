#pragma once

#include "integration/BotAssignedGearTemplateRepository.h"
#include "integration/BotAssignedGearRepository.h"
#include "integration/SqlBotIdentityRepository.h"
#include "model/WorldBotAssignedGear.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace service
{
struct WorldBotAssignedGearResult
{
    std::vector<model::WorldBotAssignedGearEntry> entries;
    model::WorldBotAssignedGearSummary summary;
    std::uint8_t refreshBand = 0;
    bool refreshed = false;
};

class WorldBotAssignedGearService
{
public:
    explicit WorldBotAssignedGearService(
        integration::BotAssignedGearRepository const& assignedGearRepository,
        integration::BotAssignedGearTemplateRepository const& assignedGearTemplateRepository);

    [[nodiscard]] WorldBotAssignedGearResult EnsureAssignedGear(
        integration::BotIdentityRecord& identity,
        std::string const& canonicalSpecKey,
        std::string const& roleKey) const;

    [[nodiscard]] static model::WorldBotAssignedGearSummary SummarizeAssignedGear(
        std::vector<model::WorldBotAssignedGearEntry> const& entries);

private:
    integration::BotAssignedGearRepository const& _assignedGearRepository;
    integration::BotAssignedGearTemplateRepository const& _assignedGearTemplateRepository;
};
} // namespace service
} // namespace living_world
