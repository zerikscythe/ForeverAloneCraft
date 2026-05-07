#pragma once

#include "model/AmbientBotTypes.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace service
{

enum class AmbientStepType : std::uint8_t
{
    Travel     = 1,  // MovePoint to anchor coords
    GatherHerb = 2,  // Simulate herb gathering (Phase 3: logged only; Phase 4: real interaction)
    GatherOre  = 3,  // Simulate ore mining
    Fish       = 4,  // Simulate fishing
    Idle       = 5,  // Stand at location for duration
    Patrol     = 6,  // Walk between anchor and nearby waypoints
};

struct AmbientStep
{
    AmbientStepType type        = AmbientStepType::Idle;
    std::uint16_t   mapId       = 0;
    float           x           = 0.f;
    float           y           = 0.f;
    float           z           = 0.f;
    std::uint32_t   durationSec = 600;
    std::string     label;
};

struct AmbientSession
{
    std::uint32_t           activityId  = 0;
    std::string             activityKey;
    std::string             displayName;
    std::vector<AmbientStep> steps;    // always begins with a Travel step
};

// Builds a destination-first activity session for a bot.
// Returns nullopt when no eligible activity exists.
class BotActivitySessionComposer
{
public:
    std::optional<AmbientSession> Compose(
        std::uint8_t faction,
        std::uint8_t level,
        bool hasHerbalism,
        bool hasMining,
        bool hasFishing) const;
};

} // namespace service
} // namespace living_world
