#include "service/BotActivitySessionComposer.h"
#include "service/AmbientTaskEligibility.h"
#include "service/WorldBotQuestHubRepository.h"
#include "service/WorldBotTaxiPlanning.h"
#include "Config.h"
#include "integration/SqlActivityLibraryRepository.h"
#include "integration/SqlTaskPointRepository.h"
#include "integration/SqlTaskTemplateRepository.h"
#include "integration/SqlZoneIndexRepository.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <random>

namespace living_world
{
namespace service
{
namespace
{
std::string EffectiveResolverKind(model::TaskTemplateStepEntry const& step)
{
    if (!step.resolverKind.empty())
        return step.resolverKind;

    return step.targetPointKey.empty() ? "zone" : "point";
}

std::string EffectiveSubjectKind(model::TaskTemplateStepEntry const& step)
{
    if (!step.subjectKind.empty())
        return step.subjectKind;

    if (step.stepType == "gather_herb") return "herb";
    if (step.stepType == "gather_ore")  return "ore";
    if (step.stepType == "fish")        return "fish";
    if (step.stepType == "idle_city" || step.stepType == "idle_inn") return "city_service";
    return "";
}

std::string NormalizeLower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsQuestingTaskFamily(std::string const& taskFamily)
{
    std::string const normalized = NormalizeLower(taskFamily);
    return normalized == "questing" || normalized == "quest";
}

std::filesystem::path ResolveWorldBotQuestHubExportRoot()
{
    return std::filesystem::path(sConfigMgr->GetOption<std::string>(
        "LivingWorld.QuestHubExportDir",
        "data/quest_hubs"));
}

WorldBotQuestHubRepository const& GetSessionComposerQuestHubRepository()
{
    static WorldBotQuestHubRepository repository(ResolveWorldBotQuestHubExportRoot());
    return repository;
}

bool IsQuestHubSubjectKey(std::string const& subjectKey)
{
    return subjectKey.starts_with("quest_hub:");
}

std::string ExtractQuestHubId(std::string const& subjectKey)
{
    if (!IsQuestHubSubjectKey(subjectKey))
        return {};
    std::string const tail = subjectKey.substr(std::string("quest_hub:").size());
    std::size_t const commaPos = tail.find(',');
    return commaPos == std::string::npos ? tail : tail.substr(0, commaPos);
}

bool IsQuestHubEligibleForLevel(
    std::uint8_t level,
    WorldBotQuestHub const& hub)
{
    std::int32_t const currentLevel = static_cast<std::int32_t>(level);
    std::int32_t const minAllowed = std::max<std::int32_t>(1, static_cast<std::int32_t>(hub.minLevel) - 3);
    std::int32_t const maxAllowed = std::min<std::int32_t>(80, static_cast<std::int32_t>(hub.maxLevel) + 3);
    return currentLevel >= minAllowed && currentLevel <= maxAllowed;
}

std::optional<WorldBotQuestHub> ResolveQuestHubDefinition(
    std::optional<std::uint32_t> zoneFilter,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t currentZoneId,
    std::uint32_t homeZoneId,
    std::string const& excludedHubId = {})
{
    std::vector<WorldBotQuestHub> hubs = zoneFilter.has_value()
        ? GetSessionComposerQuestHubRepository().LoadEligibleHubsForZone(*zoneFilter, faction, level)
        : GetSessionComposerQuestHubRepository().LoadEligibleHubs(faction, level);

    if (!excludedHubId.empty())
    {
        hubs.erase(
            std::remove_if(
                hubs.begin(),
                hubs.end(),
                [&](WorldBotQuestHub const& hub) { return hub.hubId == excludedHubId; }),
            hubs.end());
    }

    if (hubs.empty())
        return std::nullopt;

    std::stable_sort(
        hubs.begin(),
        hubs.end(),
        [&](WorldBotQuestHub const& lhs, WorldBotQuestHub const& rhs)
        {
            bool const lhsCurrentZone = currentZoneId != 0 && lhs.zoneId == currentZoneId;
            bool const rhsCurrentZone = currentZoneId != 0 && rhs.zoneId == currentZoneId;
            if (lhsCurrentZone != rhsCurrentZone)
                return lhsCurrentZone > rhsCurrentZone;

            std::uint32_t const lhsWeight = std::max<std::uint32_t>(1u, lhs.totalQuests);
            std::uint32_t const rhsWeight = std::max<std::uint32_t>(1u, rhs.totalQuests);
            return lhsWeight > rhsWeight;
        });

    std::vector<WorldBotQuestHub> preferred;
    if (currentZoneId != 0)
    {
        for (WorldBotQuestHub const& hub : hubs)
        {
            if (hub.zoneId == currentZoneId)
                preferred.push_back(hub);
        }
    }

    std::vector<WorldBotQuestHub> homePreferred;
    if (!zoneFilter.has_value() && preferred.empty() && level <= 15u && homeZoneId != 0)
    {
        for (WorldBotQuestHub const& hub : hubs)
        {
            if (hub.zoneId == homeZoneId)
                homePreferred.push_back(hub);
        }
    }

    std::vector<WorldBotQuestHub> const& candidatePool =
        !preferred.empty() ? preferred :
        !homePreferred.empty() ? homePreferred :
        hubs;
    if (candidatePool.empty())
        return std::nullopt;

    std::uint32_t const totalWeight = std::accumulate(
        candidatePool.begin(), candidatePool.end(), std::uint32_t{0},
        [&](std::uint32_t sum, WorldBotQuestHub const& hub)
        {
            std::uint32_t const levelDelta = static_cast<std::uint32_t>(
                std::abs(static_cast<int>(hub.avgLevel) - static_cast<int>(level)));
            std::uint32_t const levelBias = std::max<std::uint32_t>(1u, 6u - std::min<std::uint32_t>(5u, levelDelta));
            return sum + (std::max<std::uint32_t>(1u, hub.totalQuests) * levelBias);
        });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (WorldBotQuestHub const& hub : candidatePool)
    {
        std::uint32_t const levelDelta = static_cast<std::uint32_t>(
            std::abs(static_cast<int>(hub.avgLevel) - static_cast<int>(level)));
        std::uint32_t const levelBias = std::max<std::uint32_t>(1u, 6u - std::min<std::uint32_t>(5u, levelDelta));
        std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, hub.totalQuests) * levelBias;
        if (roll <= effectiveWeight)
            return hub;
        roll -= effectiveWeight;
    }

    return candidatePool.back();
}

AmbientSessionResumeHint BuildChainedResumeHint(AmbientSession const& session)
{
    AmbientSessionResumeHint hint;
    hint.lastSessionSourceKind = session.sourceKind;
    hint.lastSessionSourceKey = session.sourceKey;
    if (session.steps.empty())
        return hint;

    std::size_t stepIndex = session.steps.size() - 1u;
    AmbientStep const& activeStep = session.steps[stepIndex];
    hint.lastStepSubjectKind = activeStep.subjectKind;
    hint.lastStepSubjectKey = activeStep.subjectKey;
    if (activeStep.taskIndex >= 0)
    {
        std::size_t const taskIndex = static_cast<std::size_t>(activeStep.taskIndex);
        if (taskIndex < session.tasks.size())
        {
            hint.lastTaskFamily = session.tasks[taskIndex].taskFamily;
            hint.lastTaskTargetZoneId = session.tasks[taskIndex].targetZoneId;
        }
    }

    std::string const activeHubId = ExtractQuestHubId(activeStep.subjectKey);
    if (activeHubId.empty())
        return hint;

    hint.lastQuestHubElapsedMs = static_cast<std::uint64_t>(activeStep.durationSec) * 1000ull;
    while (stepIndex > 0u)
    {
        std::size_t const previousIndex = stepIndex - 1u;
        AmbientStep const& previousStep = session.steps[previousIndex];
        if (ExtractQuestHubId(previousStep.subjectKey) != activeHubId)
            break;

        hint.lastQuestHubElapsedMs += static_cast<std::uint64_t>(previousStep.durationSec) * 1000ull;
        stepIndex = previousIndex;
    }

    return hint;
}

WorldBotQuestHubTaskArea const* WeightedPickQuestHubTaskArea(
    std::vector<WorldBotQuestHubTaskArea> const& taskAreas,
    std::mt19937& rng)
{
    if (taskAreas.empty())
        return nullptr;

    std::uint32_t const totalWeight = std::accumulate(
        taskAreas.begin(), taskAreas.end(), std::uint32_t{0},
        [](std::uint32_t sum, WorldBotQuestHubTaskArea const& area)
        {
            return sum + std::max<std::uint32_t>(1u, area.weight);
        });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (WorldBotQuestHubTaskArea const& area : taskAreas)
    {
        std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, area.weight);
        if (roll <= effectiveWeight)
            return &area;
        roll -= effectiveWeight;
    }

