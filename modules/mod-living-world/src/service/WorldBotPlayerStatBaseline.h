#pragma once

#include "Player.h"

#include <array>
#include <cstdint>

namespace living_world
{
namespace service
{
struct WorldBotPlayerStatBaseline
{
    std::array<std::uint32_t, MAX_STATS> stats{};
    std::uint32_t baseHealth = 0;
    std::uint32_t baseMana = 0;
    std::uint32_t baseArmor = 0;
};

inline WorldBotPlayerStatBaseline BuildWorldBotPlayerStatBaseline(
    PlayerClassLevelInfo const& classInfo,
    PlayerLevelInfo const& levelInfo)
{
    WorldBotPlayerStatBaseline baseline;
    baseline.baseHealth = classInfo.basehealth;
    baseline.baseMana = classInfo.basemana;

    for (std::size_t i = 0; i < baseline.stats.size() && i < levelInfo.stats.size(); ++i)
        baseline.stats[i] = levelInfo.stats[i];

    baseline.baseArmor = baseline.stats[STAT_AGILITY] * 2u;
    return baseline;
}
} // namespace service
} // namespace living_world