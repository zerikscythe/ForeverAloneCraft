#pragma once

#include "model/AmbientBotTypes.h"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
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
    TaxiFlight = 7,  // Timed fake-flight between authored taxi nodes
};

struct AmbientStep
{
    AmbientStepType type        = AmbientStepType::Idle;
    std::uint16_t   mapId       = 0;
    float           x           = 0.f;
    float           y           = 0.f;
    float           z           = 0.f;
    std::uint32_t   durationSec = 600;
    std::int32_t    taskIndex   = -1; // index into AmbientSession::tasks; -1 = session-level/no task
    std::string     subjectKind;
    std::uint32_t   subjectId   = 0;
    std::string     subjectKey;
    std::string     returnAnchorRole;
    std::uint8_t    cycleCount  = 1;
    std::string     label;
};

struct AmbientSessionTask
{
    std::uint32_t activityId   = 0;
    std::string   activityKey;
    std::string   displayName;
    std::string   activityType;
    std::string   taskFamily;
    std::uint32_t targetZoneId = 0;
};

struct AmbientSession
{
    // Legacy primary activity identity kept for compatibility with older callers.
    // For chained sessions, tasks[] is the authoritative per-task metadata list.
    std::uint32_t                  activityId  = 0;
    std::string                    activityKey;
    std::string                    displayName;
    std::string                    sourceKind;   // legacy_activity|task_template|playlist
    std::string                    sourceKey;    // activity key / template key / playlist key
    std::vector<AmbientSessionTask> tasks;
    std::vector<AmbientStep>       steps;    // each task contributes Travel -> Activity
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
        bool hasFishing,
        std::uint32_t startZoneId = 0,
        std::uint32_t homeZoneId = 0,
        std::string const& homeAnchorPointKey = "",
        std::string const& homeBindPointKey = "",
        std::unordered_set<std::uint32_t> const* exploredZoneIds = nullptr) const;
};

} // namespace service
} // namespace living_world