    return &taskAreas.back();
}

WorldBotQuestHubBranch const* WeightedPickQuestHubBranch(
    std::vector<WorldBotQuestHubBranch> const& branches,
    std::mt19937& rng)
{
    if (branches.empty())
        return nullptr;

    std::uint32_t const totalWeight = std::accumulate(
        branches.begin(), branches.end(), std::uint32_t{0},
        [](std::uint32_t sum, WorldBotQuestHubBranch const& branch)
        {
            return sum + std::max<std::uint32_t>(1u, branch.weight);
        });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (WorldBotQuestHubBranch const& branch : branches)
    {
        std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, branch.weight);
        if (roll <= effectiveWeight)
            return &branch;
        roll -= effectiveWeight;
    }

    return &branches.back();
}

std::string BuildQuestHubSubjectKey(
    WorldBotQuestHub const& hub,
    WorldBotQuestHubTaskArea const* area)
{
    std::string key = "quest_hub:" + hub.hubId;
    if (!area || area->targetEntries.empty())
        return key;

    for (std::uint32_t entry : area->targetEntries)
    {
        key += ",";
        key += std::to_string(entry);
    }

    return key;
}

std::uint32_t ChooseQuestHubSliceDurationSec(std::uint32_t remainingSec, std::mt19937& rng)
{
    if (remainingSec <= 45u)
        return remainingSec;

    std::uint32_t const minSlice = std::min<std::uint32_t>(45u, remainingSec);
    std::uint32_t const maxSlice = std::min<std::uint32_t>(90u, remainingSec);
    if (minSlice >= maxSlice)
        return maxSlice;

    std::uniform_int_distribution<std::uint32_t> dist(minSlice, maxSlice);
    return dist(rng);
}

bool IsQuestResumeLevelAppropriate(
    std::uint8_t level,
    model::ZoneEntry const& zone)
{
    std::int32_t const minAllowed = std::max<std::int32_t>(1, static_cast<std::int32_t>(zone.minLevel) - 3);
    std::int32_t const maxAllowed = std::min<std::int32_t>(80, static_cast<std::int32_t>(zone.maxLevel) + 3);
    std::int32_t const currentLevel = static_cast<std::int32_t>(level);
    return currentLevel >= minAllowed && currentLevel <= maxAllowed;
}

bool TemplateSupportsQuestResume(
    model::TaskTemplateEntry const& tmpl,
    std::uint32_t resumeZoneId)
{
    for (model::TaskTemplateStepEntry const& step : tmpl.steps)
    {
        std::string const resolverKind = EffectiveResolverKind(step);
        if (resolverKind == "quest_auto")
            return true;
        if (resolverKind == "quest_zone"
            && (step.targetZoneId == 0 || step.targetZoneId == resumeZoneId))
        {
            return true;
        }
    }

    return false;
}

bool MatchesPreferredTaskFamily(
    std::string const& taskFamily,
    AmbientSessionComposeBias const* composeBias)
{
    if (!composeBias || composeBias->preferredTaskFamily.empty())
        return true;

    return NormalizeLower(taskFamily) == NormalizeLower(composeBias->preferredTaskFamily);
}

bool TemplateMatchesComposeBias(
    model::TaskTemplateEntry const& tmpl,
    AmbientSessionComposeBias const* composeBias)
{
    if (!MatchesPreferredTaskFamily(tmpl.taskFamily, composeBias))
        return false;

    if (!composeBias || composeBias->preferredZoneId == 0)
        return true;

    return std::any_of(
        tmpl.steps.begin(),
        tmpl.steps.end(),
        [&](model::TaskTemplateStepEntry const& step)
        {
            return step.targetZoneId == 0 || step.targetZoneId == composeBias->preferredZoneId;
        });
}

bool PlaylistMatchesComposeBias(
    model::PlaylistEntrySet const& playlist,
    AmbientSessionComposeBias const* composeBias,
    std::unordered_set<std::uint32_t> const& allowedTemplateIds)
{
    if (!MatchesPreferredTaskFamily(playlist.taskFamily, composeBias))
        return false;

    if (!composeBias || composeBias->preferredZoneId == 0)
        return true;

    if (playlist.entries.empty())
        return false;

    return std::all_of(
        playlist.entries.begin(),
        playlist.entries.end(),
        [&](model::PlaylistEntryRef const& entry)
        {
            return allowedTemplateIds.find(entry.taskTemplateId) != allowedTemplateIds.end();
        });
}

bool ActivityMatchesComposeBias(
    model::ActivityEntry const& activity,
    AmbientSessionComposeBias const* composeBias)
{
    if (!MatchesPreferredTaskFamily(activity.taskFamily, composeBias))
        return false;

    if (!composeBias || composeBias->preferredZoneId == 0)
        return true;

    return activity.targetZoneId == 0 || activity.targetZoneId == composeBias->preferredZoneId;
}

bool IsWorldPvPTaskFamily(std::string const& taskFamily)
{
    std::string const normalized = NormalizeLower(taskFamily);
    return normalized == "pvp"
        || normalized == "world_pvp"
        || normalized.starts_with("pvp_")
        || normalized.starts_with("world_pvp")
        || normalized.find("_pvp") != std::string::npos;
}

bool SessionPersonalityAllowsTaskFamily(
    std::string const& sessionPersonalityKey,
    std::string const& taskFamily)
{
    if (!IsWorldPvPTaskFamily(taskFamily))
        return true;

    return NormalizeLower(sessionPersonalityKey) == "aggressive";
}

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

std::size_t WeightedPickIndexWithBias(
    std::vector<model::ActivityEntry> const& pool,
    bool openerPick,
    std::mt19937& rng)
{
    std::uint32_t const totalWeight = std::accumulate(
        pool.begin(), pool.end(), std::uint32_t{0},
        [&](std::uint32_t sum, model::ActivityEntry const& e)
        {
            std::uint32_t const bias = openerPick
                ? std::max<std::uint32_t>(1u, e.openerBias)
                : std::max<std::uint32_t>(1u, e.followupBias);
            return sum + (std::max<std::uint32_t>(1u, e.weight) * bias);
        });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (std::size_t i = 0; i < pool.size(); ++i)
    {
        std::uint32_t const bias = openerPick
            ? std::max<std::uint32_t>(1u, pool[i].openerBias)
            : std::max<std::uint32_t>(1u, pool[i].followupBias);
        std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, pool[i].weight) * bias;
        if (roll <= effectiveWeight)
            return i;
        roll -= effectiveWeight;
    }

    return pool.empty() ? 0u : (pool.size() - 1u);
}

AmbientStepType ActivityTypeToStepType(std::string const& activityType)
{
    if (activityType == "gather_herb") return AmbientStepType::GatherHerb;
    if (activityType == "gather_ore")  return AmbientStepType::GatherOre;
    if (activityType == "fish")        return AmbientStepType::Fish;
    if (activityType == "patrol")      return AmbientStepType::Patrol;
    if (activityType == "grind")       return AmbientStepType::Grind;
    return AmbientStepType::Idle;
}

void AppendTravelStep(
    AmbientSession& session,
    std::int32_t taskIndex,
    std::uint16_t mapId,
    float x,
    float y,
    float z,
    std::string const& label)
{
    AmbientStep travelStep;
    travelStep.type        = AmbientStepType::Travel;
    travelStep.mapId       = mapId;
    travelStep.x           = x;
    travelStep.y           = y;
    travelStep.z           = z;
    travelStep.durationSec = 0;
    travelStep.taskIndex   = taskIndex;
    travelStep.label       = label;
    session.steps.push_back(travelStep);
}

