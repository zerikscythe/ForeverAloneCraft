#pragma once

#include "SharedDefines.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace living_world
{
namespace service
{
struct WorldBotDefensiveCombatBaseline
{
    float dodgeChance = 5.0f;
    float parryChance = 0.0f;
    float blockChance = 0.0f;
    bool canParry = false;
    bool canBlock = false;
};

inline WorldBotDefensiveCombatBaseline BuildWorldBotDefensiveCombatBaseline(
    std::uint8_t classId,
    std::int32_t agility,
    float dodgeRatio,
    bool hasShield)
{
    constexpr float DodgeBase[MAX_CLASSES] =
    {
        0.036640f,      // Warrior
        0.034943f,      // Paladin
        -0.040873f,     // Hunter
        0.020957f,      // Rogue
        0.034178f,      // Priest
        0.036640f,      // Death Knight
        0.021080f,      // Shaman
        0.036587f,      // Mage
        0.024211f,      // Warlock
        0.0f,           // Unused
        0.056097f       // Druid
    };

    constexpr float CritToDodge[MAX_CLASSES] =
    {
        0.85f / 1.15f,  // Warrior
        1.00f / 1.15f,  // Paladin
        1.11f / 1.15f,  // Hunter
        2.00f / 1.15f,  // Rogue
        1.00f / 1.15f,  // Priest
        0.85f / 1.15f,  // Death Knight
        1.60f / 1.15f,  // Shaman
        1.00f / 1.15f,  // Mage
        0.97f / 1.15f,  // Warlock
        0.0f,           // Unused
        2.00f / 1.15f   // Druid
    };

    WorldBotDefensiveCombatBaseline baseline;
    if (classId == 0 || classId > MAX_CLASSES)
        return baseline;

    std::size_t const classIndex = classId - 1;
    std::int32_t const clampedAgility = std::max<std::int32_t>(agility, 0);
    if (dodgeRatio > 0.0f)
    {
        baseline.dodgeChance = 100.0f * (
            DodgeBase[classIndex]
            + static_cast<float>(clampedAgility) * dodgeRatio * CritToDodge[classIndex]);
    }

    baseline.canParry =
        classId == CLASS_WARRIOR
        || classId == CLASS_PALADIN
        || classId == CLASS_HUNTER
        || classId == CLASS_ROGUE
        || classId == CLASS_DEATH_KNIGHT
        || classId == CLASS_SHAMAN;

    baseline.canBlock = hasShield
        && (classId == CLASS_WARRIOR
            || classId == CLASS_PALADIN
            || classId == CLASS_SHAMAN);

    baseline.parryChance = baseline.canParry ? 5.0f : 0.0f;
    baseline.blockChance = baseline.canBlock ? 5.0f : 0.0f;
    baseline.dodgeChance = std::max(baseline.dodgeChance, 0.0f);
    return baseline;
}

inline std::int32_t AdjustWorldBotDefensiveChance(
    std::int32_t existingChanceBasisPoints,
    float playerLikeChancePct,
    float creatureBaseChancePct = 5.0f)
{
    std::int32_t const adjustedChance = existingChanceBasisPoints
        + static_cast<std::int32_t>(std::lround((playerLikeChancePct - creatureBaseChancePct) * 100.0f));
    return std::max(adjustedChance, 0);
}
} // namespace service
} // namespace living_world