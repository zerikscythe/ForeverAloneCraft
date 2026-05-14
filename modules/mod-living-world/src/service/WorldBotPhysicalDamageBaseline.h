#pragma once

#include <cstdint>

namespace living_world
{
namespace service
{
struct WorldBotPhysicalDamageBaseline
{
    float mainHandMinDamage = 0.0f;
    float mainHandMaxDamage = 0.0f;
    float offHandMinDamage = 0.0f;
    float offHandMaxDamage = 0.0f;
    float rangedMinDamage = 0.0f;
    float rangedMaxDamage = 0.0f;
};

inline WorldBotPhysicalDamageBaseline BuildWorldBotPhysicalDamageBaseline(
    std::uint32_t mainHandAttackTimeMs,
    std::uint32_t offHandAttackTimeMs,
    std::uint32_t rangedAttackTimeMs)
{
    constexpr float PlayerBaseMinDamage = 1.0f;
    constexpr float PlayerBaseMaxDamage = 2.0f;

    auto const resolveAttackSpeedSeconds = [](std::uint32_t attackTimeMs)
    {
        return attackTimeMs > 0
            ? static_cast<float>(attackTimeMs) / 1000.0f
            : 1.0f;
    };

    WorldBotPhysicalDamageBaseline baseline;

    float const mainHandAttackSpeedSeconds = resolveAttackSpeedSeconds(mainHandAttackTimeMs);
    float const offHandAttackSpeedSeconds = resolveAttackSpeedSeconds(offHandAttackTimeMs);
    float const rangedAttackSpeedSeconds = resolveAttackSpeedSeconds(rangedAttackTimeMs);

    baseline.mainHandMinDamage = PlayerBaseMinDamage / mainHandAttackSpeedSeconds;
    baseline.mainHandMaxDamage = PlayerBaseMaxDamage / mainHandAttackSpeedSeconds;
    baseline.offHandMinDamage = PlayerBaseMinDamage / offHandAttackSpeedSeconds;
    baseline.offHandMaxDamage = PlayerBaseMaxDamage / offHandAttackSpeedSeconds;
    baseline.rangedMinDamage = PlayerBaseMinDamage / rangedAttackSpeedSeconds;
    baseline.rangedMaxDamage = PlayerBaseMaxDamage / rangedAttackSpeedSeconds;
    return baseline;
}
} // namespace service
} // namespace living_world