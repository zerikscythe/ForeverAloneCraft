#include "planner/SimpleZonePopulationPlanner.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace living_world
{
namespace planner
{
namespace
{
model::BotAbstractState const* FindState(
    std::unordered_map<std::uint64_t, model::BotAbstractState const*> const& stateByBotId,
    std::uint64_t botId)
{
    auto const itr = stateByBotId.find(botId);
    if (itr == stateByBotId.end())
    {
        return nullptr;
    }

    return itr->second;
}

bool IsZoneUnlocked(
    std::uint32_t zoneId,
    model::ProgressionPhaseState const& phaseState)
{
    return phaseState.unlockedZoneIds.empty() ||
        phaseState.unlockedZoneIds.find(zoneId) !=
            phaseState.unlockedZoneIds.end();
}

bool IsActivityAllowed(
    model::BotActivity activity,
    model::BotSpawnContext const& context)
{
    switch (activity)
    {
        case model::BotActivity::City:
            return context.allowCitySpawns;
        case model::BotActivity::Traveling:
        case model::BotActivity::Outdoor:
        case model::BotActivity::PartySupport:
        case model::BotActivity::RivalPatrol:
            return context.allowOutdoorSpawns;
        case model::BotActivity::Abstract:
        case model::BotActivity::Recovering:
            return false;
    }

    return false;
}

bool IsZoneCandidate(
    std::uint32_t zoneId,
    std::uint32_t playerZoneId,
    std::unordered_set<std::uint32_t> const& nearbyZoneIds)
{
    return zoneId == playerZoneId ||
        nearbyZoneIds.find(zoneId) != nearbyZoneIds.end();
}

bool HasEligibleZonePreference(
    model::BotProfile const& profile,
    std::uint32_t playerZoneId,
    std::unordered_set<std::uint32_t> const& nearbyZoneIds,
    model::ProgressionPhaseState const& phaseState)
{
    if (profile.preferredZoneIds.empty())
    {
        return true;
    }

    for (std::uint32_t const zoneId : profile.preferredZoneIds)
    {
        if (!IsZoneUnlocked(zoneId, phaseState))
        {
            continue;
        }

        if (IsZoneCandidate(zoneId, playerZoneId, nearbyZoneIds))
        {
            return true;
        }
    }

    return false;
}

bool IsEligibleForPlanning(
    model::BotProfile const& profile,
    std::unordered_map<std::uint64_t, model::BotAbstractState const*> const& stateByBotId,
    model::BotSpawnContext const& context,
    std::unordered_set<std::uint32_t> const& nearbyZoneIds,
    model::ProgressionPhaseState const& phaseState,
    ProgressionGateResolver const& gateResolver)
{
    if (!gateResolver.IsBotAllowedInContext(profile, context, phaseState))
    {
        return false;
    }

    model::BotAbstractState const* state = FindState(stateByBotId, profile.botId);
    if (!state)
    {
        return false;
    }

    if (state->isSpawned || !state->isAlive)
    {
        return false;
    }

    if (state->respawnCooldownSecondsRemaining > 0 ||
        state->relocationTimerSecondsRemaining > 0 ||
        state->encounterCooldownSecondsRemaining > 0)
    {
        return false;
    }

    if (!IsActivityAllowed(state->activity, context))
    {
        return false;
    }

    return HasEligibleZonePreference(
        profile,
        context.playerZoneId,
        nearbyZoneIds,
        phaseState);
}

float ComputePlannerScore(
    PlannedSpawn const& spawn,
    std::unordered_map<std::uint64_t, model::BotProfile const*> const& profileByBotId,
    std::unordered_map<std::uint64_t, model::BotAbstractState const*> const& stateByBotId,
    std::uint32_t playerZoneId,
    std::unordered_set<std::uint32_t> const& nearbyZoneIds)
{
    float score = spawn.score;

    auto const profileItr = profileByBotId.find(spawn.botId);
    auto const stateItr = stateByBotId.find(spawn.botId);
    if (profileItr == profileByBotId.end() || stateItr == stateByBotId.end())
    {
        return score;
    }

    model::BotProfile const& profile = *profileItr->second;
    model::BotAbstractState const& state = *stateItr->second;

    for (std::uint32_t const zoneId : profile.preferredZoneIds)
    {
        if (zoneId == playerZoneId)
        {
            score += 40.0f;
            break;
        }

        if (nearbyZoneIds.find(zoneId) != nearbyZoneIds.end())
        {
            score += 15.0f;
            break;
        }
    }

    switch (state.activity)
    {
        case model::BotActivity::City:
            score += state.cityPreference * 20.0f;
            break;
        case model::BotActivity::Traveling:
            score += std::max(state.cityPreference, state.outdoorPreference) * 10.0f;
            break;
        case model::BotActivity::Outdoor:
        case model::BotActivity::PartySupport:
        case model::BotActivity::RivalPatrol:
            score += state.outdoorPreference * 20.0f;
            break;
        case model::BotActivity::Abstract:
        case model::BotActivity::Recovering:
            break;
    }

    switch (profile.personality)
    {
        case model::BotPersonality::Indifferent:
            break;
        case model::BotPersonality::Cautious:
            score += 2.0f;
            break;
        case model::BotPersonality::Aggressive:
            score += 4.0f;
            break;
    }

    return score;
}
} // namespace

SimpleZonePopulationPlanner::SimpleZonePopulationPlanner(SpawnSelector const& spawnSelector, ProgressionGateResolver const& gateResolver)
    : _spawnSelector(spawnSelector), _gateResolver(gateResolver)
{
}

ZonePopulationPlan SimpleZonePopulationPlanner::BuildPlan(
    std::vector<model::BotProfile> const& profiles,
    std::vector<model::BotAbstractState> const& states,
    model::BotSpawnContext const& context,
    model::ProgressionPhaseState const& phaseState) const
{
    ZonePopulationPlan plan;

    std::uint32_t const budget = std::min(context.localSpawnBudget, context.visibleBotLimit);
    if (budget == 0 || !IsZoneUnlocked(context.playerZoneId, phaseState))
    {
        return plan;
    }

    std::unordered_map<std::uint64_t, model::BotAbstractState const*> stateByBotId;
    stateByBotId.reserve(states.size());
    for (model::BotAbstractState const& state : states)
    {
        stateByBotId[state.botId] = &state;
    }

    std::unordered_map<std::uint64_t, model::BotProfile const*> profileByBotId;
    profileByBotId.reserve(profiles.size());
    for (model::BotProfile const& profile : profiles)
    {
        profileByBotId[profile.botId] = &profile;
    }

    std::unordered_set<std::uint32_t> nearbyZoneIds(
        context.nearbyZoneIds.begin(),
        context.nearbyZoneIds.end());

    std::vector<model::BotProfile> eligibleProfiles;
    eligibleProfiles.reserve(profiles.size());

    for (model::BotProfile const& profile : profiles)
    {
        if (IsEligibleForPlanning(
            profile,
            stateByBotId,
            context,
            nearbyZoneIds,
            phaseState,
            _gateResolver))
        {
            eligibleProfiles.push_back(profile);
        }
    }

    std::vector<PlannedSpawn> selected = _spawnSelector.SelectSpawns(eligibleProfiles, states, context, phaseState);
    for (PlannedSpawn& spawn : selected)
    {
        spawn.score = ComputePlannerScore(
            spawn,
            profileByBotId,
            stateByBotId,
            context.playerZoneId,
            nearbyZoneIds);
    }

    std::sort(selected.begin(), selected.end(),
        [](PlannedSpawn const& left, PlannedSpawn const& right)
        {
            if (left.score == right.score)
            {
                return left.botId < right.botId;
            }

            return left.score > right.score;
        });

    if (selected.size() > budget)
    {
        selected.resize(budget);
    }

    plan.reservedBudget = static_cast<std::uint32_t>(selected.size());
    plan.spawns = std::move(selected);
    return plan;
}
} // namespace planner
} // namespace living_world
