#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{
// Controls how CompanionAI behaves each tick. Set by the owner via
// `.lwbot <bot> mode <mode>` and stored in BotPlayerRegistry keyed by
// owner GUID. Defaults to Assist when not explicitly set.
enum class BotCombatMode : std::uint8_t
{
    Assist,  // follow and engage owner's target (default)
    Passive, // follow and buff, never engage
    Hold,    // stop and stay put, never engage
    Guard,   // engage anything currently attacking the owner
};
} // namespace model
} // namespace living_world
