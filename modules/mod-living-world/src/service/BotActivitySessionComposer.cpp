#include "service/BotActivitySessionComposer.h"
#include "integration/SqlActivityLibraryRepository.h"
#include "integration/SqlZoneIndexRepository.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>

namespace living_world
{
namespace service
{
namespace
{
// Weighted random selection — picks an activity proportional to its weight.
model::ActivityEntry const* WeightedPick(
    std::vector<model::ActivityEntry> const& pool,
    std::mt19937& rng)
{
    if (pool.empty())
        return nullptr;

    std::uint32_t const totalWeight = std::accumulate(
        pool.begin(), pool.end(), std::uint32_t{0},
        [](std::uint32_t sum, model::ActivityEntry const& e)
        { return sum + e.weight; });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (model::ActivityEntry const& e : pool)
    {
        if (roll <= e.weight)
            return &e;
        roll -= e.weight;
    }
    return &pool.back();
}

AmbientStepType ActivityTypeToStepType(std::string const& activityType)
{
    if (activityType == "gather_herb") return AmbientStepType::GatherHerb;
    if (activityType == "gather_ore")  return AmbientStepType::GatherOre;
    if (activityType == "fish")        return AmbientStepType::Fish;
    if (activityType == "patrol")      return AmbientStepType::Patrol;
    return AmbientStepType::Idle;
}
} // namespace

std::optional<AmbientSession> BotActivitySessionComposer::Compose(
    std::uint8_t faction,
    std::uint8_t level,
    bool hasHerbalism,
    bool hasMining,
    bool hasFishing) const
{
    integration::SqlActivityLibraryRepository actRepo;
    auto const eligible = actRepo.LoadEligible(
        faction, level, hasHerbalism, hasMining, hasFishing);

    if (eligible.empty())
        return std::nullopt;

    // Seed with time so each call is different.
    std::mt19937 rng(static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    model::ActivityEntry const* picked = WeightedPick(eligible, rng);
    if (!picked)
        return std::nullopt;

    integration::SqlZoneIndexRepository zoneRepo;
    auto const zone = zoneRepo.Find(picked->targetZoneId);
    if (!zone)
        return std::nullopt;

    // Random duration within the activity's min/max range.
    std::uniform_int_distribution<std::uint32_t> durDist(
        picked->durationMinSec, picked->durationMaxSec);
    std::uint32_t const duration = durDist(rng);

    AmbientSession session;
    session.activityId  = picked->activityId;
    session.activityKey = picked->activityKey;
    session.displayName = picked->displayName;

    // Step 1 — always TRAVEL to destination (destination-first rule).
    AmbientStep travelStep;
    travelStep.type        = AmbientStepType::Travel;
    travelStep.mapId       = zone->mapId;
    travelStep.x           = zone->anchorX;
    travelStep.y           = zone->anchorY;
    travelStep.z           = zone->anchorZ;
    travelStep.durationSec = 0; // travel completes on arrival, not by timer
    travelStep.label       = "Travel to " + zone->zoneName;
    session.steps.push_back(travelStep);

    // Step 2 — the activity itself.
    AmbientStep actStep;
    actStep.type        = ActivityTypeToStepType(picked->activityType);
    actStep.mapId       = zone->mapId;
    actStep.x           = zone->anchorX;
    actStep.y           = zone->anchorY;
    actStep.z           = zone->anchorZ;
    actStep.durationSec = duration;
    actStep.label       = picked->displayName;
    session.steps.push_back(actStep);

    return session;
}

} // namespace service
} // namespace living_world