void AppendTransitStep(
    AmbientSession& session,
    std::int32_t taskIndex,
    std::uint16_t mapId,
    float x,
    float y,
    float z,
    std::uint32_t durationSec,
    std::string const& label,
    std::string const& routeKey,
    std::string const& sourcePointKey,
    std::string const& destPointKey,
    std::string const& transitType,
    std::string const& sourceLabel,
    std::string const& destLabel)
{
    AmbientStep flightStep;
    flightStep.type        = AmbientStepType::Transit;
    flightStep.mapId       = mapId;
    flightStep.x           = x;
    flightStep.y           = y;
    flightStep.z           = z;
    flightStep.durationSec = durationSec;
    flightStep.taskIndex   = taskIndex;
    flightStep.transitType = transitType;
    flightStep.transitRouteKey = routeKey;
    flightStep.transitSourcePointKey = sourcePointKey;
    flightStep.transitDestPointKey = destPointKey;
    flightStep.transitSourceLabel = sourceLabel;
    flightStep.transitDestLabel = destLabel;
    flightStep.label       = label;
    session.steps.push_back(flightStep);
}

service::WorldBotTaxiNetwork const& GetSessionComposerTaxiNetwork()
{
    static service::WorldBotTaxiNetwork network = service::LoadWorldBotTaxiNetwork();
    return network;
}

bool AppendDynamicTaxiTransit(
    AmbientSession& session,
    std::int32_t taskIndex,
    std::uint8_t faction,
    std::unordered_set<std::uint32_t> const* exploredZoneIds,
    bool currentLocationResolved,
    std::uint16_t currentMapId,
    std::uint32_t currentZoneId,
    float currentX,
    float currentY,
    float currentZ,
    std::uint16_t targetMapId,
    float targetX,
    float targetY,
    float targetZ,
    std::string const& targetLabel)
{
    if (!exploredZoneIds || exploredZoneIds->empty())
        return false;
    if (!currentLocationResolved || currentMapId != targetMapId)
        return false;
    if (currentZoneId == 0)
        return false;

    auto const candidate = GetSessionComposerTaxiNetwork().ResolveTravelCandidate(
        currentMapId,
        currentX,
        currentY,
        currentZ,
        targetMapId,
        targetX,
        targetY,
        targetZ,
        *exploredZoneIds,
        faction);
    if (!candidate.has_value() || candidate->empty())
        return false;

    AppendTravelStep(
        session,
        taskIndex,
        candidate->sourceNode.mapId,
        candidate->sourceNode.x,
        candidate->sourceNode.y,
        candidate->sourceNode.z,
        "Travel to flight master " + candidate->sourceNode.name);
    AppendTransitStep(
        session,
        taskIndex,
        candidate->destinationNode.mapId,
        candidate->destinationNode.x,
        candidate->destinationNode.y,
        candidate->destinationNode.z,
        std::max<std::uint32_t>(15u, (candidate->route.totalEtaMs + 999u) / 1000u),
        "Taxi via " + candidate->sourceNode.name + " -> " + candidate->destinationNode.name,
        "",
        "",
        "",
        "taxi",
        candidate->sourceNode.name,
        candidate->destinationNode.name);

    if (candidate->destinationNode.mapId != targetMapId
        || candidate->destinationNode.x != targetX
        || candidate->destinationNode.y != targetY
        || candidate->destinationNode.z != targetZ)
    {
        AppendTravelStep(
            session,
            taskIndex,
            targetMapId,
            targetX,
            targetY,
            targetZ,
            "Travel to " + targetLabel);
    }

    return true;
}

bool ResolvePointTarget(
    integration::SqlTaskPointRepository& pointRepo,
    std::string const& pointKey,
    std::uint16_t& mapId,
    float& targetX,
    float& targetY,
    float& targetZ,
    std::uint32_t& targetZoneId)
{
    auto const point = pointRepo.FindByKey(pointKey);
    if (!point)
        return false;

    mapId = point->mapId;
    targetX = point->x;
    targetY = point->y;
    targetZ = point->z;
    targetZoneId = point->zoneId;
    return true;
}

struct ResolvedTemplateStepTarget
{
    std::uint16_t mapId = 0;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    std::uint32_t targetZoneId = 0;
    std::string resolvedPointKey;
    std::string resolvedPointType;
    std::uint32_t resolvedSubjectId = 0;
    std::string resolvedSubjectKey;
    std::string resolvedReturnAnchorRole;
    std::string resolvedLabel;
    std::uint32_t durationOverrideSec = 0;
};

model::ZoneContentEntry const* WeightedPickZoneContentEntry(
    std::vector<model::ZoneContentEntry> const& content,
    std::mt19937& rng)
{
    if (content.empty())
        return nullptr;

    std::uint32_t const totalWeight = std::accumulate(
        content.begin(), content.end(), std::uint32_t{0},
        [](std::uint32_t sum, model::ZoneContentEntry const& entry)
        {
            return sum + std::max<std::uint32_t>(1u, entry.weight);
        });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (model::ZoneContentEntry const& entry : content)
    {
        std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, entry.weight);
        if (roll <= effectiveWeight)
            return &entry;
        roll -= effectiveWeight;
    }

    return &content.back();
}

std::vector<model::ZoneContentEntry> PreferAnchoredZoneContent(
    std::vector<model::ZoneContentEntry> const& content)
{
    bool const hasAnchored = std::any_of(
        content.begin(),
        content.end(),
        [](model::ZoneContentEntry const& entry)
        {
            return !entry.anchorPointKey.empty();
        });

    if (!hasAnchored)
        return content;

    std::vector<model::ZoneContentEntry> filtered;
    filtered.reserve(content.size());
    for (model::ZoneContentEntry const& entry : content)
    {
        if (!entry.anchorPointKey.empty())
            filtered.push_back(entry);
    }

    return filtered;
}

bool ResolveZoneTarget(
    integration::SqlZoneIndexRepository& zoneRepo,
    std::uint32_t zoneId,
    std::uint16_t& mapId,
    float& targetX,
    float& targetY,
    float& targetZ,
    std::uint32_t& targetZoneId)
{
    auto const zone = zoneRepo.Find(zoneId);
    if (!zone)
        return false;

    mapId = zone->mapId;
    targetX = zone->anchorX;
    targetY = zone->anchorY;
    targetZ = zone->anchorZ;
    targetZoneId = zone->zoneId;
    return true;
}

bool ResolveQuestHubTarget(
    std::optional<std::uint32_t> zoneFilter,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t currentZoneId,
    std::uint32_t homeZoneId,
    ResolvedTemplateStepTarget& resolved)
{
    std::optional<WorldBotQuestHub> pickedHub = ResolveQuestHubDefinition(
        zoneFilter,
        rng,
        faction,
        level,
        currentZoneId,
        homeZoneId);
    if (!pickedHub)
        return false;

    WorldBotQuestHubTaskArea const* pickedArea = WeightedPickQuestHubTaskArea(pickedHub->taskAreas, rng);
    resolved.mapId = pickedArea ? pickedArea->mapId : pickedHub->mapId;
    resolved.targetX = pickedArea ? pickedArea->x : pickedHub->x;
    resolved.targetY = pickedArea ? pickedArea->y : pickedHub->y;
    resolved.targetZ = pickedArea ? pickedArea->z : pickedHub->z;
    resolved.targetZoneId = pickedHub->zoneId;
    resolved.resolvedSubjectKey = "quest_hub:" + pickedHub->hubId;
    resolved.resolvedLabel = pickedHub->zoneName + " Quest Hub";
    resolved.durationOverrideSec = std::max<std::uint32_t>(300u, pickedHub->estimatedMinutes * 60u);
    return true;
}

