#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{
// Controls whether companion/account bots should use the older strict assist
// path or the smarter autonomous target-selection policy when no explicit
// player override is active.
enum class BotCombatControlMode : std::uint8_t
{
    Strict, // Obedient assist: owner target / explicit commands drive combat.
    Smart,  // Allow doctrine-aware target arbitration when not explicitly commanded.
};
} // namespace model
} // namespace living_world
