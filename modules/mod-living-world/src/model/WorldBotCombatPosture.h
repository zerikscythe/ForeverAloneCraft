#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{
enum class WorldBotCombatPosture : std::uint8_t
{
    Hold = 0,
    Close = 1,
    Reposition = 2,
    Kite = 3,
    Retreat = 4,
    HazardEscape = 5,
};

enum class WorldBotMovementDecisionSource : std::uint8_t
{
    None = 0,
    PostureDoctrine = 1,
    HazardOverride = 2,
    EmergencyFallback = 3,
};

enum class WorldBotMovementStyle : std::uint8_t
{
    Unknown = 0,
    FrontlineTank = 1,
    StickyMelee = 2,
    TurretCaster = 3,
    MobileRanged = 4,
    BacklineHealer = 5,
};
} // namespace model
} // namespace living_world