std::optional<WorldBotQuestHub> ResolveQuestHubBranchOrFallback(
    WorldBotQuestHub const& previousHub,
    std::optional<std::uint32_t> zoneFilter,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t homeZoneId)
{
    std::vector<WorldBotQuestHubBranch> eligibleBranches;
    eligibleBranches.reserve(previousHub.nextHubs.size());
    for (WorldBotQuestHubBranch const& branch : previousHub.nextHubs)
    {
        auto const candidateHub = GetSessionComposerQuestHubRepository().FindHubById(branch.hubId);
        if (!candidateHub)
            continue;
        if (candidateHub->requiredFaction != 0 && faction != 0 && candidateHub->requiredFaction != faction)
            continue;
        if (!IsQuestHubEligibleForLevel(level, *candidateHub))
            continue;
        eligibleBranches.push_back(branch);
    }

    if (!eligibleBranches.empty())
    {
        if (WorldBotQuestHubBranch const* pickedBranch = WeightedPickQuestHubBranch(eligibleBranches, rng))
            return GetSessionComposerQuestHubRepository().FindHubById(pickedBranch->hubId);
    }

    return ResolveQuestHubDefinition(
        zoneFilter,
        rng,
        faction,
        level,
        previousHub.zoneId,
        homeZoneId,
        previousHub.hubId);
}

bool AppendBudgetedQuestHubSteps(
    AmbientSession& session,
    std::int32_t taskIndex,
    std::string const& displayName,
    std::optional<std::uint32_t> zoneFilter,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t homeZoneId,
    std::uint32_t taskBudgetSec,
    WorldBotQuestHub initialHub,
    std::uint64_t initialHubElapsedMs,
    bool prependInitialTravel,
    std::uint32_t& currentZoneId,
    std::uint16_t& currentMapId,
    float& currentX,
    float& currentY,
    float& currentZ,
    bool& currentLocationResolved)
{
    if (taskBudgetSec == 0)
        return false;

    WorldBotQuestHub currentHub = std::move(initialHub);
    std::uint32_t currentHubElapsedSec = static_cast<std::uint32_t>(initialHubElapsedMs / 1000ull);
    bool addedAnySteps = false;
    std::string lastTaskAreaId;
    bool needTravel = prependInitialTravel;

    while (taskBudgetSec > 0u)
    {
        std::uint32_t const hubBudgetSec = std::max<std::uint32_t>(300u, currentHub.estimatedMinutes * 60u);
        if (currentHubElapsedSec >= hubBudgetSec)
        {
            auto nextHub = ResolveQuestHubBranchOrFallback(
                currentHub,
                zoneFilter,
                rng,
                faction,
                level,
                homeZoneId);
            if (!nextHub)
                break;

            currentHub = *nextHub;
            currentHubElapsedSec = 0u;
            lastTaskAreaId.clear();
            needTravel = true;
        }

        if (needTravel)
        {
            AppendTravelStep(
                session,
                taskIndex,
                currentHub.mapId,
                currentHub.x,
                currentHub.y,
                currentHub.z,
                "Travel to " + currentHub.zoneName + " quest hub");
            addedAnySteps = true;
            needTravel = false;
        }

        std::uint32_t remainingThisHubSec = std::min<std::uint32_t>(
            taskBudgetSec,
            hubBudgetSec - currentHubElapsedSec);

        while (remainingThisHubSec > 0u)
        {
            WorldBotQuestHubTaskArea const* area = WeightedPickQuestHubTaskArea(currentHub.taskAreas, rng);
            if (area && currentHub.taskAreas.size() > 1u && area->taskAreaId == lastTaskAreaId)
            {
                for (WorldBotQuestHubTaskArea const& candidate : currentHub.taskAreas)
                {
                    if (candidate.taskAreaId != lastTaskAreaId)
                    {
                        area = &candidate;
                        break;
                    }
                }
            }

            AmbientStep questStep;
            questStep.type = AmbientStepType::Grind;
            questStep.mapId = area ? area->mapId : currentHub.mapId;
            questStep.x = area ? area->x : currentHub.x;
            questStep.y = area ? area->y : currentHub.y;
            questStep.z = area ? area->z : currentHub.z;
            questStep.durationSec = ChooseQuestHubSliceDurationSec(remainingThisHubSec, rng);
            questStep.taskIndex = taskIndex;
            questStep.subjectKind = "quest";
            questStep.subjectKey = BuildQuestHubSubjectKey(currentHub, area);
            questStep.combatRadius = area ? std::max(25.0f, area->radius) : 45.0f;
            questStep.label = displayName;
            session.steps.push_back(questStep);
            addedAnySteps = true;

            remainingThisHubSec -= questStep.durationSec;
            taskBudgetSec -= questStep.durationSec;
            currentHubElapsedSec += questStep.durationSec;
            currentZoneId = currentHub.zoneId;
            currentMapId = questStep.mapId;
            currentX = questStep.x;
            currentY = questStep.y;
            currentZ = questStep.z;
            currentLocationResolved = true;
            lastTaskAreaId = area ? area->taskAreaId : std::string{};
        }
    }

    return addedAnySteps;
}

