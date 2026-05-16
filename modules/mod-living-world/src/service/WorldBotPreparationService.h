#pragma once

#include "integration/SqlBotIdentityRepository.h"
#include "model/WorldBotPreparedBuild.h"

#include <cstdint>
#include <string>
#include <unordered_set>

namespace living_world
{
namespace integration
{
class BotCombatDefaultProfileRepository;
class BotTalentTemplateRepository;
class BotVirtualLoadoutRepository;
} // namespace integration

namespace service
{
class WorldBotPreparationService
{
public:
    WorldBotPreparationService(
        integration::BotCombatDefaultProfileRepository const& defaultProfileRepository,
        integration::BotTalentTemplateRepository const& talentTemplateRepository,
        integration::BotVirtualLoadoutRepository const& virtualLoadoutRepository);

    [[nodiscard]] model::WorldBotPreparedBuild Prepare(
        integration::BotIdentityRecord const& identity,
        std::string const& contextKey = "PvE") const;

    [[nodiscard]] static std::string ResolveRoleKey(
        std::uint8_t classId,
        std::string const& specKey);

    [[nodiscard]] static std::uint8_t ComputeAvailableTalentPoints(std::uint8_t level);

    [[nodiscard]] static std::unordered_set<std::uint32_t> CollectTravelMobilitySpellIds(
        integration::BotIdentityRecord const& identity);

private:
    integration::BotCombatDefaultProfileRepository const& _defaultProfileRepository;
    integration::BotTalentTemplateRepository const& _talentTemplateRepository;
    integration::BotVirtualLoadoutRepository const& _virtualLoadoutRepository;
};
} // namespace service
} // namespace living_world
