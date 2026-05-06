#pragma once

#include "ObjectGuid.h"
#include "Position.h"

#include <chrono>
#include <cstdint>

class Player;

namespace living_world
{
namespace ai
{
namespace BotHazardSensor
{

// ---------------------------------------------------------------
// Public data types
// ---------------------------------------------------------------

struct BotHazardState
{
    bool         IsInDanger      = false;
    uint32_t     HazardSpellId   = 0;   // 0 = damage-pattern (no specific aura)
    Position     HazardCenter;
    float        EstimatedRadius  = 0.0f;
    float        Severity         = 0.0f;
};

struct BotHazardMovementState
{
    bool         IsEscapingHazard    = false;
    ObjectGuid   SafeAnchorGuid;
    Position     EscapeTarget;
    std::chrono::steady_clock::time_point NextHazardDecisionTime = {};
    std::chrono::steady_clock::time_point EscapeStartedTime      = {};
};

// ---------------------------------------------------------------
// Public API
// ---------------------------------------------------------------

// Main entry. Call once per 500ms tick, after Hold/Passive early-returns
// and before GetCombatDoctrine(). Returns true when the hazard system
// handled the tick; the caller must return immediately.
bool ProcessHazardTick(Player* bot, Player* owner);

// Release per-bot tracking memory. Call from ClearBotOverride() on dismiss.
void ClearHazardState(ObjectGuid botGuid);

} // namespace BotHazardSensor
} // namespace ai
} // namespace living_world