bool ResolveTemplateStepTarget(
    model::TaskTemplateStepEntry const& templateStep,
    integration::SqlZoneIndexRepository& zoneRepo,
    integration::SqlTaskPointRepository& pointRepo,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t currentZoneId,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey,
    ResolvedTemplateStepTarget& resolved)
{
    resolved.targetZoneId = templateStep.targetZoneId;
    std::string const resolverKind = EffectiveResolverKind(templateStep);
    std::string const subjectKind = EffectiveSubjectKind(templateStep);

    if (!templateStep.targetPointKey.empty() || resolverKind == "point")
    {
        if (!ResolvePointTarget(
                pointRepo,
                templateStep.targetPointKey,
                resolved.mapId,
                resolved.targetX,
                resolved.targetY,
                resolved.targetZ,
                resolved.targetZoneId))
        {
            return false;
        }

        resolved.resolvedPointKey = templateStep.targetPointKey;
        if (auto const point = pointRepo.FindByKey(templateStep.targetPointKey))
            resolved.resolvedPointType = point->pointType;
        return true;
    }

    if (resolverKind == "zone")
        return ResolveZoneTarget(
            zoneRepo,
            templateStep.targetZoneId,
            resolved.mapId,
            resolved.targetX,
            resolved.targetY,
            resolved.targetZ,
            resolved.targetZoneId);

    if (resolverKind == "home_city")
    {
        if (!homeAnchorPointKey.empty()
            && ResolvePointTarget(
                pointRepo,
                homeAnchorPointKey,
                resolved.mapId,
                resolved.targetX,
                resolved.targetY,
                resolved.targetZ,
                resolved.targetZoneId))
        {
            resolved.resolvedPointKey = homeAnchorPointKey;
            if (auto const point = pointRepo.FindByKey(homeAnchorPointKey))
                resolved.resolvedPointType = point->pointType;
            return true;
        }

        if (!homeBindPointKey.empty()
            && ResolvePointTarget(
                pointRepo,
                homeBindPointKey,
                resolved.mapId,
                resolved.targetX,
                resolved.targetY,
                resolved.targetZ,
                resolved.targetZoneId))
        {
            resolved.resolvedPointKey = homeBindPointKey;
            if (auto const point = pointRepo.FindByKey(homeBindPointKey))
                resolved.resolvedPointType = point->pointType;
            return true;
        }

        if (homeZoneId != 0)
        {
            if (auto const anchor = pointRepo.FindZoneAnchor(homeZoneId, "home", faction, level))
            {
                if (ResolvePointTarget(
                        pointRepo,
                        anchor->pointKey,
                        resolved.mapId,
                        resolved.targetX,
                        resolved.targetY,
                        resolved.targetZ,
                        resolved.targetZoneId))
                {
                    resolved.resolvedPointKey = anchor->pointKey;
                    if (auto const point = pointRepo.FindByKey(anchor->pointKey))
                        resolved.resolvedPointType = point->pointType;
                    return true;
                }
            }

            return ResolveZoneTarget(
                zoneRepo,
                homeZoneId,
                resolved.mapId,
                resolved.targetX,
                resolved.targetY,
                resolved.targetZ,
                resolved.targetZoneId);
        }

        return false;
    }

    if (resolverKind == "quest_zone")
    {
        if (templateStep.targetZoneId == 0)
            return false;

        if (ResolveQuestHubTarget(templateStep.targetZoneId, rng, faction, level, currentZoneId, homeZoneId, resolved))
            return true;

        return ResolveZoneTarget(
            zoneRepo,
            templateStep.targetZoneId,
            resolved.mapId,
            resolved.targetX,
            resolved.targetY,
            resolved.targetZ,
            resolved.targetZoneId);
    }

    if (resolverKind == "quest_auto")
    {
        if (ResolveQuestHubTarget(std::nullopt, rng, faction, level, currentZoneId, homeZoneId, resolved))
            return true;

        return false;
    }

    if (resolverKind == "resource_zone" || resolverKind == "creature_zone")
    {
        if (templateStep.targetZoneId == 0)
            return false;

        if (!subjectKind.empty())
        {
            std::vector<model::ZoneContentEntry> content = pointRepo.LoadZoneContentByZoneAndKind(
                templateStep.targetZoneId,
                subjectKind,
                faction,
                level);
            content = PreferAnchoredZoneContent(content);
            if (model::ZoneContentEntry const* picked = WeightedPickZoneContentEntry(content, rng))
            {
                resolved.resolvedSubjectId = picked->subjectId;
                resolved.resolvedSubjectKey = picked->subjectKey;
                resolved.resolvedReturnAnchorRole = picked->returnAnchorRole;
                resolved.targetZoneId = picked->zoneId;

                if (!picked->anchorPointKey.empty()
                    && ResolvePointTarget(
                        pointRepo,
                        picked->anchorPointKey,
                        resolved.mapId,
                        resolved.targetX,
                        resolved.targetY,
                        resolved.targetZ,
                        resolved.targetZoneId))
                {
                    resolved.resolvedPointKey = picked->anchorPointKey;
                    if (auto const point = pointRepo.FindByKey(picked->anchorPointKey))
                        resolved.resolvedPointType = point->pointType;
                    return true;
                }
            }
        }

        return ResolveZoneTarget(
            zoneRepo,
            templateStep.targetZoneId,
            resolved.mapId,
            resolved.targetX,
            resolved.targetY,
            resolved.targetZ,
            resolved.targetZoneId);
    }

    if (resolverKind == "resource_auto" || resolverKind == "creature_auto")
    {
        if (subjectKind.empty())
            return false;

        std::vector<model::ZoneContentEntry> content = pointRepo.LoadZoneContentByKind(subjectKind, faction, level);
        content = PreferAnchoredZoneContent(content);
        if (content.empty())
            return false;

        model::ZoneContentEntry const* picked = nullptr;
        std::vector<model::ZoneContentEntry> currentZoneContent;
        if (currentZoneId != 0)
        {
            for (model::ZoneContentEntry const& entry : content)
            {
                if (entry.zoneId == currentZoneId)
                    currentZoneContent.push_back(entry);
            }
            currentZoneContent = PreferAnchoredZoneContent(currentZoneContent);
        }

        if (!currentZoneContent.empty())
        {
            picked = WeightedPickZoneContentEntry(currentZoneContent, rng);
        }
        else
        {
            picked = WeightedPickZoneContentEntry(content, rng);
        }

        if (!picked)
            return false;

        resolved.resolvedSubjectId = picked->subjectId;
        resolved.resolvedSubjectKey = picked->subjectKey;
        resolved.resolvedReturnAnchorRole = picked->returnAnchorRole;
        resolved.targetZoneId = picked->zoneId;

        if (!picked->anchorPointKey.empty()
            && ResolvePointTarget(
                pointRepo,
                picked->anchorPointKey,
                resolved.mapId,
                resolved.targetX,
                resolved.targetY,
                resolved.targetZ,
                resolved.targetZoneId))
        {
            resolved.resolvedPointKey = picked->anchorPointKey;
            if (auto const point = pointRepo.FindByKey(picked->anchorPointKey))
                resolved.resolvedPointType = point->pointType;
            return true;
        }

        return ResolveZoneTarget(
            zoneRepo,
            picked->zoneId,
            resolved.mapId,
            resolved.targetX,
            resolved.targetY,
            resolved.targetZ,
            resolved.targetZoneId);
    }

    return ResolveZoneTarget(
        zoneRepo,
        templateStep.targetZoneId,
        resolved.mapId,
        resolved.targetX,
        resolved.targetY,
        resolved.targetZ,
        resolved.targetZoneId);
}

