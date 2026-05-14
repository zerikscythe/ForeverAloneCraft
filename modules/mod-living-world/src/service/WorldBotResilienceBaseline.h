#pragma once

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline std::int32_t CalculateWorldBotResilienceDamageReduction(
    std::int32_t damage,
    float resiliencePct,
    float rate,
    float capPct)
{
    if (damage <= 0 || resiliencePct <= 0.0f)
        return 0;

    float const percent = std::min(resiliencePct * rate, capPct);
    return static_cast<std::int32_t>(static_cast<float>(damage) * percent / 100.0f);
}

inline void ApplyWorldBotResilience(
    float resiliencePct,
    float* crit,
    std::int32_t* damage,
    bool isCrit)
{
    if (resiliencePct <= 0.0f)
        return;

    if (crit)
        *crit -= resiliencePct;

    if (!damage || *damage <= 0)
        return;

    if (isCrit)
        *damage -= CalculateWorldBotResilienceDamageReduction(*damage, resiliencePct, 2.2f, 33.0f);

    *damage -= CalculateWorldBotResilienceDamageReduction(*damage, resiliencePct, 2.0f, 100.0f);
    *damage = std::max(*damage, 0);
}
} // namespace service
} // namespace living_world
