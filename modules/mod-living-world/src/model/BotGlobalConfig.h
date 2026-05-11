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

    // Combat positioning / movement thresholds.
    // These influence how ranged/healer bots decide when to re-follow, close in,
    // back away, and reset retreat behavior.
    float           combatFollowOverrideDistance = 20.0f;
    float           repositionDistance           = 8.0f;
    float           rangedMinDistance            = 8.0f;
    float           rangedOptimalDistance        = 25.0f;
    float           rangedCastRange              = 30.0f;
    float           rangedRetreatDistance        = 5.0f;
    float           rangedRetreatTriggerPct      = 80.0f;
    float           rangedRetreatResetPct        = 60.0f;

    // Assist / guard targeting policy.
    bool            assistUseCurrentVictim            = true;
    bool            assistUseOwnerVictim              = true;
    bool            assistOwnerVictimMustTargetOwner  = true;
    bool            attackLockUseOwnerVictim          = true;
    bool            attackLockUseOwnerSelection       = true;
    bool            guardUseCurrentVictim             = true;
    bool            guardUseOwnerAttackers            = true;

    // Assist / command target validity strictness.
    // Safety checks like null/alive/self/map/friendly are always enforced.
    // These booleans only control whether the candidate must also pass the
    // current targetable-for-attack gate before the bot accepts it.
    bool            assistRequireTargetableForAttack  = true;
    bool            commandRequireTargetableForAttack = false;
};

} // namespace model
} // namespace living_world