std::optional<AmbientSession> BuildSessionFromTemplate(
    model::TaskTemplateEntry const& tmpl,
    integration::SqlZoneIndexRepository& zoneRepo,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t startZoneId,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey,
    std::unordered_set<std::uint32_t> const* exploredZoneIds,
    AmbientSessionResumeHint const* resumeHint = nullptr)
{
    AmbientSession session;
    integration::SqlTaskPointRepository pointRepo;
    std::uint32_t currentZoneId = startZoneId;
    std::uint16_t currentMapId = 0;
    float currentX = 0.f;
    float currentY = 0.f;
    float currentZ = 0.f;
    std::uint32_t resolvedCurrentZoneId = currentZoneId;
    bool currentLocationResolved = false;
    if (currentZoneId != 0)
    {
        currentLocationResolved =
            ResolveZoneTarget(zoneRepo, currentZoneId, currentMapId, currentX, currentY, currentZ, resolvedCurrentZoneId);
    }

    for (model::TaskTemplateStepEntry const& templateStep : tmpl.steps)
    {
        ResolvedTemplateStepTarget resolvedTarget;

        if (!ResolveTemplateStepTarget(
                templateStep,
                zoneRepo,
                pointRepo,
                rng,
                faction,
                level,
                currentZoneId,
                homeZoneId,
                homeAnchorPointKey,
                homeBindPointKey,
                resolvedTarget))
        {
            return std::nullopt;
        }

        AmbientSessionTask task;
        task.activityId   = 0;
        task.activityKey  = tmpl.templateKey + ":" + templateStep.stepType;
        task.displayName  = !resolvedTarget.resolvedLabel.empty()
            ? resolvedTarget.resolvedLabel
            : templateStep.label;
        task.activityType = templateStep.stepType;
        task.taskFamily   = tmpl.taskFamily;
        task.targetZoneId = resolvedTarget.targetZoneId;
        task.targetPointKey = !resolvedTarget.resolvedPointKey.empty()
            ? resolvedTarget.resolvedPointKey
            : templateStep.targetPointKey;
        if (!task.targetPointKey.empty())
        {
            if (!resolvedTarget.resolvedPointType.empty())
                task.targetPointType = resolvedTarget.resolvedPointType;
            else if (auto const point = pointRepo.FindByKey(task.targetPointKey))
                task.targetPointType = point->pointType;
        }

        std::int32_t const taskIndex = static_cast<std::int32_t>(session.tasks.size());
        session.tasks.push_back(std::move(task));

        bool addedTransitRoute = false;
        if (currentZoneId != 0 && currentZoneId != resolvedTarget.targetZoneId)
        {
            std::vector<model::TaskTransitRouteEntry> const transitPath = pointRepo.FindTransitPathForZones(
                currentZoneId,
                resolvedTarget.targetZoneId,
                tmpl.requiredFaction,
                tmpl.minLevel);
            if (!transitPath.empty())
            {
                bool const pathUsesOnlyTaxi = std::all_of(
                    transitPath.begin(),
                    transitPath.end(),
                    [](model::TaskTransitRouteEntry const& route)
                    {
                        return route.transitType == "taxi";
                    });

                if (pathUsesOnlyTaxi
                    && AppendDynamicTaxiTransit(
                        session,
                        taskIndex,
                        faction,
                        exploredZoneIds,
                        currentLocationResolved,
                        currentMapId,
                        currentZoneId,
                        currentX,
                        currentY,
                        currentZ,
                        resolvedTarget.mapId,
                            resolvedTarget.targetX,
                            resolvedTarget.targetY,
                            resolvedTarget.targetZ,
                            task.displayName))
                {
                    addedTransitRoute = true;
                }
                else
                {
                    for (model::TaskTransitRouteEntry const& route : transitPath)
                    {
                        AppendTravelStep(
                            session,
                            taskIndex,
                            route.sourceMapId,
                            route.sourceX,
                            route.sourceY,
                            route.sourceZ,
                            "Travel to " + route.sourcePointName);
                        AppendTransitStep(
                            session,
                            taskIndex,
                            route.destMapId,
                            route.destX,
                            route.destY,
                            route.destZ,
                            std::max<std::uint32_t>(15u, route.durationSec),
                            route.displayName,
                            route.routeKey,
                            route.sourcePointKey,
                            route.destPointKey,
                            route.transitType,
                            route.sourcePointName,
                            route.destPointName);
                    }

                    if (transitPath.back().destPointKey != task.targetPointKey)
                    {
                        AppendTravelStep(
                            session,
                            taskIndex,
                            resolvedTarget.mapId,
                            resolvedTarget.targetX,
                            resolvedTarget.targetY,
                            resolvedTarget.targetZ,
                            "Travel to " + task.displayName);
                    }

                    addedTransitRoute = true;
                }
            }
            else if (AppendDynamicTaxiTransit(
                session,
                taskIndex,
                faction,
                exploredZoneIds,
                currentLocationResolved,
                currentMapId,
                currentZoneId,
                currentX,
                currentY,
                currentZ,
                resolvedTarget.mapId,
                resolvedTarget.targetX,
                resolvedTarget.targetY,
                resolvedTarget.targetZ,
                task.displayName))
            {
                addedTransitRoute = true;
            }
        }

        if (!addedTransitRoute)
        {
            AppendTravelStep(
                session,
                taskIndex,
                resolvedTarget.mapId,
                resolvedTarget.targetX,
                resolvedTarget.targetY,
                resolvedTarget.targetZ,
                "Travel to " + task.displayName);
        }

        std::uint32_t activityDurationSec = 600u;
        if (templateStep.durationMinSec != 0 || templateStep.durationMaxSec != 0)
        {
            std::uniform_int_distribution<std::uint32_t> durDist(
                templateStep.durationMinSec,
                std::max(templateStep.durationMinSec, templateStep.durationMaxSec));
            activityDurationSec = durDist(rng);
        }
        else if (resolvedTarget.durationOverrideSec != 0)
        {
            activityDurationSec = resolvedTarget.durationOverrideSec;
        }
        activityDurationSec *= std::max<std::uint32_t>(1u, templateStep.cycleCount);

        std::string const resolverKind = EffectiveResolverKind(templateStep);
        bool const questResolver = resolverKind == "quest_auto" || resolverKind == "quest_zone";
        if (questResolver && ActivityTypeToStepType(templateStep.stepType) == AmbientStepType::Grind)
        {
            std::string const startingHubId = ExtractQuestHubId(resolvedTarget.resolvedSubjectKey);
            auto startingHub = startingHubId.empty()
                ? std::optional<WorldBotQuestHub>{}
                : GetSessionComposerQuestHubRepository().FindHubById(startingHubId);
            if (!startingHub)
                return std::nullopt;

            std::uint64_t initialHubElapsedMs = 0;
            if (resumeHint
                && IsQuestingTaskFamily(resumeHint->lastTaskFamily)
                && ExtractQuestHubId(resumeHint->lastStepSubjectKey) == startingHub->hubId
                && IsQuestHubEligibleForLevel(level, *startingHub))
            {
                initialHubElapsedMs = resumeHint->lastQuestHubElapsedMs;
            }

            if (!AppendBudgetedQuestHubSteps(
                    session,
                    taskIndex,
                    task.displayName,
                    resolverKind == "quest_zone"
                        ? std::optional<std::uint32_t>(templateStep.targetZoneId)
                        : std::nullopt,
                    rng,
                    faction,
                    level,
                    homeZoneId,
                    activityDurationSec,
                    *startingHub,
                    initialHubElapsedMs,
                    false,
                    currentZoneId,
                    currentMapId,
                    currentX,
                    currentY,
                    currentZ,
                    currentLocationResolved))
            {
                return std::nullopt;
            }

            continue;
        }

        AmbientStep activityStep;
        activityStep.type      = ActivityTypeToStepType(templateStep.stepType);
        activityStep.mapId     = resolvedTarget.mapId;
        activityStep.x         = resolvedTarget.targetX;
        activityStep.y         = resolvedTarget.targetY;
        activityStep.z         = resolvedTarget.targetZ;
        activityStep.taskIndex = taskIndex;
        activityStep.subjectKind = EffectiveSubjectKind(templateStep);
        activityStep.subjectId = resolvedTarget.resolvedSubjectId != 0
            ? resolvedTarget.resolvedSubjectId
            : templateStep.subjectId;
        activityStep.subjectKey = !resolvedTarget.resolvedSubjectKey.empty()
            ? resolvedTarget.resolvedSubjectKey
            : templateStep.subjectKey;
        activityStep.returnAnchorRole = !resolvedTarget.resolvedReturnAnchorRole.empty()
            ? resolvedTarget.resolvedReturnAnchorRole
            : templateStep.returnAnchorRole;
        activityStep.cycleCount = std::max<std::uint8_t>(1u, templateStep.cycleCount);
        activityStep.targetPointKey = task.targetPointKey;
        if (taskIndex >= 0 && static_cast<std::size_t>(taskIndex) < session.tasks.size())
            activityStep.targetPointType = session.tasks[taskIndex].targetPointType;
        activityStep.combatRadius = 30.f;
        activityStep.label     = task.displayName;
        activityStep.durationSec = activityDurationSec;

        session.steps.push_back(activityStep);

        currentZoneId = resolvedTarget.targetZoneId;
        currentMapId = resolvedTarget.mapId;
        currentX = resolvedTarget.targetX;
        currentY = resolvedTarget.targetY;
        currentZ = resolvedTarget.targetZ;
        currentLocationResolved = true;
    }

    if (session.tasks.empty() || session.steps.empty())
        return std::nullopt;

    session.activityId = 0;
    session.activityKey = tmpl.templateKey;
    session.displayName = tmpl.displayName;
    session.sourceKind = "task_template";
    session.sourceKey = tmpl.templateKey;
    return session;
}

std::optional<model::TaskTemplateEntry> FindTemplateById(
    std::vector<model::TaskTemplateEntry> const& templates,
    std::uint32_t templateId)
{
    auto const itr = std::find_if(templates.begin(), templates.end(),
        [&](model::TaskTemplateEntry const& tmpl)
        {
            return tmpl.templateId == templateId;
        });
    if (itr == templates.end())
        return std::nullopt;

    return *itr;
}

std::optional<AmbientSession> BuildSessionFromPlaylist(
    model::PlaylistEntrySet const& playlist,
    std::vector<model::TaskTemplateEntry> const& templates,
    integration::SqlZoneIndexRepository& zoneRepo,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t startZoneId,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey,
    std::unordered_set<std::uint32_t> const* exploredZoneIds)
{
    AmbientSession session;
    std::uint32_t currentZoneId = startZoneId;
    std::optional<AmbientSessionResumeHint> chainedResumeHint;

    for (model::PlaylistEntryRef const& entry : playlist.entries)
    {
        auto const templateOpt = FindTemplateById(templates, entry.taskTemplateId);
        if (!templateOpt)
            return std::nullopt;

        std::uint8_t const repeatCount = std::max<std::uint8_t>(1u, entry.repeatCount);
        for (std::uint8_t repeatIndex = 0; repeatIndex < repeatCount; ++repeatIndex)
        {
            auto subSession = BuildSessionFromTemplate(
                *templateOpt,
                zoneRepo,
                rng,
                faction,
                level,
                currentZoneId,
                homeZoneId,
                homeAnchorPointKey,
                homeBindPointKey,
                exploredZoneIds,
                chainedResumeHint ? &*chainedResumeHint : nullptr);
            if (!subSession)
                return std::nullopt;

            std::int32_t const taskIndexOffset = static_cast<std::int32_t>(session.tasks.size());
            for (service::AmbientSessionTask task : subSession->tasks)
                session.tasks.push_back(std::move(task));

            for (service::AmbientStep step : subSession->steps)
            {
                if (step.taskIndex >= 0)
                    step.taskIndex += taskIndexOffset;
                session.steps.push_back(std::move(step));
            }

            if (!subSession->tasks.empty())
                currentZoneId = subSession->tasks.back().targetZoneId;

            AmbientSessionResumeHint nextHint = BuildChainedResumeHint(*subSession);
            if (!chainedResumeHint || IsQuestingTaskFamily(nextHint.lastTaskFamily))
                chainedResumeHint = std::move(nextHint);
        }
    }

    if (session.tasks.empty() || session.steps.empty())
        return std::nullopt;

    session.activityId = 0;
    session.activityKey = playlist.playlistKey;
    session.displayName = playlist.displayName;
    session.sourceKind = "playlist";
    session.sourceKey = playlist.playlistKey;
    return session;
}

