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
std::size_t WeightedPickIndex(
    std::vector<model::ActivityEntry> const& pool,
    std::mt19937& rng)
{
    std::uint32_t const totalWeight = std::accumulate(
        pool.begin(), pool.end(), std::uint32_t{0},
        [](std::uint32_t sum, model::ActivityEntry const& e)
        { return sum + e.weight; });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (std::size_t i = 0; i < pool.size(); ++i)
    {
        if (roll <= pool[i].weight)
            return i;
        roll -= pool[i].weight;
    }

    return pool.empty() ? 0u : (pool.size() - 1u);
}

AmbientStepType ActivityTypeToStepType(std::string const& activityType)
{
    if (activityType == "gather_herb") return AmbientStepType::GatherHerb;
    if (activityType == "gather_ore")  return AmbientStepType::GatherOre;
    if (activityType == "fish")        return AmbientStepType::Fish;
    if (activityType == "patrol")      return AmbientStepType::Patrol;
    return AmbientStepType::Idle;
}

// First realignment slice:
// build a short chained session from the existing activity table rather than
// selecting exactly one activity. This preserves the current schema/API while
// moving Tier 2 behavior toward the intended "day in the life" session model.
std::uint32_t ChooseTaskCount(std::size_t eligibleCount, std::mt19937& rng)
{
    if (eligibleCount == 0)
        return 0;

    if (eligibleCount == 1)
        return 1;

    if (eligibleCount == 2)
        return 2;

    std::uint32_t const maxTasks = static_cast<std::uint32_t>(
        std::min<std::size_t>(eligibleCount, 5));
    std::uniform_int_distribution<std::uint32_t> countDist(3, maxTasks);
    return countDist(rng);
}

bool MatchesRequiredZoneType(
    model::ActivityEntry const& activity,
    model::ZoneEntry const& zone)
{
    return activity.requiredZoneType.empty()
        || activity.requiredZoneType == "any"
        || activity.requiredZoneType == zone.zoneType;
}

bool MatchesZoneLevelBand(
    std::uint8_t level,
    model::ZoneEntry const& zone)
{
    std::int32_t const minAllowed = std::max<std::int32_t>(
        1, static_cast<std::int32_t>(level) - 5);
    std::int32_t const maxAllowed = std::min<std::int32_t>(
        80, static_cast<std::int32_t>(level) + 5);

    return static_cast<std::int32_t>(zone.maxLevel) >= minAllowed
        && static_cast<std::int32_t>(zone.minLevel) <= maxAllowed;
}

std::string NormalizeTaskFamily(std::string const& taskFamily)
{
    return taskFamily.empty() ? "misc" : taskFamily;
}

std::uint32_t CountSelectedFamily(
    std::vector<model::ActivityEntry> const& selected,
    std::string const& taskFamily)
{
    std::string const normalized = NormalizeTaskFamily(taskFamily);
    return static_cast<std::uint32_t>(std::count_if(
        selected.begin(), selected.end(),
        [&](model::ActivityEntry const& entry)
        {
            return NormalizeTaskFamily(entry.taskFamily) == normalized;
        }));
}

std::uint32_t CountSelectedZone(
    std::vector<model::ActivityEntry> const& selected,
    std::uint32_t zoneId)
{
    return static_cast<std::uint32_t>(std::count_if(
        selected.begin(), selected.end(),
        [&](model::ActivityEntry const& entry)
        {
            return entry.targetZoneId == zoneId;
        }));
}

bool MatchesFamilyRepeatRules(
    std::vector<model::ActivityEntry> const& selected,
    model::ActivityEntry const& candidate)
{
    std::uint32_t const familyCount = CountSelectedFamily(
        selected, candidate.taskFamily);
    std::uint32_t const familyCap = std::max<std::uint32_t>(
        1u, candidate.maxPerSession);

    return familyCount < familyCap;
}

bool MatchesZoneRepeatRules(
    std::vector<model::ActivityEntry> const& selected,
    model::ActivityEntry const& candidate)
{
    return CountSelectedZone(selected, candidate.targetZoneId) < 2u;
}

