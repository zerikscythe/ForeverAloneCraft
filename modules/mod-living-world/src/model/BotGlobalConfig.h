#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{

// Formation style used by IssueFormationFollow.
enum class FollowFormation : uint8_t
{
    Ring    = 0,  // Evenly distributed around the full circle (current default)
    V       = 1,  // Spread behind the owner in a V shape
    Line    = 2,  // Single-file staggered left/right directly behind
    Cluster = 3,  // All bots at the same angle, bunched behind
};

// Global bot behaviour settings loaded from living_world_bot_global_config.
struct BotGlobalConfig
{
    // Follow distances — role-resolved at movement time.
    // Fallback is used when the role cannot be determined (e.g. Passive mode).
    float           followDistanceFallback = 2.0f;
    float           followDistanceMelee   = 1.0f;
    float           followDistanceHealer  = 1.5f;
    float           followDistanceRanged  = 2.5f;

    FollowFormation followFormation    = FollowFormation::Ring;
    uint32_t        followSlotCount    = 7;
    bool            mountWithOwner     = true;
    bool            autoLoot           = false;
};

} // namespace model
} // namespace living_world