std::optional<AmbientSession> TryBuildQuestResumeSession(
    std::vector<model::TaskTemplateEntry> const& templates,
    integration::SqlZoneIndexRepository& zoneRepo,
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    std::uint32_t startZoneId,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey,
    std::unordered_set<std::uint32_t> const* exploredZoneIds,
    AmbientSessionResumeHint const* resumeHint)
{
    if (!resumeHint
        || !IsQuestingTaskFamily(resumeHint->lastTaskFamily)
        || resumeHint->lastTaskTargetZoneId == 0)
    {
        return std::nullopt;
    }

    std::string const lastHubId = ExtractQuestHubId(resumeHint->lastStepSubjectKey);
    if (!lastHubId.empty())
    {
        auto const hub = GetSessionComposerQuestHubRepository().FindHubById(lastHubId);
        if (!hub || !IsQuestHubEligibleForLevel(level, *hub))
            return std::nullopt;
    }
    else
    {
        auto const zone = zoneRepo.Find(resumeHint->lastTaskTargetZoneId);
        if (!zone || !IsQuestResumeLevelAppropriate(level, *zone))
            return std::nullopt;
    }

    std::vector<model::TaskTemplateEntry const*> candidates;
    for (model::TaskTemplateEntry const& tmpl : templates)
    {
        if (TemplateSupportsQuestResume(tmpl, resumeHint->lastTaskTargetZoneId))
            candidates.push_back(&tmpl);
    }

    if (candidates.empty())
        return std::nullopt;

    std::uint32_t const totalWeight = std::accumulate(
        candidates.begin(), candidates.end(), std::uint32_t{0},
        [](std::uint32_t sum, model::TaskTemplateEntry const* tmpl)
        {
            return sum + std::max<std::uint32_t>(1u, tmpl ? tmpl->weight : 1u);
        });

    std::uniform_int_distribution<std::uint32_t> dist(1, totalWeight);
    std::uint32_t roll = dist(rng);
    for (model::TaskTemplateEntry const* tmpl : candidates)
    {
        std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, tmpl ? tmpl->weight : 1u);
        if (roll <= effectiveWeight)
        {
            auto session = BuildSessionFromTemplate(
                *tmpl,
                zoneRepo,
                rng,
                faction,
                level,
                resumeHint->lastTaskTargetZoneId,
                homeZoneId,
                homeAnchorPointKey,
                homeBindPointKey,
                exploredZoneIds,
                resumeHint);
            if (!session)
                return std::nullopt;

            session->sourceKind = "quest_resume";
            session->sourceKey =
                (resumeHint->lastSessionSourceKey.empty()
                    ? tmpl->templateKey
                    : resumeHint->lastSessionSourceKey)
                + ":zone_" + std::to_string(resumeHint->lastTaskTargetZoneId);
            if (!session->displayName.empty())
                session->displayName += " (Resume)";
            (void)startZoneId;
            return session;
        }

        roll -= effectiveWeight;
    }

    return std::nullopt;
}

std::optional<AmbientSession> BuildDirectQuestHubSession(
    WorldBotQuestHub const& hub,
    std::mt19937& rng)
{
    AmbientSession session;
    std::string const displayName = hub.zoneName + " Questing";

    AmbientSessionTask task;
    task.activityId = 0;
    task.activityKey = "quest_hub:" + hub.hubId;
    task.displayName = displayName;
    task.activityType = "grind";
    task.taskFamily = "questing";
    task.targetZoneId = hub.zoneId;
    std::int32_t const taskIndex = static_cast<std::int32_t>(session.tasks.size());
    session.tasks.push_back(std::move(task));

    std::uint32_t currentZoneId = 0;
    std::uint16_t currentMapId = 0;
    float currentX = 0.0f;
    float currentY = 0.0f;
    float currentZ = 0.0f;
    bool currentLocationResolved = false;
    if (!AppendBudgetedQuestHubSteps(
            session,
            taskIndex,
            displayName,
            std::optional<std::uint32_t>(hub.zoneId),
            rng,
            hub.requiredFaction,
            static_cast<std::uint8_t>(hub.avgLevel == 0 ? hub.minLevel : hub.avgLevel),
            hub.zoneId,
            std::max<std::uint32_t>(300u, hub.estimatedMinutes * 60u),
            hub,
            0u,
            true,
            currentZoneId,
            currentMapId,
            currentX,
            currentY,
            currentZ,
            currentLocationResolved))
    {
        return std::nullopt;
    }

    session.activityId = 0;
    session.activityKey = "quest_hub:" + hub.hubId;
    session.displayName = displayName;
    session.sourceKind = "quest_hub";
    session.sourceKey = "quest_hub:" + hub.hubId;
    return session;
}

std::optional<AmbientSession> TryBuildQuestHubFollowupSession(
    std::mt19937& rng,
    std::uint8_t faction,
    std::uint8_t level,
    AmbientSessionResumeHint const* resumeHint)
{
    if (!resumeHint
        || !IsQuestingTaskFamily(resumeHint->lastTaskFamily)
        || !IsQuestHubSubjectKey(resumeHint->lastStepSubjectKey))
    {
        return std::nullopt;
    }

    std::string const previousHubId = ExtractQuestHubId(resumeHint->lastStepSubjectKey);
    if (previousHubId.empty())
        return std::nullopt;

    auto const previousHub = GetSessionComposerQuestHubRepository().FindHubById(previousHubId);
    if (!previousHub)
        return std::nullopt;

    std::uint64_t const previousHubBudgetMs =
        static_cast<std::uint64_t>(std::max<std::uint32_t>(300u, previousHub->estimatedMinutes * 60u)) * 1000ull;
    bool const hubExhausted = resumeHint->lastQuestHubElapsedMs >= previousHubBudgetMs;
    bool const hubStillAppropriate = IsQuestHubEligibleForLevel(level, *previousHub);
    if (!hubExhausted && hubStillAppropriate)
        return std::nullopt;

    auto const nextHub = ResolveQuestHubBranchOrFallback(
        *previousHub,
        std::nullopt,
        rng,
        faction,
        level,
        previousHub->zoneId);
    if (!nextHub)
        return std::nullopt;

    return BuildDirectQuestHubSession(*nextHub, rng);
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

bool IsCityErrand(model::ActivityEntry const& entry)
{
    return NormalizeTaskFamily(entry.taskFamily) == "city_errand";
}

bool HasAnyNonCityCandidate(std::vector<model::ActivityEntry> const& pool)
{
    return std::any_of(pool.begin(), pool.end(), [](model::ActivityEntry const& entry)
    {
        return !IsCityErrand(entry);
    });
}
} // namespace