bool MatchesTaskFamilyChainRules(
    std::vector<model::ActivityEntry> const& selected,
    model::ActivityEntry const& candidate)
{
    if (selected.empty())
        return true;

    model::ActivityEntry const& previous = selected.back();
    std::string const previousFamily = NormalizeTaskFamily(previous.taskFamily);
    std::string const nextFamily = NormalizeTaskFamily(candidate.taskFamily);

    if (previousFamily != nextFamily)
        return true;

    // Back-to-back city errands are believable; most other same-family repeats
    // feel like accidental loops rather than a varied session chain.
    return nextFamily == "city_errand";
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
    auto eligible = actRepo.LoadEligible(
        faction, level, hasHerbalism, hasMining, hasFishing);

    if (eligible.empty())
        return std::nullopt;

    integration::SqlZoneIndexRepository zoneRepo;

    std::vector<model::ActivityEntry> filtered;
    filtered.reserve(eligible.size());
    for (model::ActivityEntry const& candidate : eligible)
    {
        auto const zone = zoneRepo.Find(candidate.targetZoneId);
        if (!zone)
            continue;

        if (!MatchesRequiredZoneType(candidate, *zone))
            continue;

        if (!MatchesZoneLevelBand(level, *zone))
            continue;

        filtered.push_back(candidate);
    }

    if (filtered.empty())
        return std::nullopt;

    // Seed with time so each call is different.
    std::mt19937 rng(static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    std::uint32_t const requestedTaskCount = ChooseTaskCount(filtered.size(), rng);
    if (requestedTaskCount == 0)
        return std::nullopt;

    std::vector<model::ActivityEntry> selected;
    selected.reserve(requestedTaskCount);

    // Weighted selection without replacement, but filtered step-by-step so
    // the session forms a believable chain instead of a random bag of tasks.
    for (std::uint32_t i = 0; i < requestedTaskCount && !filtered.empty(); ++i)
    {
        std::vector<model::ActivityEntry> chainCandidates;
        std::vector<std::size_t> chainCandidateIndices;

        for (std::size_t candidateIndex = 0; candidateIndex < filtered.size(); ++candidateIndex)
        {
            model::ActivityEntry const& candidate = filtered[candidateIndex];

            if (!MatchesFamilyRepeatRules(selected, candidate))
                continue;

            if (!MatchesZoneRepeatRules(selected, candidate))
                continue;

            if (!MatchesTaskFamilyChainRules(selected, candidate))
                continue;

            chainCandidates.push_back(candidate);
            chainCandidateIndices.push_back(candidateIndex);
        }

        // Relax the chain-specific rule if it would otherwise prevent a session
        // from being composed at all; still honor family/zone repetition caps.
        if (chainCandidates.empty())
        {
            for (std::size_t candidateIndex = 0; candidateIndex < filtered.size(); ++candidateIndex)
            {
                model::ActivityEntry const& candidate = filtered[candidateIndex];

                if (!MatchesFamilyRepeatRules(selected, candidate))
                    continue;

                if (!MatchesZoneRepeatRules(selected, candidate))
                    continue;

                chainCandidates.push_back(candidate);
                chainCandidateIndices.push_back(candidateIndex);
            }
        }

        if (chainCandidates.empty())
            break;

        std::size_t const pickedSubsetIndex = WeightedPickIndex(chainCandidates, rng);
        std::size_t const pickedIndex = chainCandidateIndices[pickedSubsetIndex];
        selected.push_back(filtered[pickedIndex]);
        filtered.erase(filtered.begin() + static_cast<std::ptrdiff_t>(pickedIndex));
    }

    if (selected.empty())
        return std::nullopt;

    AmbientSession session;

    for (model::ActivityEntry const& picked : selected)
    {
        auto const zone = zoneRepo.Find(picked.targetZoneId);
        if (!zone)
            continue;

        std::uniform_int_distribution<std::uint32_t> durDist(
            picked.durationMinSec, picked.durationMaxSec);
        std::uint32_t const duration = durDist(rng);

        AmbientSessionTask task;
        task.activityId   = picked.activityId;
        task.activityKey  = picked.activityKey;
        task.displayName  = picked.displayName;
        task.activityType = picked.activityType;
        task.taskFamily   = picked.taskFamily;
        task.targetZoneId = picked.targetZoneId;

        std::int32_t const taskIndex = static_cast<std::int32_t>(session.tasks.size());
        session.tasks.push_back(std::move(task));

        // Always TRAVEL before each activity in the chain. This keeps the
        // destination-first rule while making inter-task travel explicit.
        AmbientStep travelStep;
        travelStep.type        = AmbientStepType::Travel;
        travelStep.mapId       = zone->mapId;
        travelStep.x           = zone->anchorX;
        travelStep.y           = zone->anchorY;
        travelStep.z           = zone->anchorZ;
        travelStep.durationSec = 0;
        travelStep.taskIndex   = taskIndex;
        travelStep.label       = "Travel to " + zone->zoneName;
        session.steps.push_back(travelStep);

        AmbientStep actStep;
        actStep.type        = ActivityTypeToStepType(picked.activityType);
        actStep.mapId       = zone->mapId;
        actStep.x           = zone->anchorX;
        actStep.y           = zone->anchorY;
        actStep.z           = zone->anchorZ;
        actStep.durationSec = duration;
        actStep.taskIndex   = taskIndex;
        actStep.label       = picked.displayName;
        session.steps.push_back(actStep);
    }

    if (session.tasks.empty() || session.steps.empty())
        return std::nullopt;

    session.activityId = session.tasks.front().activityId;
    session.activityKey = session.tasks.size() > 1
        ? "multi_activity_session"
        : session.tasks.front().activityKey;
    session.displayName = session.tasks.size() > 1
        ? "Chained session (" + std::to_string(session.tasks.size()) + " tasks)"
        : session.tasks.front().displayName;

    return session;
}

} // namespace service
} // namespace living_world