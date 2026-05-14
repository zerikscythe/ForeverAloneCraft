#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace living_world
{
namespace service
{
inline float BuildWorldBotSpiritManaRegenPerSecond(
    float intellect,
    float spirit,
    float mpPerSpiritRatio,
    float powerRegenMultiplier)
{
    intellect = std::max(intellect, 0.0f);
    spirit = std::max(spirit, 0.0f);
    return std::sqrt(intellect) * spirit * mpPerSpiritRatio * powerRegenMultiplier;
}

inline float BuildWorldBotManaRegenPerTick(
    float spiritRegenPerSecond,
    float mp5RegenPerSecond,
    float interruptedRegenPct,
    bool underLastManaUseEffect,
    float powerRate,
    std::uint32_t intervalMs)
{
    interruptedRegenPct = std::clamp(interruptedRegenPct, 0.0f, 100.0f);
    float const spiritContribution = underLastManaUseEffect
        ? spiritRegenPerSecond * interruptedRegenPct / 100.0f
        : spiritRegenPerSecond;
    return (mp5RegenPerSecond + spiritContribution)
        * powerRate
        * static_cast<float>(intervalMs)
        / 1000.0f;
}
} // namespace service
} // namespace living_world