std::optional<AmbientSession> BotActivitySessionComposer::Compose(
    std::uint8_t faction,
    std::uint8_t level,
    bool hasHerbalism,
    bool hasMining,
    bool hasFishing,
    std::uint32_t startZoneId,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey,
    std::unordered_set<std::uint32_t> const* exploredZoneIds,
    AmbientSessionResumeHint const* resumeHint,
    AmbientSessionComposeBias const* composeBias,
    std::string const& sessionPersonalityKey) const
{
    AmbientProfessionCapabilities const professionCapabilities{
        hasHerbalism,
        hasMining,
        hasFishing
    };

    integration::SqlActivityLibraryRepository actRepo;
    auto eligible = actRepo.LoadEligible(
        faction, level, hasHerbalism, hasMining, hasFishing);

    eligible.erase(
        std::remove_if(
            eligible.begin(),
            eligible.end(),
            [&](model::ActivityEntry const& entry)
            {
                return !MeetsProfessionRequirements(entry, professionCapabilities)
                    || !SessionPersonalityAllowsTaskFamily(sessionPersonalityKey, entry.taskFamily);
            }),
        eligible.end());

    if (eligible.empty())
        return std::nullopt;

    integration::SqlZoneIndexRepository zoneRepo;

    integration::SqlTaskTemplateRepository taskTemplateRepo;
    auto templates = taskTemplateRepo.LoadEligible(
        faction, level, hasHerbalism, hasMining, hasFishing);
    auto playlists = taskTemplateRepo.LoadEligiblePlaylists(
        faction, level, hasHerbalism, hasMining, hasFishing);

    templates.erase(
        std::remove_if(
            templates.begin(),
            templates.end(),
            [&](model::TaskTemplateEntry const& entry)
            {
                return !MeetsProfessionRequirements(entry, professionCapabilities)
                    || !SessionPersonalityAllowsTaskFamily(sessionPersonalityKey, entry.taskFamily);
            }),
        templates.end());

    playlists.erase(
        std::remove_if(
            playlists.begin(),
            playlists.end(),
            [&](model::PlaylistEntrySet const& entry)
            {
                return !MeetsProfessionRequirements(entry, professionCapabilities)
                    || !SessionPersonalityAllowsTaskFamily(sessionPersonalityKey, entry.taskFamily);
            }),
        playlists.end());

    if (composeBias
        && (!composeBias->preferredTaskFamily.empty() || composeBias->preferredZoneId != 0))
    {
        templates.erase(
            std::remove_if(
                templates.begin(),
                templates.end(),
                [&](model::TaskTemplateEntry const& entry)
                {
                    return !TemplateMatchesComposeBias(entry, composeBias);
                }),
            templates.end());

        std::unordered_set<std::uint32_t> allowedTemplateIds;
        allowedTemplateIds.reserve(templates.size());
        for (model::TaskTemplateEntry const& entry : templates)
            allowedTemplateIds.insert(entry.templateId);

        playlists.erase(
            std::remove_if(
                playlists.begin(),
                playlists.end(),
                [&](model::PlaylistEntrySet const& entry)
                {
                    return !PlaylistMatchesComposeBias(entry, composeBias, allowedTemplateIds);
                }),
            playlists.end());
    }

    std::mt19937 rng(static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    if (auto session = TryBuildQuestHubFollowupSession(
            rng,
            faction,
            level,
            resumeHint))
    {
        return session;
    }

    if (!templates.empty())
    {
        if (auto session = TryBuildQuestResumeSession(
                templates,
                zoneRepo,
                rng,
                faction,
                level,
                startZoneId,
                homeZoneId,
                homeAnchorPointKey,
                homeBindPointKey,
                exploredZoneIds,
                resumeHint))
        {
            return session;
        }
    }

    if (!playlists.empty() && !templates.empty())
    {
        std::uint32_t const totalPlaylistWeight = std::accumulate(
            playlists.begin(), playlists.end(), std::uint32_t{0},
            [](std::uint32_t sum, model::PlaylistEntrySet const& playlist)
            {
                return sum + std::max<std::uint32_t>(1u, playlist.weight);
            });

        std::uniform_int_distribution<std::uint32_t> dist(1, totalPlaylistWeight);
        std::uint32_t roll = dist(rng);
        for (model::PlaylistEntrySet const& playlist : playlists)
        {
            std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, playlist.weight);
            if (roll <= effectiveWeight)
            {
                if (auto session = BuildSessionFromPlaylist(
                        playlist,
                        templates,
                        zoneRepo,
                        rng,
                        faction,
                        level,
                        startZoneId,
                        homeZoneId,
                        homeAnchorPointKey,
                        homeBindPointKey,
                        exploredZoneIds))
                    return session;
                break;
            }
            roll -= effectiveWeight;
        }
    }

    if (!templates.empty())
    {
        std::uint32_t const totalTemplateWeight = std::accumulate(
            templates.begin(), templates.end(), std::uint32_t{0},
            [](std::uint32_t sum, model::TaskTemplateEntry const& tmpl)
            {
                return sum + std::max<std::uint32_t>(1u, tmpl.weight);
            });

        std::uniform_int_distribution<std::uint32_t> dist(1, totalTemplateWeight);
        std::uint32_t roll = dist(rng);
        for (model::TaskTemplateEntry const& tmpl : templates)
        {
            std::uint32_t const effectiveWeight = std::max<std::uint32_t>(1u, tmpl.weight);
            if (roll <= effectiveWeight)
            {
                if (auto session = BuildSessionFromTemplate(
                        tmpl,
                        zoneRepo,
                        rng,
                        faction,
                        level,
                        startZoneId,
                        homeZoneId,
                        homeAnchorPointKey,
                        homeBindPointKey,
                        exploredZoneIds))
                    return session;
                break;
            }
            roll -= effectiveWeight;
        }
    }

    std::vector<model::ActivityEntry> filtered;
    filtered.reserve(eligible.size());
    for (model::ActivityEntry const& candidate : eligible)
    {
        if (!ActivityMatchesComposeBias(candidate, composeBias))
            continue;

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
    {
        std::uint32_t const fallbackZoneId = composeBias && composeBias->preferredZoneId != 0
            ? composeBias->preferredZoneId
            : startZoneId;

        if (fallbackZoneId != 0)
        {
            for (model::ActivityEntry const& candidate : actRepo.LoadZoneFallbackEligible(
                     fallbackZoneId,
                     faction,
                     level,
                     hasHerbalism,
                     hasMining,
                     hasFishing))
            {
                if (!ActivityMatchesComposeBias(candidate, composeBias))
                    continue;

                auto const zone = zoneRepo.Find(candidate.targetZoneId);
                if (!zone)
                    continue;

                if (!MatchesRequiredZoneType(candidate, *zone))
                    continue;

                filtered.push_back(candidate);
            }
        }
    }

    if (filtered.empty())
        return std::nullopt;

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

            if (selected.empty() && HasAnyNonCityCandidate(filtered) && IsCityErrand(candidate))
                continue;

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

                if (selected.empty() && HasAnyNonCityCandidate(filtered) && IsCityErrand(candidate))
                    continue;

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

        std::size_t const pickedSubsetIndex = WeightedPickIndexWithBias(
            chainCandidates,
            selected.empty(),
            rng);
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
    session.sourceKind = "legacy_activity";
    session.sourceKey = session.activityKey;

    return session;
}

std::optional<AmbientSession> BotActivitySessionComposer::ComposeForcedSourceKey(
    std::string const& sourceKey,
    std::uint8_t faction,
    std::uint8_t level,
    bool hasHerbalism,
    bool hasMining,
    bool hasFishing,
    std::uint32_t startZoneId,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey,
    std::unordered_set<std::uint32_t> const* exploredZoneIds) const
{
    if (sourceKey.empty())
        return std::nullopt;

    integration::SqlZoneIndexRepository zoneRepo;
    integration::SqlTaskTemplateRepository taskTemplateRepo;

    auto templates = taskTemplateRepo.LoadEligible(
        faction, level, hasHerbalism, hasMining, hasFishing);
    auto playlists = taskTemplateRepo.LoadEligiblePlaylists(
        faction, level, hasHerbalism, hasMining, hasFishing);

    std::mt19937 rng(static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    if (IsQuestHubSubjectKey(sourceKey))
    {
        if (auto const hub = GetSessionComposerQuestHubRepository().FindHubById(ExtractQuestHubId(sourceKey)))
            return BuildDirectQuestHubSession(*hub, rng);
    }

    if (auto const templateItr = std::find_if(
            templates.begin(),
            templates.end(),
            [&](model::TaskTemplateEntry const& tmpl)
            {
                return tmpl.templateKey == sourceKey;
            });
        templateItr != templates.end())
    {
        return BuildSessionFromTemplate(
            *templateItr,
            zoneRepo,
            rng,
            faction,
            level,
            startZoneId,
            homeZoneId,
            homeAnchorPointKey,
            homeBindPointKey,
            exploredZoneIds);
    }

    if (auto const playlistItr = std::find_if(
            playlists.begin(),
            playlists.end(),
            [&](model::PlaylistEntrySet const& playlist)
            {
                return playlist.playlistKey == sourceKey;
            });
        playlistItr != playlists.end())
    {
        return BuildSessionFromPlaylist(
            *playlistItr,
            templates,
            zoneRepo,
            rng,
            faction,
            level,
            startZoneId,
            homeZoneId,
            homeAnchorPointKey,
            homeBindPointKey,
            exploredZoneIds);
    }

    return std::nullopt;
}

} // namespace service
} // namespace living_world
