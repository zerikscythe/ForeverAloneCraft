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

inline float ApplyWorldBotDiminishingAvoidance(
    float nondiminishingChancePct,
    float diminishingChancePct,
    float capPct,
    float classK)
{
    if (diminishingChancePct <= 0.0f || capPct <= 0.0f || classK <= 0.0f)
        return std::max(nondiminishingChancePct, 0.0f);

    return std::max(
        nondiminishingChancePct
            + diminishingChancePct * capPct / (diminishingChancePct + capPct * classK),
        0.0f);
}

inline float BuildWorldBotDefenseRatingAvoidancePct(float defenseSkillRatingBonus)
{
    return static_cast<float>(static_cast<std::int32_t>(defenseSkillRatingBonus)) * 0.04f;
}

inline WorldBotDefensiveCombatBaseline BuildWorldBotDefensiveCombatBaseline(
    std::uint8_t classId,
    std::int32_t agility,
    float dodgeRatio,
    bool hasShield,
    float defenseSkillRatingBonus = 0.0f,
    float dodgeRatingBonus = 0.0f,
    float parryRatingBonus = 0.0f,
    float blockRatingBonus = 0.0f)
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

    constexpr float DiminishingK[MAX_CLASSES] =
    {
        0.9560f,  // Warrior
        0.9560f,  // Paladin
        0.9880f,  // Hunter
        0.9880f,  // Rogue
        0.9830f,  // Priest
        0.9560f,  // Death Knight
        0.9880f,  // Shaman
        0.9830f,  // Mage
        0.9830f,  // Warlock
        0.0f,     // Unused
        0.9720f   // Druid
    };

    constexpr float DodgeCap[MAX_CLASSES] =
    {
        88.129021f,   // Warrior
        88.129021f,   // Paladin
        145.560408f,  // Hunter
        145.560408f,  // Rogue
        150.375940f,  // Priest
        88.129021f,   // Death Knight
        145.560408f,  // Shaman
        150.375940f,  // Mage
        150.375940f,  // Warlock
        0.0f,         // Unused
        116.890707f   // Druid
    };

    constexpr float ParryCap[MAX_CLASSES] =
    {
        47.003525f,   // Warrior
        47.003525f,   // Paladin
        145.560408f,  // Hunter
        145.560408f,  // Rogue
        0.0f,         // Priest
        47.003525f,   // Death Knight
        145.560408f,  // Shaman
        0.0f,         // Mage
        0.0f,         // Warlock
        0.0f,         // Unused
        0.0f          // Druid
    };

    WorldBotDefensiveCombatBaseline baseline;
    if (classId == 0 || classId > MAX_CLASSES)
        return baseline;

    std::size_t const classIndex = classId - 1;
    std::int32_t const clampedAgility = std::max<std::int32_t>(agility, 0);
    float const defenseAvoidance = BuildWorldBotDefenseRatingAvoidancePct(defenseSkillRatingBonus);
    if (dodgeRatio > 0.0f)
    {
        float const agilityDodge = 100.0f * (
            DodgeBase[classIndex]
            + static_cast<float>(clampedAgility) * dodgeRatio * CritToDodge[classIndex]);
        baseline.dodgeChance = ApplyWorldBotDiminishingAvoidance(
            agilityDodge,
            defenseAvoidance + dodgeRatingBonus,
            DodgeCap[classIndex],
            DiminishingK[classIndex]);
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

    baseline.parryChance = baseline.canParry
        ? ApplyWorldBotDiminishingAvoidance(
            5.0f,
            defenseAvoidance + parryRatingBonus,
            ParryCap[classIndex],
            DiminishingK[classIndex])
        : 0.0f;
    baseline.blockChance = baseline.canBlock
        ? std::max(5.0f + defenseAvoidance + blockRatingBonus, 0.0f)
        : 0.0f;
    baseline.dodgeChance = std::max(baseline.dodgeChance, 0.0f);
    return baseline;
}

inline float BuildWorldBotDefenseMissChanceBonusPct(
    std::uint8_t classId,
    float defenseSkillRatingBonus)
{
    constexpr float MissCap[MAX_CLASSES] =
    {
        16.0f,  // Warrior
        16.0f,  // Paladin
        16.0f,  // Hunter
        16.0f,  // Rogue
        16.0f,  // Priest
        16.0f,  // Death Knight
        16.0f,  // Shaman
        16.0f,  // Mage
        16.0f,  // Warlock
        0.0f,   // Unused
        16.0f   // Druid
    };

    constexpr float DiminishingK[MAX_CLASSES] =
    {
        0.9560f,  // Warrior
        0.9560f,  // Paladin
        0.9880f,  // Hunter
        0.9880f,  // Rogue
        0.9830f,  // Priest
        0.9560f,  // Death Knight
        0.9880f,  // Shaman
        0.9830f,  // Mage
        0.9830f,  // Warlock
        0.0f,     // Unused
        0.9720f   // Druid
    };

    if (classId == 0 || classId > MAX_CLASSES)
        return 0.0f;

    std::size_t const classIndex = classId - 1;
    return ApplyWorldBotDiminishingAvoidance(
        0.0f,
        BuildWorldBotDefenseRatingAvoidancePct(defenseSkillRatingBonus),
        MissCap[classIndex],
        DiminishingK[classIndex]);
}

inline float BuildWorldBotDefenseCritSuppressionPct(float defenseSkillRatingBonus)
{
    return BuildWorldBotDefenseRatingAvoidancePct(defenseSkillRatingBonus);
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
