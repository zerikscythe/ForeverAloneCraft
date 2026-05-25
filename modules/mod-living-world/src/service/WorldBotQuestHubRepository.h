#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace service
{

struct WorldBotQuestHubTaskArea
{
    std::string   taskAreaId;
    std::string   kind;
    std::uint16_t mapId = 0;
    float         x = 0.0f;
    float         y = 0.0f;
    float         z = 0.0f;
    float         radius = 0.0f;
    std::uint32_t weight = 1;
    std::vector<std::uint32_t> targetEntries;
};

struct WorldBotQuestHubBranch
{
    std::string   hubId;
    std::uint32_t zoneId = 0;
    std::string   zoneName;
    std::uint16_t mapId = 0;
    float         x = 0.0f;
    float         y = 0.0f;
    float         z = 0.0f;
    std::uint32_t weight = 1;
};

struct WorldBotQuestHub
{
    std::string                        hubId;
    std::uint32_t                      zoneId = 0;
    std::string                        zoneName;
    std::uint16_t                      mapId = 0;
    float                              x = 0.0f;
    float                              y = 0.0f;
    float                              z = 0.0f;
    std::uint8_t                       requiredFaction = 0; // 0 both, 1 alliance, 2 horde
    std::uint8_t                       minLevel = 1;
    std::uint8_t                       maxLevel = 80;
    std::uint8_t                       avgLevel = 1;
    std::uint32_t                      totalQuests = 0;
    std::uint32_t                      estimatedMinutes = 10;
    std::vector<WorldBotQuestHubTaskArea> taskAreas;
    std::vector<WorldBotQuestHubBranch>   nextHubs;
};

class WorldBotQuestHubRepository
{
public:
    explicit WorldBotQuestHubRepository(std::filesystem::path exportRoot);

    [[nodiscard]] std::optional<WorldBotQuestHub> FindHubById(std::string const& hubId) const;
    [[nodiscard]] std::vector<WorldBotQuestHub> LoadEligibleHubsForZone(
        std::uint32_t zoneId,
        std::uint8_t faction,
        std::uint8_t level) const;
    [[nodiscard]] std::vector<WorldBotQuestHub> LoadEligibleHubs(
        std::uint8_t faction,
        std::uint8_t level) const;

private:
    std::filesystem::path _exportRoot;

    [[nodiscard]] std::vector<WorldBotQuestHub> LoadAll() const;
};

} // namespace service
} // namespace living_world
