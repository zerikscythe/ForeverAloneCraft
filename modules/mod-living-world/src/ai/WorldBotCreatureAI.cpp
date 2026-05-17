#include "ai/WorldBotCreatureAI.h"

#include "Config.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "CellImpl.h"
#include "DataStores/DBCStores.h"
#include "Map.h"
#include "Globals/ObjectMgr.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ItemTemplate.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "DataStores/DBCStores.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlBotCombatDefaultProfileRepository.h"
#include "integration/SqlBotCombatProfileRepository.h"
#include "integration/SqlBotCombatProfileSelectionRepository.h"
#include "integration/SqlBotAssignedGearRepository.h"
#include "integration/SqlBotExploredZoneRepository.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "integration/SqlTaskPointRepository.h"
#include "integration/SqlBotTalentTemplateRepository.h"
#include "integration/SqlBotVirtualLoadoutRepository.h"
#include "service/BotCombatDoctrineResolver.h"
#include "service/BotHazardConfigService.h"
#include "service/BotCombatActionExecution.h"
#include "service/BotCombatRuntimeEvaluator.h"
#include "service/BotContextService.h"
#include "service/SimpleBotCombatSpecRoleResolver.h"
#include "service/WorldBotAssignedGearService.h"
#include "service/WorldBotCombatSituationBuilder.h"
#include "service/WorldBotMovementDoctrineEvaluator.h"
#include "service/WorldBotMovementExecution.h"
#include "service/WorldBotAttackPowerBaseline.h"
#include "service/WorldBotCombatRatingBaseline.h"
#include "service/WorldBotPassiveSpellRules.h"
#include "service/WorldBotHasteBaseline.h"
#include "service/WorldBotPhysicalDamageBaseline.h"
#include "service/WorldBotPlayerStatBaseline.h"
#include "service/WorldBotPowerBaseline.h"
#include "service/WorldBotPreparationService.h"
#include "service/WorldBotTaxiPlanning.h"
#include "model/BotSpecKey.h"
#include "Transport.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <limits>
#include <iomanip>
#include <sstream>

namespace living_world
{
namespace ai
{

namespace
{
std::filesystem::path ResolveWorldBotRouteExportRoot()
{
    std::string configured = sConfigMgr->GetOption<std::string>(
        "LivingWorld.RouteExportDir",
        "data/worldbot_routes");

    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(configured);
    if (std::filesystem::path(configured).is_relative())
    {
        candidates.emplace_back(std::filesystem::path("..") / configured);
        candidates.emplace_back(std::filesystem::path("..") / ".." / configured);
    }

    // Deployment-first fallbacks: prefer a server-local route bundle placed
    // alongside the worldserver environment before falling back to editor paths.
    candidates.emplace_back("data/worldbot_routes");
    candidates.emplace_back(std::filesystem::path("..") / "data" / "worldbot_routes");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "data" / "worldbot_routes");
    candidates.emplace_back("tools/lw-zone-editor/data/exported_routes");
    candidates.emplace_back(std::filesystem::path("..") / "tools" / "lw-zone-editor" / "data" / "exported_routes");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "tools" / "lw-zone-editor" / "data" / "exported_routes");

    for (std::filesystem::path const& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec))
            return candidate;
    }

    return std::filesystem::path(configured);
}

service::WorldBotRoutePlanner& GetWorldBotRoutePlanner()
{
    static service::WorldBotRoutePlanner planner(ResolveWorldBotRouteExportRoot());
    return planner;
}

service::WorldBotTaxiNetwork& GetWorldBotTaxiNetwork()
{
    static service::WorldBotTaxiNetwork network = service::LoadWorldBotTaxiNetwork();
    return network;
}

constexpr float GatherSearchRadius = 200.0f;
constexpr float GatherInteractRange = 6.0f;
constexpr float GatherAnchorReturnDistance = 60.0f;
constexpr std::uint32_t DebugManaGemItemId = 33312;
constexpr float CrossMapTransitAbstractSourceDistanceYards = 300.0f;
constexpr std::uint32_t CrossMapTransitAbstractMinElapsedMs = 20000u;

struct PhysicalTransitRouteSpec
{
    char const* routeKey = "";
    char const* transitType = "";
    std::uint32_t transportEntry = 0;
    float boardDetectRadius = 90.0f;
    float boardArriveRadius = 10.0f;
    float disembarkRadius = 70.0f;
};

struct PhysicalTransitDeckSpot
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

std::optional<PhysicalTransitRouteSpec> ResolvePhysicalTransitRouteSpec(
    service::AmbientStep const& step)
{
    std::string transitType = step.transitType;
    std::transform(
        transitType.begin(),
        transitType.end(),
        transitType.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (transitType != "boat")
        return std::nullopt;

    if (step.transitRouteKey == "ratchet_to_booty_bay"
        || step.transitRouteKey == "booty_bay_to_ratchet")
    {
        // Maiden's Fancy transport-local deck coordinates sampled from its
        // passenger map so boarding lands on a sane lower-deck location.
        return PhysicalTransitRouteSpec{
            step.transitRouteKey.c_str(),
            "boat",
            20808u,
            110.0f,
            8.0f,
            85.0f,
        };
    }

    return std::nullopt;
}

PhysicalTransitDeckSpot PickPhysicalTransitDeckSpot(PhysicalTransitRouteSpec const& routeSpec)
{
    if (routeSpec.transportEntry == 20808u)
    {
        // Maiden's Fancy transport-local deck spots sampled from its passenger map.
        static constexpr std::array<PhysicalTransitDeckSpot, 6> spots{{
            { 15.6121f,  1.09944f,  6.09764f, 2.52482f },
            { 17.8437f, -7.84575f,  6.09877f, 1.64493f },
            { 15.8067f, -5.80051f, 11.9732f,  1.86484f },
            {  9.39981f, 9.17899f, 11.5941f,  1.52083f },
            { -11.4014f, 6.67999f,  6.09785f, 2.93715f },
            {  6.20811f, 0.005208f, 14.0554f, 2.54813f },
        }};
        return spots[urand(0u, static_cast<std::uint32_t>(spots.size() - 1u))];
    }

    return {};
}

integration::SqlBotIdentityRepository& GetIdentityRepo()
{
    static integration::SqlBotIdentityRepository repo;
    return repo;
}

integration::SqlBotExploredZoneRepository& GetExploredZoneRepo()
{
    static integration::SqlBotExploredZoneRepository repo;
    return repo;
}

service::BotContextService& GetCombatContextService()
{
    static service::BotContextService service;
    return service;
}

service::BotCombatDoctrineResolver& GetDoctrineResolver()
{
    static integration::SqlAccountAltRuntimeRepository runtimeRepository;
    static integration::SqlBotCombatProfileRepository profileRepository;
    static integration::SqlBotCombatProfileSelectionRepository selectionRepository;
    static integration::SqlBotCombatDefaultProfileRepository defaultProfileRepository;
    static service::SimpleBotCombatSpecRoleResolver specRoleResolver;
    static service::BotCombatDoctrineResolver doctrineResolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        specRoleResolver,
        GetCombatContextService());
    return doctrineResolver;
}

service::WorldBotPreparationService& GetWorldBotPreparationService()
{
    static integration::SqlBotCombatDefaultProfileRepository defaultProfileRepository;
    static integration::SqlBotTalentTemplateRepository talentTemplateRepository;
    static integration::SqlBotVirtualLoadoutRepository virtualLoadoutRepository;
    static service::WorldBotPreparationService preparationService(
        defaultProfileRepository,
        talentTemplateRepository,
        virtualLoadoutRepository);
    return preparationService;
}

service::WorldBotAssignedGearService& GetWorldBotAssignedGearService()
{
    static integration::SqlBotAssignedGearRepository assignedGearRepository;
    static service::WorldBotAssignedGearService assignedGearService(assignedGearRepository);
    return assignedGearService;
}

service::BotCombatProfilePreparationService& GetProfilePreparationService()
{
    static service::BotCombatProfilePreparationService preparationService(
        GetDoctrineResolver());
    return preparationService;
}

service::BotCombatRuntimeEvaluator& GetRuntimeEvaluator()
{
    static service::BotCombatRuntimeEvaluator evaluator;
    return evaluator;
}

service::BotHazardConfigService& GetHazardConfigService()
{
    static integration::SqlBotHazardConfigRepository repo;
    static service::BotHazardConfigService service(repo);
    return service;
}

std::string DescribeResumeState(integration::BotIdentityRecord const& identity)
{
    if (identity.lastSeenZoneId != 0)
    {
        return "resume_from_zone=" + std::to_string(identity.lastSeenZoneId)
            + " session_count=" + std::to_string(identity.sessionCount);
    }

    return "fresh_spawn session_count=" + std::to_string(identity.sessionCount);
}

std::string DescribeNextTask(service::AmbientSession const& session, std::size_t currentStep)
{
    for (std::size_t i = currentStep; i < session.steps.size(); ++i)
    {
        service::AmbientStep const& step = session.steps[i];
        if (step.type != service::AmbientStepType::Travel)
            return step.label;
    }

    return "session_complete";
}

std::string FormatDurationMs(std::uint64_t ms)
{
    std::uint64_t const totalSeconds = ms / 1000ull;
    std::uint64_t const minutes = totalSeconds / 60ull;
    std::uint64_t const seconds = totalSeconds % 60ull;
    std::ostringstream oss;
    oss << minutes << ":" << std::setw(2) << std::setfill('0') << seconds;
    return oss.str();
}

std::string ResolveZoneName(std::uint32_t zoneId)
{
    if (zoneId == 0)
        return "Unknown";

    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(zoneId))
    {
        if (area->area_name[0] && area->area_name[0][0] != '\0')
            return area->area_name[0];
    }

    return "Zone " + std::to_string(zoneId);
}

std::string ResolveStepObjectiveLabel(service::AmbientSession const& session, service::AmbientStep const& step)
{
    if (step.taskIndex >= 0)
    {
        std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
        if (taskIndex < session.tasks.size())
        {
            service::AmbientSessionTask const& task = session.tasks[taskIndex];
            if (!task.displayName.empty())
                return task.displayName;
            if (!task.activityKey.empty())
                return task.activityKey;
        }
    }

    if (!step.label.empty())
        return step.label;
    if (!session.displayName.empty())
        return session.displayName;
    return "current objective";
}

std::string BuildTravelNarrative(
    service::AmbientSession const& session,
    service::AmbientStep const& step,
    std::string const& routeBits)
{
    std::string const objective = ResolveStepObjectiveLabel(session, step);
    std::string const zoneName = ResolveZoneName(
        step.taskIndex >= 0 && static_cast<std::size_t>(step.taskIndex) < session.tasks.size()
            ? session.tasks[static_cast<std::size_t>(step.taskIndex)].targetZoneId
            : 0u);

    std::ostringstream oss;
    oss << "En route to " << zoneName;
    if (!objective.empty() && objective != zoneName)
        oss << " for " << objective;
    if (!routeBits.empty())
        oss << " | " << routeBits;
    return oss.str();
}

char const* DescribeTravelOptionMode(service::WorldBotTravelOptionMode mode)
{
    switch (mode)
    {
        case service::WorldBotTravelOptionMode::TaxiFull:
            return "taxi_full";
        case service::WorldBotTravelOptionMode::TaxiPartial:
            return "taxi_partial";
        case service::WorldBotTravelOptionMode::Ground:
        default:
            return "ground";
    }
}

std::string BuildPositionDetail(
    Unit const* bot,
    std::string const& detail)
{
    if (!bot)
        return detail;

    return detail
        + " | zone=" + std::to_string(bot->GetZoneId())
        + " pos=("
        + std::to_string(bot->GetPositionX()) + ","
        + std::to_string(bot->GetPositionY()) + ","
        + std::to_string(bot->GetPositionZ()) + ")";
}

std::string DescribeSessionOrigin(service::AmbientSession const& session)
{
    std::string const sourceKind = session.sourceKind.empty()
        ? "unknown"
        : session.sourceKind;
    std::string const sourceKey = session.sourceKey.empty()
        ? session.activityKey
        : session.sourceKey;

    return "source_kind='" + sourceKind
        + "' source_key='" + sourceKey
        + "' session='" + session.displayName + "'";
}

std::string NormalizeTransitType(std::string transitType);
std::string DescribeScriptedTransitDetail(service::AmbientStep const& step);

std::string DescribeSessionBlueprint(service::AmbientSession const& session)
{
    std::ostringstream oss;
    oss << "tasks=" << session.tasks.size()
        << " steps=" << session.steps.size();

    for (std::size_t i = 0; i < session.steps.size(); ++i)
    {
        service::AmbientStep const& step = session.steps[i];
        oss << " [" << i << ":";
        switch (step.type)
        {
            case service::AmbientStepType::Travel:
                oss << "travel";
                break;
            case service::AmbientStepType::Idle:
                oss << "idle";
                break;
            case service::AmbientStepType::Patrol:
                oss << "patrol";
                break;
            case service::AmbientStepType::GatherHerb:
                oss << "gather_herb";
                break;
            case service::AmbientStepType::GatherOre:
                oss << "gather_ore";
                break;
            case service::AmbientStepType::Fish:
                oss << "fish";
                break;
            case service::AmbientStepType::Transit:
                oss << "transit";
                if (!step.transitType.empty())
                    oss << ":" << NormalizeTransitType(step.transitType);
                break;
            default:
                oss << "other";
                break;
        }

        if (!step.label.empty())
            oss << " '" << step.label << "'";

        oss << " -> (" << step.x << "," << step.y << "," << step.z << ")";
        if (step.durationSec != 0)
            oss << " dur=" << step.durationSec << "s";
        oss << "]";
    }

    return oss.str();
}

std::string DescribeTravelRecovery(
    service::AmbientStep const& step,
    char const* reason)
{
    return std::string(reason)
        + " -> " + step.label
        + " pos=("
        + std::to_string(step.x) + ","
        + std::to_string(step.y) + ","
        + std::to_string(step.z) + ")";
}

char const* DescribeAmbientStepTypeKey(service::AmbientStepType type)
{
    switch (type)
    {
        case service::AmbientStepType::Travel:
            return "travel";
        case service::AmbientStepType::GatherHerb:
            return "gather_herb";
        case service::AmbientStepType::GatherOre:
            return "gather_ore";
        case service::AmbientStepType::Fish:
            return "fish";
        case service::AmbientStepType::Idle:
            return "idle";
        case service::AmbientStepType::Patrol:
            return "patrol";
        case service::AmbientStepType::Transit:
            return "transit";
        default:
            return "unknown";
    }
}

std::string NormalizeTransitType(std::string transitType)
{
    if (transitType.empty())
        return "transit";

    std::transform(
        transitType.begin(),
        transitType.end(),
        transitType.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return transitType;
}

std::string DescribeScriptedTransitDetail(service::AmbientStep const& step)
{
    std::string const transitType = NormalizeTransitType(step.transitType);
    std::string const sourceLabel = step.transitSourceLabel.empty() ? "source" : step.transitSourceLabel;
    std::string const destLabel = step.transitDestLabel.empty() ? "destination" : step.transitDestLabel;

    return transitType + " " + sourceLabel + " -> " + destLabel;
}

std::uint32_t ResolveStepZoneId(
    service::AmbientSession const& session,
    service::AmbientStep const& step)
{
    if (step.taskIndex < 0)
        return 0;

    std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
    if (taskIndex >= session.tasks.size())
        return 0;

    return session.tasks[taskIndex].targetZoneId;
}

std::uint32_t CountNearbyHostileUnits(Unit* subject, float radius)
{
    if (!subject || radius <= 0.0f)
        return 0;

    std::vector<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(subject, subject, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(subject, targets, check);
    Cell::VisitObjects(subject, searcher, radius);
    return static_cast<std::uint32_t>(targets.size());
}

std::vector<service::WorldBotNearbyHostileSnapshot> CollectNearbyHostileSnapshots(
    Unit* subject,
    Unit* currentTarget,
    float radius)
{
    std::vector<service::WorldBotNearbyHostileSnapshot> snapshots;
    if (!subject || radius <= 0.0f)
        return snapshots;

    std::vector<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(subject, subject, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(subject, targets, check);
    Cell::VisitObjects(subject, searcher, radius);

    snapshots.reserve(targets.size());
    for (Unit* hostile : targets)
    {
        if (!hostile || hostile == subject)
            continue;

        service::WorldBotNearbyHostileSnapshot snapshot;
        snapshot.x = hostile->GetPositionX();
        snapshot.y = hostile->GetPositionY();
        snapshot.z = hostile->GetPositionZ();
        snapshot.isCurrentTarget = hostile == currentTarget;
        snapshot.engaged = snapshot.isCurrentTarget
            || hostile->IsThreatenedBy(subject)
            || subject->IsThreatenedBy(hostile);
        snapshots.push_back(snapshot);
    }

    return snapshots;
}

std::string DescribeSpellForTrace(std::uint32_t spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return std::to_string(spellId);

    char const* name = spellInfo->SpellName[0];
    if (!name || !*name)
        return std::to_string(spellId);

    return std::string(name) + "(" + std::to_string(spellId) + ")";
}

std::string DescribeCombatActionForTrace(service::BotCombatEvaluatedAction const& action)
{
    if (action.actionType == model::BotCombatActionType::Item)
        return "Item(" + std::to_string(action.itemId) + ")";

    return DescribeSpellForTrace(action.spellId);
}

char const* DescribeAoEMode(std::optional<model::BotCombatAoEMode> aoeMode)
{
    if (!aoeMode)
        return "none";

    switch (*aoeMode)
    {
        case model::BotCombatAoEMode::Centroid:
            return "centroid";
        case model::BotCombatAoEMode::Feet:
            return "feet";
    }

    return "unknown";
}

std::string DescribeTravelOptionChoice(service::WorldBotResolvedTravelOption const& option)
{
    std::ostringstream oss;
    if (option.usesTaxi() && option.taxiJourney.has_value() && !option.taxiJourney->empty())
    {
        service::WorldBotResolvedTaxiJourney const& journey = *option.taxiJourney;
        oss << "taking taxi via " << journey.taxiCandidate.sourceNode.name
            << " -> " << journey.taxiCandidate.destinationNode.name
            << " ride_eta=" << FormatDurationMs(journey.taxiCandidate.route.totalEtaMs)
            << " total_eta=" << FormatDurationMs(option.totalEtaMs);

        if (option.groundPlan.has_value() && option.groundPlan->etaMs > option.totalEtaMs)
        {
            oss << " saved="
                << FormatDurationMs(option.groundPlan->etaMs - option.totalEtaMs)
                << " vs roads";
        }

        return oss.str();
    }

    oss << "staying on roads eta=" << FormatDurationMs(option.totalEtaMs);
    if (option.taxiJourney.has_value() && !option.taxiJourney->empty())
    {
        service::WorldBotResolvedTaxiJourney const& journey = *option.taxiJourney;
        oss << " taxi_alt=" << FormatDurationMs(journey.totalEtaMs)
            << " via " << journey.taxiCandidate.sourceNode.name
            << " -> " << journey.taxiCandidate.destinationNode.name;
        if (journey.totalEtaMs > option.totalEtaMs)
            oss << " slower_by=" << FormatDurationMs(journey.totalEtaMs - option.totalEtaMs);
    }
    else
    {
        oss << " no_known_taxi_route";
    }

    return oss.str();
}

char const* DescribeMovementStyle(model::WorldBotMovementStyle style)
{
    switch (style)
    {
        case model::WorldBotMovementStyle::FrontlineTank:
            return "frontline_tank";
        case model::WorldBotMovementStyle::StickyMelee:
            return "sticky_melee";
        case model::WorldBotMovementStyle::TurretCaster:
            return "turret_caster";
        case model::WorldBotMovementStyle::MobileRanged:
            return "mobile_ranged";
        case model::WorldBotMovementStyle::BacklineHealer:
            return "backline_healer";
        case model::WorldBotMovementStyle::Unknown:
        default:
            return "unknown";
    }
}

char const* DescribeCombatPosture(model::WorldBotCombatPosture posture)
{
    switch (posture)
    {
        case model::WorldBotCombatPosture::Hold:
            return "hold";
        case model::WorldBotCombatPosture::Close:
            return "close";
        case model::WorldBotCombatPosture::Reposition:
            return "reposition";
        case model::WorldBotCombatPosture::Kite:
            return "kite";
        case model::WorldBotCombatPosture::Retreat:
            return "retreat";
        case model::WorldBotCombatPosture::HazardEscape:
            return "hazard_escape";
        default:
            return "unknown";
    }
}

char const* DescribeMovementDecisionSource(model::WorldBotMovementDecisionSource source)
{
    switch (source)
    {
        case model::WorldBotMovementDecisionSource::PostureDoctrine:
            return "posture_doctrine";
        case model::WorldBotMovementDecisionSource::HazardOverride:
            return "hazard_override";
        case model::WorldBotMovementDecisionSource::EmergencyFallback:
            return "emergency_fallback";
        case model::WorldBotMovementDecisionSource::None:
        default:
            return "none";
    }
}

char const* DescribeMovementPlanKind(service::WorldBotMovementPlanKind kind)
{
    switch (kind)
    {
        case service::WorldBotMovementPlanKind::Chase:
            return "chase";
        case service::WorldBotMovementPlanKind::MovePoint:
            return "move_point";
        case service::WorldBotMovementPlanKind::None:
        default:
            return "none";
    }
}

char const* DescribeTravelCapabilityTier(service::WorldBotTravelCapabilityTier tier)
{
    switch (tier)
    {
        case service::WorldBotTravelCapabilityTier::Foot:
            return "foot";
        case service::WorldBotTravelCapabilityTier::GroundBasic:
            return "ground_basic";
        case service::WorldBotTravelCapabilityTier::GroundFast:
            return "ground_fast";
        case service::WorldBotTravelCapabilityTier::FlightBasic:
            return "flight_basic";
        case service::WorldBotTravelCapabilityTier::FlightFast:
            return "flight_fast";
        case service::WorldBotTravelCapabilityTier::Taxi:
            return "taxi";
        default:
            return "unknown";
    }
}

model::WorldBotHazardSnapshot BuildWorldBotHazardSnapshot(
    Unit* bot,
    service::SharedHazardEvaluationState& hazardState)
{
    model::WorldBotHazardSnapshot snapshot;
    if (!bot)
        return snapshot;

    bool hasKnownAura = false;
    std::uint32_t hazardSpellId = 0;
    std::unordered_set<uint32_t> const auraIds = GetHazardConfigService().GetHazardAuraIds();
    for (uint32_t spellId : auraIds)
    {
        if (bot->HasAura(spellId))
        {
            hasKnownAura = true;
            hazardSpellId = spellId;
            break;
        }
    }

    Position const currentPosition = bot->GetPosition();
    service::SharedHazardEvaluationResult const evaluation =
        service::EvaluateSharedHazard(
            hazardState,
            std::chrono::steady_clock::now(),
            bot->GetHealthPct(),
            currentPosition,
            hasKnownAura,
            hazardSpellId,
            hasKnownAura ? 1.0f : 0.0f,
            GetHazardConfigService().GetTuning());

    snapshot.active = evaluation.dangerDetectedNow || evaluation.commitWindowActive;
    snapshot.explicitAuraTriggered = evaluation.explicitAuraTriggered;
    snapshot.repeatedDamageTriggered = evaluation.repeatedDamageTriggered;
    snapshot.commitWindowActive = evaluation.commitWindowActive;
    snapshot.hazardSpellId = evaluation.hazardSpellId;
    snapshot.consecutiveDamageTicks = static_cast<std::uint32_t>(std::max(evaluation.consecutiveDamageTicks, 0));
    snapshot.severity = evaluation.severity;

    return snapshot;
}

float GetManaPct(Unit const* unit)
{
    if (!unit)
        return 0.0f;

    std::uint32_t const maxMana = unit->GetMaxPower(POWER_MANA);
    if (maxMana == 0)
        return 0.0f;

    return 100.0f * static_cast<float>(unit->GetPower(POWER_MANA))
        / static_cast<float>(maxMana);
}

std::string DescribeVirtualLoadout(model::WorldBotVirtualLoadout const& loadout)
{
    std::ostringstream oss;
    oss << "virtual_loadout='" << loadout.displayName << "' "
        << "gear_tier=" << static_cast<std::uint32_t>(loadout.gearTier) << " "
        << "stats={str:" << loadout.bonusStrength
        << ",agi:" << loadout.bonusAgility
        << ",sta:" << loadout.bonusStamina
        << ",int:" << loadout.bonusIntellect
        << ",spi:" << loadout.bonusSpirit
        << ",hp:" << loadout.bonusHealth
        << ",mana:" << loadout.bonusMana
        << ",armor:" << loadout.bonusArmor
        << ",ap:" << loadout.bonusAttackPower
        << ",rap:" << loadout.bonusRangedAttackPower
        << "}";
    return oss.str();
}

std::string DescribeAssignedGearSummary(model::WorldBotAssignedGearSummary const& summary)
{
    std::ostringstream oss;
    oss << "assigned_gear_stats={str:" << summary.bonusStrength
        << ",agi:" << summary.bonusAgility
        << ",sta:" << summary.bonusStamina
        << ",int:" << summary.bonusIntellect
        << ",spi:" << summary.bonusSpirit
        << ",hp:" << summary.bonusHealth
        << ",mana:" << summary.bonusMana
        << ",armor:" << summary.bonusArmor
        << ",res_holy:" << summary.bonusHolyResistance
        << ",res_fire:" << summary.bonusFireResistance
        << ",res_nature:" << summary.bonusNatureResistance
        << ",res_frost:" << summary.bonusFrostResistance
        << ",res_shadow:" << summary.bonusShadowResistance
        << ",res_arcane:" << summary.bonusArcaneResistance
        << ",ap:" << summary.bonusAttackPower
        << ",rap:" << summary.bonusRangedAttackPower
        << ",def:" << summary.bonusDefenseSkillRating
        << ",dodge:" << summary.bonusDodgeRating
        << ",parry:" << summary.bonusParryRating
        << ",block:" << summary.bonusBlockRating
        << ",block_value:" << summary.bonusBlockValue
        << ",hit_melee:" << summary.bonusMeleeHitRating
        << ",hit_ranged:" << summary.bonusRangedHitRating
        << ",hit_spell:" << summary.bonusSpellHitRating
        << ",crit_melee:" << summary.bonusMeleeCritRating
        << ",crit_ranged:" << summary.bonusRangedCritRating
        << ",crit_spell:" << summary.bonusSpellCritRating
        << ",haste_melee:" << summary.bonusMeleeHasteRating
        << ",haste_ranged:" << summary.bonusRangedHasteRating
        << ",haste_spell:" << summary.bonusSpellHasteRating
        << ",expertise:" << summary.bonusExpertiseRating
        << ",armor_pen:" << summary.bonusArmorPenetrationRating
        << ",hit_taken:" << summary.bonusHitTakenRating
        << ",crit_taken:" << summary.bonusCritTakenRating
        << ",resilience:" << summary.bonusResilienceRating
        << ",spell_power:" << summary.bonusSpellPower
        << ",healing:" << summary.bonusHealingPower
        << ",mp5:" << summary.bonusManaRegen
        << ",hp5:" << summary.bonusHealthRegen
        << ",spell_pen:" << summary.bonusSpellPenetration
        << "}";
    return oss.str();
}

class NearestGatherNodeCheck
{
public:
    NearestGatherNodeCheck(WorldObject const& source, SkillType requiredSkill, float range)
        : _source(source), _requiredSkill(requiredSkill), _range(range)
    {
    }

    bool operator()(GameObject* go)
    {
        if (!go || !go->GetGOInfo() || !go->isSpawned())
            return false;

        if (!_source.IsWithinDistInMap(go, _range))
            return false;

        std::uint32_t const lockId = go->GetGOInfo()->GetLockId();
        if (lockId == 0)
            return false;

        LockEntry const* lock = sLockStore.LookupEntry(lockId);
        if (!lock)
            return false;

        bool matchesSkill = false;
        for (std::size_t i = 0; i < MAX_LOCK_CASE; ++i)
        {
            if (lock->Type[i] != LOCK_KEY_SKILL)
                continue;

            if (SkillByLockType(static_cast<LockType>(lock->Index[i])) == _requiredSkill)
            {
                matchesSkill = true;
                break;
            }
        }

        if (!matchesSkill)
            return false;

        _range = _source.GetDistance(go);
        return true;
    }

private:
    WorldObject const& _source;
    SkillType _requiredSkill;
    float _range;
};
} // namespace

// ---------------------------------------------------------------------------
WorldBotCreatureAI::WorldBotCreatureAI(Creature* creature)
    : CreatureAI(creature)
{
}

void WorldBotCreatureAI::InitializeAI()
{
    // Identity and session are set by the spawn path via SetIdentityAndSession.
    // Nothing to do here — we wait until that call arrives.
    me->SetReactState(REACT_AGGRESSIVE);
    me->SetUInt32Value(UNIT_NPC_FLAGS, 0);
    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);
}

void WorldBotCreatureAI::SetIdentityAndSession(
    integration::BotIdentityRecord const& identity,
    service::AmbientSession          const& session,
    std::size_t currentStep,
    std::uint32_t stepElapsedMs,
    std::uint64_t worldOnlineMsSoFar,
    bool alreadyMarkedActive,
    bool resumedFromAbstract)
{
    _identity     = identity;
    _identity.specKey = model::CanonicalizeBotSpecKey(_identity.specKey);
    _session      = session;
    _currentStep  = currentStep;
    _activityTimer = stepElapsedMs;
    _traveling    = false;
    _sessionDone  = false;
    _sessionReady = true;
    _worldOnlineMs = worldOnlineMsSoFar;
    _combatSuspendedStep = false;
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);
    _knownExploredZoneIds.clear();
    _preparedBuild = {};
    _preparedBuildReady = false;
    InvalidateCombatProfile();
    _lastDebugCombatManaDrainWorldMs = 0;
    _debugCombatManaGemObserved = false;
    _usedSimulatedItemsThisCombat.clear();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();

    _preparedBuild = GetWorldBotPreparationService().Prepare(_identity, "PvE");
    {
        service::WorldBotAssignedGearResult assignedGear = GetWorldBotAssignedGearService().EnsureAssignedGear(
            _identity,
            _preparedBuild.canonicalSpecKey,
            _preparedBuild.resolvedRoleKey);
        bool const gearRefreshStateChanged = _identity.gearRefreshPending != identity.gearRefreshPending
            || _identity.lastGearRefreshBand != identity.lastGearRefreshBand;
        if (gearRefreshStateChanged)
        {
            GetIdentityRepo().UpdateGearRefreshState(
                _identity.id,
                _identity.gearRefreshPending,
                _identity.lastGearRefreshBand);
        }

        _preparedBuild.assignedGear = std::move(assignedGear.entries);
        _preparedBuild.assignedGearSummary = assignedGear.summary;
        _preparedBuild.assignedGearRefreshBand = assignedGear.refreshBand;
        _preparedBuild.assignedGearRefreshed = assignedGear.refreshed;

        _hasShieldBaseline = false;
        for (model::WorldBotAssignedGearEntry const& entry : _preparedBuild.assignedGear)
        {
            if (entry.slot != EQUIPMENT_SLOT_OFFHAND)
                continue;

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
            if (itemTemplate && itemTemplate->InventoryType == INVTYPE_SHIELD)
                _hasShieldBaseline = true;
            break;
        }
    }
    _preparedBuildReady = _preparedBuild.IsReady();

    if (_preparedBuildReady)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "build_prepared",
            "personality='" + _preparedBuild.personalityKey
                + "' spec='" + _preparedBuild.canonicalSpecKey
                + "' role='" + _preparedBuild.resolvedRoleKey
                + "' gear_tier=" + std::to_string(_identity.gearTier)
                + "' default_profile_id=" + std::to_string(_preparedBuild.defaultCombatProfileId)
                + " talent_template_id=" + std::to_string(_preparedBuild.talentTemplateId)
                + " allocated_points=" + std::to_string(_preparedBuild.allocatedTalentPoints)
                + "/" + std::to_string(_preparedBuild.availableTalentPoints)
                + " known_spells=" + std::to_string(_preparedBuild.knownSpellIds.size())
                + " assigned_gear_slots=" + std::to_string(_preparedBuild.assignedGear.size())
                + " assigned_gear_band=" + std::to_string(_preparedBuild.assignedGearRefreshBand)
                + " assigned_gear_refreshed=" + std::to_string(_preparedBuild.assignedGearRefreshed ? 1 : 0)
                + " " + DescribeAssignedGearSummary(_preparedBuild.assignedGearSummary)
                + (_preparedBuild.virtualLoadout
                    ? " " + DescribeVirtualLoadout(*_preparedBuild.virtualLoadout)
                    : " virtual_loadout='none'"));
    }
    else
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "build_prepare_failed",
            "personality='" + _preparedBuild.personalityKey
                + "' spec='" + _preparedBuild.canonicalSpecKey
                + "' role='" + _preparedBuild.resolvedRoleKey
                + "' reason='" + _preparedBuild.failureReason + "'");
    }

    ApplyIdentityToCreature();

    for (std::uint32_t const zoneId : GetExploredZoneRepo().LoadExploredZones(_identity.id))
        _knownExploredZoneIds.insert(zoneId);

    if (!alreadyMarkedActive)
        GetIdentityRepo().MarkActive(_identity.id);

    ObserveCurrentZoneExploration();

    if (!alreadyMarkedActive)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "session_start",
            DescribeSessionOrigin(_session)
                + " tasks=" + std::to_string(_session.tasks.size())
                + " steps=" + std::to_string(_session.steps.size()));

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "session_blueprint",
            DescribeSessionBlueprint(_session));
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        DescribeResumeState(_identity));

    if (resumedFromAbstract)
    {
        if (_currentStep < _session.steps.size())
        {
            service::AmbientStep const& step = _session.steps[_currentStep];
            if (step.type == service::AmbientStepType::Transit)
                TryResumePhysicalTransit(step);
        }

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "status_change",
            "materialized_from_abstract step=" + std::to_string(_currentStep)
                + " step_elapsed_ms=" + std::to_string(_activityTimer)
                + " world_online_ms=" + std::to_string(_worldOnlineMs));
    }

    if (_identity.lastSeenZoneId != 0)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "status_change",
            "is alive - resuming tasks");
    }

    PersistRuntimeLedgerState();

    if (_currentStep >= _session.steps.size())
        CompletSession();
}

void WorldBotCreatureAI::ApplyIdentityToCreature()
{
    if (!me)
        return;

    me->SetName(_identity.name);
    me->SetLevel(_identity.level);
    me->SetDisplayId(_identity.displayId);
    me->SetFaction(_identity.faction == 2 ? FACTION_HORDE_GENERIC : FACTION_ALLIANCE_GENERIC);
    me->SetReactState(REACT_AGGRESSIVE);
    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);

    // Unit class lives in UNIT_FIELD_BYTES_0 byte 1. Byte 3 is the power type.
    // World-bot combat doctrine resolution calls Unit::getClass(), so writing the
    // class into byte 3 leaves runtime class at 0 and yields empty prepared
    // doctrine entries even after successful build preparation.
    me->SetByteValue(UNIT_FIELD_BYTES_0, 1, _identity.classId);

    Powers const powerType = service::ResolveWorldBotPowerType(_identity.classId);

    me->SetByteValue(UNIT_FIELD_BYTES_0, 3, static_cast<std::uint8_t>(powerType));

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(_identity.classId, _identity.level, &classInfo);

    PlayerLevelInfo levelInfo;
    sObjectMgr->GetPlayerLevelInfo(_identity.raceId, _identity.classId, _identity.level, &levelInfo);

    service::WorldBotPlayerStatBaseline const baseline =
        service::BuildWorldBotPlayerStatBaseline(classInfo, levelInfo);
    service::WorldBotAttackPowerBaseline const attackPowerBaseline =
        service::BuildWorldBotAttackPowerBaseline(
            _identity.classId,
            _identity.level,
            static_cast<std::int32_t>(baseline.stats[STAT_STRENGTH]),
            static_cast<std::int32_t>(baseline.stats[STAT_AGILITY]));
    service::WorldBotPhysicalDamageBaseline const physicalDamageBaseline =
        service::BuildWorldBotPhysicalDamageBaseline(
            me->GetAttackTime(BASE_ATTACK),
            me->GetAttackTime(OFF_ATTACK),
            me->GetAttackTime(RANGED_ATTACK));

    for (std::uint8_t i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        me->SetCreateStat(Stats(i), static_cast<float>(baseline.stats[i]));
        me->SetStat(Stats(i), static_cast<int32>(baseline.stats[i]));
    }

    me->SetCreateHealth(baseline.baseHealth);
    me->SetMaxHealth(baseline.baseHealth);
    me->SetArmor(static_cast<int32>(baseline.baseArmor));
    me->SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + AsUnderlyingType(SPELL_SCHOOL_NORMAL), 0.0f);
    me->SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + AsUnderlyingType(SPELL_SCHOOL_NORMAL), 0.0f);

    for (std::uint8_t i = 1; i < MAX_SPELL_SCHOOL; ++i)
    {
        me->SetResistance(SpellSchools(i), 0);
        me->SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + i, 0.0f);
        me->SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + i, 0.0f);
    }

    me->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, static_cast<float>(attackPowerBaseline.meleeAttackPower));
    me->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, static_cast<float>(attackPowerBaseline.rangedAttackPower));
    me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, physicalDamageBaseline.mainHandMinDamage);
    me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, physicalDamageBaseline.mainHandMaxDamage);
    me->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, physicalDamageBaseline.offHandMinDamage);
    me->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, physicalDamageBaseline.offHandMaxDamage);
    me->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, physicalDamageBaseline.rangedMinDamage);
    me->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, physicalDamageBaseline.rangedMaxDamage);

    if (powerType == POWER_MANA)
    {
        if (baseline.baseMana > 0)
        {
            me->SetCreateMana(baseline.baseMana);
            me->SetMaxPower(POWER_MANA, baseline.baseMana);
            me->SetPower(POWER_MANA, baseline.baseMana);
        }
    }
    else
    {
        std::uint32_t const maxPower = service::ResolveWorldBotMaxPower(powerType, 0);
        me->SetMaxPower(powerType, maxPower);
        me->SetPower(powerType, service::ResolveWorldBotSpawnPower(powerType, maxPower));
    }

    if (_preparedBuild.virtualLoadout)
    {
        auto const applyStatBonus =
            [&](Stats stat, std::int32_t bonus)
            {
                if (bonus == 0)
                    return;

                me->SetCreateStat(stat, me->GetCreateStat(stat) + static_cast<float>(bonus));
                me->SetStat(stat, static_cast<int32>(me->GetStat(stat) + static_cast<float>(bonus)));
            };

        auto const applyUnitBonus =
            [&](UnitMods unitMod, std::int32_t bonus)
            {
                if (bonus == 0)
                    return;

                float const currentBonus = me->GetFlatModifierValue(unitMod, TOTAL_VALUE);
                me->SetStatFlatModifier(unitMod, TOTAL_VALUE, currentBonus + static_cast<float>(bonus));
            };

        model::WorldBotVirtualLoadout const& loadout = *_preparedBuild.virtualLoadout;
        applyStatBonus(STAT_STRENGTH, loadout.bonusStrength);
        applyStatBonus(STAT_AGILITY, loadout.bonusAgility);
        applyStatBonus(STAT_STAMINA, loadout.bonusStamina);
        applyStatBonus(STAT_INTELLECT, loadout.bonusIntellect);
        applyStatBonus(STAT_SPIRIT, loadout.bonusSpirit);

        applyUnitBonus(UNIT_MOD_HEALTH, loadout.bonusHealth);
        applyUnitBonus(UNIT_MOD_MANA, loadout.bonusMana);
        applyUnitBonus(UNIT_MOD_ARMOR, loadout.bonusArmor);
        applyUnitBonus(UNIT_MOD_ATTACK_POWER, loadout.bonusAttackPower);
        applyUnitBonus(UNIT_MOD_ATTACK_POWER_RANGED, loadout.bonusRangedAttackPower);
    }

    {
        model::WorldBotAssignedGearSummary const& summary = _preparedBuild.assignedGearSummary;

        auto const applyStatBonus =
            [&](Stats stat, std::int32_t bonus)
            {
                if (bonus == 0)
                    return;

                me->SetCreateStat(stat, me->GetCreateStat(stat) + static_cast<float>(bonus));
                me->SetStat(stat, static_cast<int32>(me->GetStat(stat) + static_cast<float>(bonus)));
            };

        auto const applyUnitBonus =
            [&](UnitMods unitMod, std::int32_t bonus)
            {
                if (bonus == 0)
                    return;

                float const currentBonus = me->GetFlatModifierValue(unitMod, TOTAL_VALUE);
                me->SetStatFlatModifier(unitMod, TOTAL_VALUE, currentBonus + static_cast<float>(bonus));
            };

        auto const applyResistanceBonus =
            [&](SpellSchools school, std::int32_t bonus)
            {
                if (bonus == 0)
                    return;

                me->SetResistance(school, std::max<int32>(0, me->GetResistance(school) + bonus));
            };

        applyStatBonus(STAT_STRENGTH, summary.bonusStrength);
        applyStatBonus(STAT_AGILITY, summary.bonusAgility);
        applyStatBonus(STAT_STAMINA, summary.bonusStamina);
        applyStatBonus(STAT_INTELLECT, summary.bonusIntellect);
        applyStatBonus(STAT_SPIRIT, summary.bonusSpirit);

        applyUnitBonus(UNIT_MOD_HEALTH, summary.bonusHealth);
        applyUnitBonus(UNIT_MOD_MANA, summary.bonusMana);
        applyUnitBonus(UNIT_MOD_ARMOR, summary.bonusArmor);
        applyUnitBonus(UNIT_MOD_ATTACK_POWER, summary.bonusAttackPower);
        applyUnitBonus(UNIT_MOD_ATTACK_POWER_RANGED, summary.bonusRangedAttackPower);

        applyResistanceBonus(SPELL_SCHOOL_HOLY, summary.bonusHolyResistance);
        applyResistanceBonus(SPELL_SCHOOL_FIRE, summary.bonusFireResistance);
        applyResistanceBonus(SPELL_SCHOOL_NATURE, summary.bonusNatureResistance);
        applyResistanceBonus(SPELL_SCHOOL_FROST, summary.bonusFrostResistance);
        applyResistanceBonus(SPELL_SCHOOL_SHADOW, summary.bonusShadowResistance);
        applyResistanceBonus(SPELL_SCHOOL_ARCANE, summary.bonusArcaneResistance);
    }

    for (std::uint32_t spellId : _preparedBuild.knownSpellIds)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!service::ShouldAutoCastWorldBotPassiveSpell(spellInfo))
            continue;

        if (me->HasAura(spellId))
            continue;

        me->CastSpell(me, spellId, true);
    }

    me->UpdateAllStats();
    {
        model::WorldBotAssignedGearSummary const& summary = _preparedBuild.assignedGearSummary;
        service::ApplyWorldBotHasteBonus(
            *me,
            service::ResolveWorldBotCombatRatingBonus(me, CR_HASTE_MELEE, summary.bonusMeleeHasteRating),
            service::ResolveWorldBotCombatRatingBonus(me, CR_HASTE_RANGED, summary.bonusRangedHasteRating),
            service::ResolveWorldBotCombatRatingBonus(me, CR_HASTE_SPELL, summary.bonusSpellHasteRating));
    }
    me->SetFullHealth();

    if (powerType != POWER_MANA)
        me->SetMaxPower(powerType, service::ResolveWorldBotMaxPower(powerType, 0));

    if (me->GetMaxPower(powerType) > 0)
        me->SetPower(powerType, service::ResolveWorldBotSpawnPower(powerType, me->GetMaxPower(powerType)));
}

void WorldBotCreatureAI::UpdateAI(uint32 diff)
{
    if (!_sessionReady || _sessionDone)
        return;

    _worldOnlineMs += diff;

    _tickAccum += diff;
    if (_tickAccum < TickIntervalMs)
        return;
    _tickAccum -= TickIntervalMs;

    ObserveCurrentZoneExploration();

    if (me->IsInCombat() || me->GetVictim())
    {
        TickCombat(TickIntervalMs);
        return;
    }

    if (_combatSuspendedStep)
        ResumeSuspendedStepAfterCombat();

    TickStep(TickIntervalMs);

    std::uint32_t const snapshotIntervalMs =
        _session.sourceKind == "debug_route_harness"
            ? 5000u
            : PositionSnapshotIntervalMs;

    if ((_worldOnlineMs % snapshotIntervalMs) < TickIntervalMs)
    {
        std::string detail = DescribeNextTask(_session, _currentStep);
        if (_currentStep < _session.steps.size()
            && _session.steps[_currentStep].type == service::AmbientStepType::Travel
            && _traveling)
        {
            detail = BuildTravelNarrative(
                _session,
                _session.steps[_currentStep],
                DescribeActiveTravelTarget(_session.steps[_currentStep]));
        }
        else if (_currentStep < _session.steps.size()
            && _session.steps[_currentStep].type == service::AmbientStepType::Transit)
        {
            detail = DescribeRuntimeStateDetail();
        }

        RecordPositionSnapshot("position_tick", detail);
    }
}

std::string WorldBotCreatureAI::DescribeRuntimeStateKey() const
{
    if (!_sessionReady)
        return "session_loading";
    if (_sessionDone || _currentStep >= _session.steps.size())
        return "session_complete";

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type == service::AmbientStepType::Travel)
    {
        switch (_activeTravelExecutionPhase)
        {
            case ActiveTravelExecutionPhase::TaxiApproach:
                return "travel_taxi_approach";
            case ActiveTravelExecutionPhase::TaxiTransit:
                return "travel_taxi_flight";
            case ActiveTravelExecutionPhase::TaxiFinalLeg:
                return "travel_taxi_final_leg";
            case ActiveTravelExecutionPhase::GroundOnly:
                return "travel_ground";
            case ActiveTravelExecutionPhase::None:
            default:
                return _traveling ? "travel_ground" : "travel_planning";
        }
    }

    if (step.type == service::AmbientStepType::Transit)
    {
        if (_activeTransitExecutionPhase != ActiveTransitExecutionPhase::None)
        {
            switch (_activeTransitExecutionPhase)
            {
                case ActiveTransitExecutionPhase::WaitingForTransport:
                    return std::string("travel_transit_") + NormalizeTransitType(step.transitType) + "_wait";
                case ActiveTransitExecutionPhase::Boarding:
                    return std::string("travel_transit_") + NormalizeTransitType(step.transitType) + "_board";
                case ActiveTransitExecutionPhase::Riding:
                    return std::string("travel_transit_") + NormalizeTransitType(step.transitType) + "_ride";
                case ActiveTransitExecutionPhase::None:
                default:
                    break;
            }
        }

        return std::string("travel_transit_") + NormalizeTransitType(step.transitType);
    }

    return std::string("activity_") + DescribeAmbientStepTypeKey(step.type);
}

std::string WorldBotCreatureAI::DescribeRuntimeStateDetail() const
{
    if (!_sessionReady)
        return "Preparing session";
    if (_sessionDone || _currentStep >= _session.steps.size())
        return "Session complete";

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type == service::AmbientStepType::Travel)
    {
        if (_traveling)
            return BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step));

        return BuildTravelNarrative(_session, step, "planning route");
    }

    if (step.type == service::AmbientStepType::Transit)
    {
        if (_activeTransitExecutionPhase != ActiveTransitExecutionPhase::None && !_activePhysicalTransit.empty())
        {
            switch (_activeTransitExecutionPhase)
            {
                case ActiveTransitExecutionPhase::WaitingForTransport:
                    return "Waiting at " + _activePhysicalTransit.sourceLabel
                        + " for " + _activePhysicalTransit.transitType
                        + " to " + _activePhysicalTransit.destLabel;
                case ActiveTransitExecutionPhase::Boarding:
                    return "Boarding " + _activePhysicalTransit.transitType
                        + " " + _activePhysicalTransit.sourceLabel
                        + " -> " + _activePhysicalTransit.destLabel;
                case ActiveTransitExecutionPhase::Riding:
                    return "Riding " + _activePhysicalTransit.transitType
                        + " " + _activePhysicalTransit.sourceLabel
                        + " -> " + _activePhysicalTransit.destLabel;
                case ActiveTransitExecutionPhase::None:
                default:
                    break;
            }
        }

        std::string detail = DescribeScriptedTransitDetail(step);
        if (!step.label.empty() && step.label != detail)
            detail += " | " + step.label;
        return detail;
    }

    if (!step.label.empty())
        return step.label;

    return ResolveStepObjectiveLabel(_session, step);
}

void WorldBotCreatureAI::PersistRuntimeLedgerState(std::string const& detailOverride) const
{
    std::string const detail = detailOverride.empty()
        ? DescribeRuntimeStateDetail()
        : detailOverride;

    GetIdentityRepo().UpdateActiveRuntimeState(
        _identity.id,
        me ? me->GetZoneId() : 0u,
        _worldOnlineMs,
        DescribeRuntimeStateKey(),
        detail);
}

void WorldBotCreatureAI::RecordPositionSnapshot(char const* eventType, std::string const& detail) const
{
    PersistRuntimeLedgerState(detail);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        eventType,
        BuildPositionDetail(me, detail));
}

void WorldBotCreatureAI::ObserveCurrentZoneExploration()
{
    if (!me || _identity.id == 0)
        return;

    std::uint32_t const zoneId = me->GetZoneId();
    if (zoneId == 0)
        return;

    if (!_knownExploredZoneIds.insert(zoneId).second)
        return;

    GetExploredZoneRepo().MarkExplored(_identity.id, zoneId);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "zone_explored",
        "Unlocked taxi knowledge for " + ResolveZoneName(zoneId)
            + " zone_id=" + std::to_string(zoneId));
}

void WorldBotCreatureAI::RecordCombatTrace(std::string const& detail)
{
    if (!me || detail.empty())
        return;

    if (detail == _lastCombatTraceDetail && (_worldOnlineMs - _lastCombatTraceWorldMs) < 2000)
        return;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_trace",
        detail);
    _lastCombatTraceDetail = detail;
    _lastCombatTraceWorldMs = _worldOnlineMs;
}

std::string WorldBotCreatureAI::BuildCombatTraceDetail(
    char const* phase,
    service::BotCombatEvaluationResult const& result,
    Unit* target) const
{
    std::ostringstream oss;
    float const distance = (me && target) ? me->GetDistance(target) : 0.0f;
    std::uint32_t const hostiles10 = CountNearbyHostileUnits(me, 10.0f);
    std::uint32_t const hostiles30 = CountNearbyHostileUnits(me, 30.0f);

    oss << "phase='" << phase << "' "
        << "decision='";

    if (result.disposition == service::BotCombatEvaluationDisposition::Cast && result.action)
    {
        service::BotCombatEvaluatedAction const& action = *result.action;
        oss << "cast' "
            << "entry='" << action.entryLabel << "' "
            << "entry_id=" << action.entryId << " "
            << "action_id=" << action.actionId << " "
            << "action_type=" << static_cast<std::uint32_t>(action.actionType) << " "
            << "spell='" << DescribeCombatActionForTrace(action) << "' "
            << "simulated_item_use=" << (action.simulatedItemUse ? 1 : 0) << " "
            << "target_key='" << action.targetKey << "' "
            << "target='" << (action.target ? action.target->GetName() : "none") << "' "
            << "target_guid=" << (action.target ? action.target->GetGUID().GetCounter() : 0) << " "
            << "aoe_mode='" << DescribeAoEMode(action.aoeMode) << "' "
            << "delivery='" << (action.useDestination ? "destination" : "unit") << "' "
            << "style='" << ((action.aoeMinTargets && *action.aoeMinTargets > 1) ? "aoe" : "single") << "' ";

        if (action.aoeMinTargets)
            oss << "aoe_min_targets=" << static_cast<std::uint32_t>(*action.aoeMinTargets) << " ";
        if (action.aoeRadius)
            oss << "aoe_radius=" << *action.aoeRadius << " ";
        if (action.useDestination)
            oss << "destination=(" << action.destinationX << "," << action.destinationY << "," << action.destinationZ << ") ";
    }
    else if (result.disposition == service::BotCombatEvaluationDisposition::Wait)
    {
        oss << "wait' "
            << "entry='" << result.traceEntryLabel << "' "
            << "entry_id=" << result.traceEntryId << " "
            << "action_id=" << result.traceActionId << " "
            << "reason='" << result.traceReason << "' "
            << "wait_ms=" << result.waitMs << " "
            << "target_key='" << result.traceTargetKey << "' ";
    }
    else
    {
        oss << "none' ";
    }

    oss << "target_hp_pct=" << (target ? target->GetHealthPct() : 0.0f) << " "
        << "self_hp_pct=" << (me ? me->GetHealthPct() : 0.0f) << " "
        << "distance=" << distance << " "
        << "hostiles_10yd=" << hostiles10 << " "
        << "hostiles_30yd=" << hostiles30;
    return oss.str();
}

std::string WorldBotCreatureAI::BuildCombatMovementTraceDetail(
    char const* decision,
    Unit* target) const
{
    std::ostringstream oss;
    float const distance = (me && target) ? me->GetDistance(target) : 0.0f;
    oss << "phase='movement' decision='" << decision << "' "
        << "target='" << (target ? target->GetName() : "none") << "' "
        << "target_guid=" << (target ? target->GetGUID().GetCounter() : 0) << " "
        << "target_hp_pct=" << (target ? target->GetHealthPct() : 0.0f) << " "
        << "self_hp_pct=" << (me ? me->GetHealthPct() : 0.0f) << " "
        << "distance=" << distance << " "
        << "hostiles_10yd=" << CountNearbyHostileUnits(me, 10.0f) << " "
        << "hostiles_30yd=" << CountNearbyHostileUnits(me, 30.0f);
    return oss.str();
}

bool WorldBotCreatureAI::BuildRuntimeSnapshot(RuntimeSnapshot& out) const
{
    if (!_sessionReady || _sessionDone || !me)
        return false;

    out.identity = _identity;
    out.session = _session;
    out.worldOnlineMs = _worldOnlineMs;
    out.progress.currentStep = _currentStep;
    out.progress.stepStartKnown = true;
    out.progress.stepStartMapId = static_cast<std::uint16_t>(me->GetMapId());
    out.progress.stepStartX = me->GetPositionX();
    out.progress.stepStartY = me->GetPositionY();
    out.progress.stepStartZ = me->GetPositionZ();
    out.progress.stepElapsedMs = 0;
    out.inTaxiTransit = _activeTravelExecutionPhase == ActiveTravelExecutionPhase::TaxiTransit;
    out.inPhysicalTransit =
        _currentStep < _session.steps.size()
        && _session.steps[_currentStep].type == service::AmbientStepType::Transit
        && _activeTransitExecutionPhase != ActiveTransitExecutionPhase::None;
    if (out.inPhysicalTransit
        && _activeTransitExecutionPhase == ActiveTransitExecutionPhase::Riding
        && !_activePhysicalTransit.empty()
        && _activePhysicalTransit.sourceMapId != _activePhysicalTransit.destMapId)
    {
        out.physicalTransitTransportEntry = _activePhysicalTransit.transportEntry;
        out.physicalTransitReadyForAbstract = _activityTimer >= CrossMapTransitAbstractMinElapsedMs;

        if (me->GetTransport())
        {
            me->m_movementInfo.transport.pos.GetPosition(
                out.physicalTransitLocalX,
                out.physicalTransitLocalY,
                out.physicalTransitLocalZ,
                out.physicalTransitLocalO);
        }
    }

    if (_currentStep >= _session.steps.size())
        return true;

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type == service::AmbientStepType::Travel && _activeTravelStepStartKnown)
    {
        out.progress.stepStartMapId = _activeTravelStepStartMapId;
        out.progress.stepStartX = _activeTravelStepStartX;
        out.progress.stepStartY = _activeTravelStepStartY;
        out.progress.stepStartZ = _activeTravelStepStartZ;

        if (out.inTaxiTransit && !_activeTaxiJourney.empty())
        {
            out.progress.stepElapsedMs =
                _activeTaxiJourney.sourceGroundPlan.etaMs
                + std::min(_activeTaxiTransitElapsedMs, _activeTaxiJourney.taxiCandidate.route.totalEtaMs);
        }
    }
    else if (step.type != service::AmbientStepType::Travel)
    {
        out.progress.stepElapsedMs = _activityTimer;
    }

    return true;
}

bool WorldBotCreatureAI::TryBuildRouteTravelPlan(
    service::AmbientStep const& step,
    service::WorldBotResolvedTravelPlan& outPlan) const
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;
    if (step.mapId != me->GetMapId())
        return false;

    std::uint32_t const zoneId = ResolveStepZoneId(_session, step);
    if (zoneId == 0)
        return false;

    service::WorldBotTravelCapabilityConfig const capabilityConfig =
        service::LoadWorldBotTravelCapabilityConfig();

    auto const plan = GetWorldBotRoutePlanner().ResolveTravelPlan(
        step.mapId,
        me->GetZoneId(),
        zoneId,
        me->GetPositionX(),
        me->GetPositionY(),
        me->GetPositionZ(),
        step.x,
        step.y,
        step.z,
        ResolveTravelCapabilityTier(),
        capabilityConfig);

    if (!plan || plan->empty())
        return false;

    outPlan = *plan;
    return true;
}

bool WorldBotCreatureAI::TryBuildBestTravelOption(
    service::AmbientStep const& step,
    service::WorldBotResolvedTravelOption& outOption) const
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;
    if (step.mapId != me->GetMapId())
        return false;

    std::uint32_t const zoneId = ResolveStepZoneId(_session, step);
    if (zoneId == 0)
        return false;

    service::WorldBotTravelCapabilityTier const travelTier = ResolveTravelCapabilityTier();
    service::WorldBotTravelCapabilityConfig const capabilityConfig =
        service::LoadWorldBotTravelCapabilityConfig();

    auto const groundResolver =
        [](std::uint16_t mapId,
           std::uint32_t startZoneIdHint,
           std::uint32_t destZoneId,
           float startX,
           float startY,
           float startZ,
           float destX,
           float destY,
           float destZ,
           service::WorldBotTravelCapabilityTier tier,
           service::WorldBotTravelCapabilityConfig const& config)
            -> std::optional<service::WorldBotResolvedTravelPlan>
        {
            return GetWorldBotRoutePlanner().ResolveTravelPlan(
                mapId,
                startZoneIdHint,
                destZoneId,
                startX,
                startY,
                startZ,
                destX,
                destY,
                destZ,
                tier,
                config);
        };

    auto const option = service::ResolveBestTravelOption(
        GetWorldBotTaxiNetwork(),
        groundResolver,
        step.mapId,
        me->GetZoneId(),
        zoneId,
        me->GetPositionX(),
        me->GetPositionY(),
        me->GetPositionZ(),
        step.x,
        step.y,
        step.z,
        _knownExploredZoneIds,
        _identity.faction,
        travelTier,
        capabilityConfig);
    if (!option.has_value())
        return false;

    outOption = *option;
    return true;
}

void WorldBotCreatureAI::ClearActiveRouteTravelPlan()
{
    _routeTravelPlanActive = false;
    _routeTravelWaypointIndex = 0;
    _routeTravelLastZoneId = 0;
    _routeTravelLastReanchorWorldMs = 0;
    _routeTravelPlan = {};
}

void WorldBotCreatureAI::ActivateRouteTravelPlan(service::WorldBotResolvedTravelPlan const& plan)
{
    _routeTravelPlan = plan;
    _routeTravelPlanActive = true;
    _routeTravelWaypointIndex = 0;
    _routeTravelLastZoneId = me ? me->GetZoneId() : 0;
    _routeTravelLastReanchorWorldMs = _worldOnlineMs;
}

void WorldBotCreatureAI::ClearActiveTaxiTravel()
{
    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::None;
    _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
    _activeTaxiTransitElapsedMs = 0;
    _activeTaxiJourney = {};
}

void WorldBotCreatureAI::ClearActivePhysicalTransit()
{
    if (me && me->GetTransport())
        me->GetTransport()->RemovePassenger(me, true);

    _activeTransitExecutionPhase = ActiveTransitExecutionPhase::None;
    _activePhysicalTransit = {};
}

bool WorldBotCreatureAI::BeginActiveTaxiTransit(service::AmbientStep const& step)
{
    if (!me || _activeTaxiJourney.empty())
        return false;

    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::TaxiTransit;
    _activeTaxiTransitElapsedMs = 0;
    _travelWatchdogConfig = {};
    ResetTravelWatchdog(_travelWatchdog);
    ClearVisibleTravelMode();
    ClearActiveRouteTravelPlan();
    me->StopMoving();
    me->GetMotionMaster()->Clear();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "travel_taxi_board",
        BuildTravelNarrative(
            _session,
            step,
            "boarding taxi at " + _activeTaxiJourney.taxiCandidate.sourceNode.name
                + " -> " + _activeTaxiJourney.taxiCandidate.destinationNode.name
                + " eta=" + FormatDurationMs(_activeTaxiJourney.taxiCandidate.route.totalEtaMs)));

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        "Taxi in transit -> " + _activeTaxiJourney.taxiCandidate.sourceNode.name
            + " to " + _activeTaxiJourney.taxiCandidate.destinationNode.name
            + " (" + FormatDurationMs(_activeTaxiJourney.taxiCandidate.route.totalEtaMs) + ")");

    PersistRuntimeLedgerState();

    return true;
}

bool WorldBotCreatureAI::CompleteActiveTaxiTransit(service::AmbientStep const& step)
{
    if (!me || _activeTaxiJourney.empty())
        return false;

    service::WorldBotTaxiNode const& destinationNode = _activeTaxiJourney.taxiCandidate.destinationNode;
    float landingZ = destinationNode.z;
    if (std::fabs(landingZ) <= 0.01f)
        me->UpdateGroundPositionZ(destinationNode.x, destinationNode.y, landingZ);
    me->NearTeleportTo(destinationNode.x, destinationNode.y, landingZ, me->GetOrientation());
    ObserveCurrentZoneExploration();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "travel_taxi_arrive",
        BuildTravelNarrative(
            _session,
            step,
            "landed at " + destinationNode.name
                + " zone=" + std::to_string(destinationNode.zoneId)));

    ActivateRouteTravelPlan(_activeTaxiJourney.destinationGroundPlan);
    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::TaxiFinalLeg;
    _activeTaxiTransitElapsedMs = 0;
    _travelWatchdogConfig = BuildActiveTravelWatchdogConfig(step, ResolveTravelCapabilityTier());
    ApplyVisibleTravelMode(ResolveTravelCapabilityTier());
    MoveToActiveTravelTarget(step);
    ResetTravelWatchdog(_travelWatchdog);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "travel_taxi_resume_ground",
        BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step)));

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        "Taxi complete -> resuming ground travel | "
            + BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step)));

    PersistRuntimeLedgerState();

    return true;
}

bool WorldBotCreatureAI::TryBeginPhysicalTransit(service::AmbientStep const& step)
{
    if (!me || step.type != service::AmbientStepType::Transit)
        return false;
    if (_activeTransitExecutionPhase != ActiveTransitExecutionPhase::None)
        return true;

    auto const routeSpec = ResolvePhysicalTransitRouteSpec(step);
    if (!routeSpec.has_value())
        return false;
    if (step.transitSourcePointKey.empty() || step.transitDestPointKey.empty())
        return false;

    integration::SqlTaskPointRepository pointRepo;
    auto const sourcePoint = pointRepo.FindByKey(step.transitSourcePointKey);
    auto const destPoint = pointRepo.FindByKey(step.transitDestPointKey);
    if (!sourcePoint || !destPoint)
        return false;

    _activePhysicalTransit.routeKey = step.transitRouteKey;
    _activePhysicalTransit.transitType = NormalizeTransitType(step.transitType);
    _activePhysicalTransit.sourceLabel = step.transitSourceLabel.empty() ? sourcePoint->pointName : step.transitSourceLabel;
    _activePhysicalTransit.destLabel = step.transitDestLabel.empty() ? destPoint->pointName : step.transitDestLabel;
    _activePhysicalTransit.transportEntry = routeSpec->transportEntry;
    _activePhysicalTransit.sourceMapId = sourcePoint->mapId;
    _activePhysicalTransit.destMapId = destPoint->mapId;
    _activePhysicalTransit.sourceX = sourcePoint->x;
    _activePhysicalTransit.sourceY = sourcePoint->y;
    _activePhysicalTransit.sourceZ = sourcePoint->z;
    _activePhysicalTransit.destX = destPoint->x;
    _activePhysicalTransit.destY = destPoint->y;
    _activePhysicalTransit.destZ = destPoint->z;
    PhysicalTransitDeckSpot const deckSpot = PickPhysicalTransitDeckSpot(*routeSpec);
    _activePhysicalTransit.boardLocalX = deckSpot.x;
    _activePhysicalTransit.boardLocalY = deckSpot.y;
    _activePhysicalTransit.boardLocalZ = deckSpot.z;
    _activePhysicalTransit.boardLocalO = deckSpot.o;
    _activePhysicalTransit.boardDetectRadius = routeSpec->boardDetectRadius;
    _activePhysicalTransit.boardArriveRadius = routeSpec->boardArriveRadius;
    _activePhysicalTransit.disembarkRadius = routeSpec->disembarkRadius;
    _activePhysicalTransit.activeTransportGuid.Clear();
    _activeTransitExecutionPhase = ActiveTransitExecutionPhase::WaitingForTransport;

    ClearVisibleTravelMode();
    me->StopMoving();
    me->GetMotionMaster()->Clear();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "travel_transit_wait",
        BuildTravelNarrative(
            _session,
            step,
            "waiting for " + _activePhysicalTransit.sourceLabel
                + " -> " + _activePhysicalTransit.destLabel
                + " transport"));

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        "Waiting for physical transit -> " + DescribeRuntimeStateDetail());

    PersistRuntimeLedgerState();
    return true;
}

bool WorldBotCreatureAI::TryResumePhysicalTransit(service::AmbientStep const& step)
{
    if (!me || step.type != service::AmbientStepType::Transit)
        return false;
    if (_activeTransitExecutionPhase != ActiveTransitExecutionPhase::None)
        return true;

    Transport* transport = me->GetTransport();
    if (!transport)
        return false;

    auto const routeSpec = ResolvePhysicalTransitRouteSpec(step);
    if (!routeSpec.has_value() || transport->GetEntry() != routeSpec->transportEntry)
        return false;

    if (step.transitSourcePointKey.empty() || step.transitDestPointKey.empty())
        return false;

    integration::SqlTaskPointRepository pointRepo;
    auto const sourcePoint = pointRepo.FindByKey(step.transitSourcePointKey);
    auto const destPoint = pointRepo.FindByKey(step.transitDestPointKey);
    if (!sourcePoint || !destPoint)
        return false;

    _activePhysicalTransit.routeKey = step.transitRouteKey;
    _activePhysicalTransit.transitType = NormalizeTransitType(step.transitType);
    _activePhysicalTransit.sourceLabel = step.transitSourceLabel.empty() ? sourcePoint->pointName : step.transitSourceLabel;
    _activePhysicalTransit.destLabel = step.transitDestLabel.empty() ? destPoint->pointName : step.transitDestLabel;
    _activePhysicalTransit.transportEntry = routeSpec->transportEntry;
    _activePhysicalTransit.sourceMapId = sourcePoint->mapId;
    _activePhysicalTransit.destMapId = destPoint->mapId;
    _activePhysicalTransit.sourceX = sourcePoint->x;
    _activePhysicalTransit.sourceY = sourcePoint->y;
    _activePhysicalTransit.sourceZ = sourcePoint->z;
    _activePhysicalTransit.destX = destPoint->x;
    _activePhysicalTransit.destY = destPoint->y;
    _activePhysicalTransit.destZ = destPoint->z;
    _activePhysicalTransit.boardDetectRadius = routeSpec->boardDetectRadius;
    _activePhysicalTransit.boardArriveRadius = routeSpec->boardArriveRadius;
    _activePhysicalTransit.disembarkRadius = routeSpec->disembarkRadius;
    _activePhysicalTransit.activeTransportGuid = transport->GetGUID();
    me->m_movementInfo.transport.pos.GetPosition(
        _activePhysicalTransit.boardLocalX,
        _activePhysicalTransit.boardLocalY,
        _activePhysicalTransit.boardLocalZ,
        _activePhysicalTransit.boardLocalO);
    _activeTransitExecutionPhase = ActiveTransitExecutionPhase::Riding;
    PersistRuntimeLedgerState();
    return true;
}

bool WorldBotCreatureAI::TickPhysicalTransit(service::AmbientStep const& step)
{
    if (!me || _activePhysicalTransit.empty())
        return false;

    auto findTransport =
        [this]() -> Transport*
        {
            Map* map = me ? me->GetMap() : nullptr;
            if (!map)
                return nullptr;

            if (!_activePhysicalTransit.activeTransportGuid.IsEmpty())
            {
                if (Transport* transport = map->GetTransport(_activePhysicalTransit.activeTransportGuid))
                {
                    if (transport->GetEntry() == _activePhysicalTransit.transportEntry)
                        return transport;
                }
            }

            for (Transport* transport : map->GetAllTransports())
            {
                if (!transport
                    || !transport->IsInWorld()
                    || transport->GetEntry() != _activePhysicalTransit.transportEntry)
                {
                    continue;
                }

                _activePhysicalTransit.activeTransportGuid = transport->GetGUID();
                return transport;
            }

            return nullptr;
        };

    if (_activeTransitExecutionPhase == ActiveTransitExecutionPhase::WaitingForTransport
        || _activeTransitExecutionPhase == ActiveTransitExecutionPhase::Boarding)
    {
        Transport* transport = findTransport();
        if (!transport)
            return true;

        if (_activeTransitExecutionPhase == ActiveTransitExecutionPhase::WaitingForTransport)
        {
            if (transport->GetMapId() != _activePhysicalTransit.sourceMapId)
                return true;

            float const sourceDist = transport->GetDistance(
                _activePhysicalTransit.sourceX,
                _activePhysicalTransit.sourceY,
                _activePhysicalTransit.sourceZ);
            if (sourceDist > _activePhysicalTransit.boardDetectRadius)
                return true;
        }

        float boardX = _activePhysicalTransit.boardLocalX;
        float boardY = _activePhysicalTransit.boardLocalY;
        float boardZ = _activePhysicalTransit.boardLocalZ;
        float boardO = _activePhysicalTransit.boardLocalO;
        transport->CalculatePassengerPosition(boardX, boardY, boardZ, &boardO);

        _activeTransitExecutionPhase = ActiveTransitExecutionPhase::Boarding;
        if (me->GetDistance(boardX, boardY, boardZ) > _activePhysicalTransit.boardArriveRadius)
        {
            me->GetMotionMaster()->MovePoint(
                static_cast<uint32>(_currentStep),
                boardX,
                boardY,
                boardZ);
            return true;
        }

        me->NearTeleportTo(boardX, boardY, boardZ, boardO);
        transport->AddPassenger(me, true);
        me->StopMovingOnCurrentPos();
        _activeTransitExecutionPhase = ActiveTransitExecutionPhase::Riding;

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "travel_transit_board",
            BuildTravelNarrative(
                _session,
                step,
                "boarded " + _activePhysicalTransit.transitType
                    + " " + _activePhysicalTransit.sourceLabel
                    + " -> " + _activePhysicalTransit.destLabel));
        PersistRuntimeLedgerState();
        return true;
    }

    if (_activeTransitExecutionPhase != ActiveTransitExecutionPhase::Riding)
        return false;

    Transport* transport = me->GetTransport();
    if (!transport)
        transport = findTransport();
    if (!transport)
        return true;

    float const destDist = transport->GetDistance(
        _activePhysicalTransit.destX,
        _activePhysicalTransit.destY,
        _activePhysicalTransit.destZ);
    if (destDist > _activePhysicalTransit.disembarkRadius)
        return true;

    if (me->GetTransport())
        me->GetTransport()->RemovePassenger(me, true);

    me->NearTeleportTo(
        _activePhysicalTransit.destX,
        _activePhysicalTransit.destY,
        _activePhysicalTransit.destZ,
        me->GetOrientation());
    ObserveCurrentZoneExploration();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "travel_transit_arrive",
        BuildTravelNarrative(
            _session,
            step,
            "arrived by " + _activePhysicalTransit.transitType
                + " at " + _activePhysicalTransit.destLabel));

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        "Transit complete -> " + DescribeNextTask(_session, _currentStep + 1));

    _activityTimer = 0;
    ClearActivePhysicalTransit();
    AdvanceStep();
    return true;
}

void WorldBotCreatureAI::MoveToActiveTravelTarget(service::AmbientStep const& step)
{
    float targetX = step.x;
    float targetY = step.y;
    float targetZ = step.z;

    if (_routeTravelPlanActive && _routeTravelWaypointIndex < _routeTravelPlan.waypoints.size())
    {
        service::WorldBotRouteWaypoint const& waypoint =
            _routeTravelPlan.waypoints[_routeTravelWaypointIndex];
        targetX = waypoint.x;
        targetY = waypoint.y;
        targetZ = waypoint.z;
    }

    // Most route points should carry a baked ground Z already. Only fall back to
    // live terrain sampling when the stored height is clearly missing/placeholder.
    if (std::fabs(targetZ) <= 0.01f)
        me->UpdateGroundPositionZ(targetX, targetY, targetZ);

    me->GetMotionMaster()->MovePoint(
        static_cast<uint32>(_currentStep),
        targetX,
        targetY,
        targetZ);
}

float WorldBotCreatureAI::GetActiveTravelTargetDistance(service::AmbientStep const& step) const
{
    if (!me)
        return std::numeric_limits<float>::max();

    if (_routeTravelPlanActive && _routeTravelWaypointIndex < _routeTravelPlan.waypoints.size())
    {
        service::WorldBotRouteWaypoint const& waypoint =
            _routeTravelPlan.waypoints[_routeTravelWaypointIndex];
        return me->GetDistance(waypoint.x, waypoint.y, waypoint.z);
    }

    return me->GetDistance(step.x, step.y, step.z);
}

bool WorldBotCreatureAI::AdvanceAlongActiveRouteTravelPlan()
{
    if (!_routeTravelPlanActive)
        return false;

    if ((_routeTravelWaypointIndex + 1u) >= _routeTravelPlan.waypoints.size())
        return false;

    ++_routeTravelWaypointIndex;
    return true;
}

bool WorldBotCreatureAI::TryReanchorActiveRouteTravelPlan(service::AmbientStep const& step, char const* reason)
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;
    if ((_worldOnlineMs - _routeTravelLastReanchorWorldMs) < 2000)
        return false;

    service::WorldBotResolvedTravelPlan replanned;
    if (!TryBuildRouteTravelPlan(step, replanned) || replanned.empty())
        return false;

    _routeTravelPlan = std::move(replanned);
    _routeTravelPlanActive = true;
    _routeTravelWaypointIndex = 0;
    _routeTravelLastZoneId = me->GetZoneId();
    _routeTravelLastReanchorWorldMs = _worldOnlineMs;
    ResetTravelWatchdog(_travelWatchdog);
    MoveToActiveTravelTarget(step);

    integration::BotActivityLog::Record(
        me, _identity.name, _identity.id,
        "travel_reanchor",
        BuildTravelNarrative(
            _session,
            step,
            std::string(reason)
                + " -> nearest node found"
                + " attach_yd=" + std::to_string(_routeTravelPlan.attachDistanceYards)
                + " route_yd=" + std::to_string(_routeTravelPlan.routeDistanceYards)
                + " final_leg_yd=" + std::to_string(_routeTravelPlan.detachDistanceYards)
                + " total_yd=" + std::to_string(_routeTravelPlan.totalDistanceYards)
                + " eta=" + FormatDurationMs(_routeTravelPlan.etaMs)
                + " waypoints=" + std::to_string(_routeTravelPlan.waypoints.size())));

    return true;
}

std::string WorldBotCreatureAI::DescribeActiveTravelTarget(service::AmbientStep const& step) const
{
    std::ostringstream oss;
    if (_activeTravelExecutionPhase == ActiveTravelExecutionPhase::TaxiTransit && !_activeTaxiJourney.empty())
    {
        std::uint32_t const rideEtaMs = _activeTaxiJourney.taxiCandidate.route.totalEtaMs;
        std::uint32_t const remainingMs = rideEtaMs > _activeTaxiTransitElapsedMs
            ? (rideEtaMs - _activeTaxiTransitElapsedMs)
            : 0u;
        oss << "taxi transit " << _activeTaxiJourney.taxiCandidate.sourceNode.name
            << " -> " << _activeTaxiJourney.taxiCandidate.destinationNode.name
            << " remaining=" << FormatDurationMs(remainingMs);
        return oss.str();
    }

    if (_activeTravelExecutionPhase == ActiveTravelExecutionPhase::TaxiApproach && !_activeTaxiJourney.empty())
    {
        oss << "heading to flight master " << _activeTaxiJourney.taxiCandidate.sourceNode.name << " | ";
    }
    else if (_activeTravelExecutionPhase == ActiveTravelExecutionPhase::TaxiFinalLeg && !_activeTaxiJourney.empty())
    {
        oss << "after taxi from " << _activeTaxiJourney.taxiCandidate.sourceNode.name
            << " -> " << _activeTaxiJourney.taxiCandidate.destinationNode.name << " | ";
    }

    if (_routeTravelPlanActive && _routeTravelWaypointIndex < _routeTravelPlan.waypoints.size())
    {
        service::WorldBotRouteWaypoint const& waypoint =
            _routeTravelPlan.waypoints[_routeTravelWaypointIndex];
        oss << "next node " << (_routeTravelWaypointIndex + 1u) << "/" << _routeTravelPlan.waypoints.size()
            << " target=(" << waypoint.x << "," << waypoint.y << "," << waypoint.z << ")"
            << " total_distance_yd=" << _routeTravelPlan.totalDistanceYards
            << " speed_ydps=" << _routeTravelPlan.speedYardsPerSecond
            << " eta=" << FormatDurationMs(_routeTravelPlan.etaMs);
        return oss.str();
    }

    oss << "direct target=(" << step.x << "," << step.y << "," << step.z << ")";
    return oss.str();
}

ai::TravelWatchdogConfig WorldBotCreatureAI::BuildActiveTravelWatchdogConfig(
    service::AmbientStep const& step,
    service::WorldBotTravelCapabilityTier tier) const
{
    ai::TravelWatchdogConfig config = kDefaultTravelWatchdogConfig;

    std::uint32_t budgetMs = config.timeoutMs;
    if (_routeTravelPlanActive && _routeTravelPlan.etaMs > 0)
    {
        float const paddedMs = (static_cast<float>(_routeTravelPlan.etaMs) * 1.75f) + 60000.0f;
        budgetMs = static_cast<std::uint32_t>(std::clamp(paddedMs, 120000.0f, 1800000.0f));
    }
    else if (me)
    {
        service::WorldBotTravelCapabilityConfig const capabilityConfig =
            service::LoadWorldBotTravelCapabilityConfig();
        float const speed = std::max(
            0.1f,
            service::ResolveWorldBotTravelSpeedYardsPerSecond(tier, capabilityConfig));
        float const directDistance = me->GetDistance(step.x, step.y, step.z);
        float const paddedMs = ((directDistance / speed) * 1000.0f * 1.75f) + 60000.0f;
        budgetMs = static_cast<std::uint32_t>(std::clamp(paddedMs, 120000.0f, 1800000.0f));
    }

    config.timeoutMs = budgetMs;
    config.stagnantLimitMs = std::clamp<std::uint32_t>(budgetMs / 8u, 10000u, 45000u);
    return config;
}

service::WorldBotTravelCapabilityTier WorldBotCreatureAI::ResolveTravelCapabilityTier() const
{
    service::WorldBotTravelCapabilityPolicy const capabilityPolicy =
        service::LoadWorldBotTravelCapabilityPolicy();

    return service::ResolveWorldBotTravelCapabilityTierForLevel(
        static_cast<std::uint8_t>(_identity.level),
        false,
        capabilityPolicy);
}

void WorldBotCreatureAI::ApplyVisibleTravelMode(service::WorldBotTravelCapabilityTier tier)
{
    if (!me)
        return;

    ClearVisibleTravelMode();

    service::WorldBotTravelCapabilityConfig const capabilityConfig =
        service::LoadWorldBotTravelCapabilityConfig();
    float const footSpeed = std::max(0.001f, capabilityConfig.footYardsPerSecond);
    float const targetSpeed = service::ResolveWorldBotTravelSpeedYardsPerSecond(tier, capabilityConfig);
    float const speedRate = std::max(1.0f, targetSpeed / footSpeed);
    std::uint32_t const spellId =
        service::WorldBotPreparationService::ResolvePreferredTravelMobilitySpellId(_identity, tier);

    if (spellId != 0)
        me->CastSpell(me, spellId, true);

    me->SetSpeed(MOVE_RUN, speedRate, true);

    _visibleTravelModeActive = (tier != service::WorldBotTravelCapabilityTier::Foot)
        || spellId != 0
        || speedRate > 1.0f;
    _visibleTravelModeSpellId = spellId;
    _visibleTravelCapabilityTier = tier;
    _visibleTravelSpeedRate = speedRate;
}

void WorldBotCreatureAI::ClearVisibleTravelMode()
{
    if (!me)
        return;

    if (me->HasMountedAura())
        me->RemoveAurasByType(SPELL_AURA_MOUNTED);
    else if (me->IsMounted())
        me->Dismount();

    if (me->HasShapeshiftAura())
        me->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

    me->SetSpeed(MOVE_RUN, 1.0f, true);

    _visibleTravelModeActive = false;
    _visibleTravelModeSpellId = 0;
    _visibleTravelCapabilityTier = service::WorldBotTravelCapabilityTier::Foot;
    _visibleTravelSpeedRate = 1.0f;
}

void WorldBotCreatureAI::JustEngagedWith(Unit* who)
{
    SuspendCurrentStepForCombat(who);
}

void WorldBotCreatureAI::JustReachedHome()
{
    if (_combatSuspendedStep && !me->IsInCombat() && !me->GetVictim())
        ResumeSuspendedStepAfterCombat();
}

std::string WorldBotCreatureAI::DescribeCurrentStep() const
{
    if (_currentStep >= _session.steps.size())
        return "session_complete";

    return _session.steps[_currentStep].label;
}

void WorldBotCreatureAI::InvalidateCombatProfile()
{
    _combatPreparedProfile = {};
    _combatProfilePrepared = false;
}

void WorldBotCreatureAI::ResetGatherState()
{
    _gatherTargetGuid.Clear();
    _gatherMovingToNode = false;
    _gatherCompletedCycles = 0;
}

void WorldBotCreatureAI::EnsureCombatProfile()
{
    if (_combatProfilePrepared || !_sessionReady || !me)
        return;

    if (!_preparedBuildReady)
        return;

    _combatPreparedProfile = GetProfilePreparationService().PrepareForWorldBot(
        me,
        _preparedBuild.knownSpellIds,
        _preparedBuild.canonicalSpecKey,
        _preparedBuild.resolvedRoleKey,
        _preparedBuild.contextKey);
    _combatProfilePrepared = true;
}

bool WorldBotCreatureAI::IsDebugCombatManaDrainIdentity() const
{
    if (_identity.id == 0)
        return false;

    std::uint32_t const drainIdentityId =
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugCombatManaDrainIdentityId", 0);
    return drainIdentityId != 0 && drainIdentityId == _identity.id;
}

bool WorldBotCreatureAI::ApplyDebugCombatManaTarget(Unit* target, char const* traceDecision, bool logAttempt)
{
    if (!me || _debugCombatManaGemObserved || !IsDebugCombatManaDrainIdentity())
        return false;

    std::uint32_t const maxMana = me->GetMaxPower(POWER_MANA);
    std::uint32_t const targetManaPct = std::clamp<std::uint32_t>(
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugCombatManaDrainTargetManaPct", 60),
        0u,
        99u);
    std::uint32_t const currentMana = me->GetPower(POWER_MANA);

    auto const recordAttemptTrace =
        [&](char const* reason, bool applied, std::uint32_t targetMana)
        {
            if (!logAttempt || !traceDecision || !*traceDecision)
                return;

            std::ostringstream oss;
            oss << "phase='movement' decision='" << traceDecision << "' "
                << "reason='" << (reason ? reason : "none") << "' "
                << "applied=" << (applied ? 1 : 0) << " "
                << "current_mana=" << currentMana << " "
                << "max_mana=" << maxMana << " "
                << "target_mana_pct=" << targetManaPct << " "
                << "target_mana=" << targetMana << " "
                << "target='" << (target ? target->GetName() : "none") << "' "
                << "target_guid=" << (target ? target->GetGUID().GetCounter() : 0);
            RecordCombatTrace(oss.str());
        };

    if (maxMana == 0)
    {
        recordAttemptTrace("max_mana_zero", false, 0);
        return false;
    }

    std::uint32_t const targetMana = (maxMana * targetManaPct) / 100u;
    if (currentMana <= targetMana)
    {
        recordAttemptTrace("already_at_or_below_target", false, targetMana);
        return false;
    }

    me->SetPower(POWER_MANA, targetMana);
    _lastDebugCombatManaDrainWorldMs = _worldOnlineMs;

    recordAttemptTrace("applied", true, targetMana);

    if (!logAttempt && traceDecision && *traceDecision)
        RecordCombatTrace(BuildCombatMovementTraceDetail(traceDecision, target));

    return true;
}

void WorldBotCreatureAI::SuspendCurrentStepForCombat(Unit* target)
{
    if (_combatSuspendedStep || !_sessionReady || _sessionDone)
        return;

    _combatSuspendedStep = true;
    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    _lastDebugCombatManaDrainWorldMs = 0;
    _debugCombatManaGemObserved = false;
    _usedSimulatedItemsThisCombat.clear();
    if (_traveling || _gatherMovingToNode)
    {
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        _traveling = false;
        ClearVisibleTravelMode();
        ClearActiveRouteTravelPlan();
        ClearActiveTaxiTravel();
        _gatherMovingToNode = false;
        ResetTravelWatchdog(_travelWatchdog);
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_enter",
        "step='" + DescribeCurrentStep()
            + "' target_guid=" + std::to_string(target ? target->GetGUID().GetCounter() : 0));

    ApplyDebugCombatManaTarget(target, "debug_mana_force_combat_enter", true);
}

void WorldBotCreatureAI::ResumeSuspendedStepAfterCombat()
{
    if (!_combatSuspendedStep)
        return;

    _combatSuspendedStep = false;
    _traveling = false;
    ClearVisibleTravelMode();
    ClearActiveRouteTravelPlan();
    ClearActiveTaxiTravel();
    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    _lastDebugCombatManaDrainWorldMs = 0;
    _debugCombatManaGemObserved = false;
    _usedSimulatedItemsThisCombat.clear();
    ResetTravelWatchdog(_travelWatchdog);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_exit",
        "resume_step='" + DescribeCurrentStep() + "'");
}

GameObject* WorldBotCreatureAI::ResolveGatherTarget() const
{
    if (!_gatherTargetGuid || !me)
        return nullptr;

    return ObjectAccessor::GetGameObject(*me, _gatherTargetGuid);
}

bool WorldBotCreatureAI::IsGatherNodeForStep(
    GameObject const* go,
    service::AmbientStep const& step) const
{
    if (!go || !go->GetGOInfo() || !go->isSpawned())
        return false;

    SkillType requiredSkill = SKILL_NONE;
    switch (step.type)
    {
        case service::AmbientStepType::GatherHerb:
            requiredSkill = SKILL_HERBALISM;
            break;
        case service::AmbientStepType::GatherOre:
            requiredSkill = SKILL_MINING;
            break;
        default:
            return false;
    }

    std::uint32_t const lockId = go->GetGOInfo()->GetLockId();
    if (lockId == 0)
        return false;

    LockEntry const* lock = sLockStore.LookupEntry(lockId);
    if (!lock)
        return false;

    for (std::size_t i = 0; i < MAX_LOCK_CASE; ++i)
    {
        if (lock->Type[i] != LOCK_KEY_SKILL)
            continue;

        if (SkillByLockType(static_cast<LockType>(lock->Index[i])) == requiredSkill)
            return true;
    }

    return false;
}

GameObject* WorldBotCreatureAI::FindNearestGatherNode(service::AmbientStep const& step) const
{
    if (!me)
        return nullptr;

    SkillType requiredSkill = SKILL_NONE;
    switch (step.type)
    {
        case service::AmbientStepType::GatherHerb:
            requiredSkill = SKILL_HERBALISM;
            break;
        case service::AmbientStepType::GatherOre:
            requiredSkill = SKILL_MINING;
            break;
        default:
            return nullptr;
    }

    GameObject* result = nullptr;
    float bestDistance = GatherSearchRadius;

    auto workerFn =
        [&](GameObject* go)
        {
            if (!go || !go->GetGOInfo() || !go->isSpawned())
                return;

            if (!IsGatherNodeForStep(go, step))
                return;

            float const dist = me->GetDistance(go);
            if (dist > bestDistance)
                return;

            bestDistance = dist;
            result = go;
        };

    Acore::GameObjectWorker<decltype(workerFn)> worker(me, workerFn);
    Cell::VisitObjects(me, worker, GatherSearchRadius);
    return result;
}

void WorldBotCreatureAI::TickGatherStep(service::AmbientStep const& step)
{
    if (!me)
        return;

    if (_activityTimer == 0)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activity_start",
            step.label + " subject_kind='" + step.subjectKind + "'");
    }

    _activityTimer += TickIntervalMs;
    std::uint32_t const durationMs = std::max<std::uint32_t>(1000u, step.durationSec * 1000u);
    std::uint8_t const requiredCycles = std::max<std::uint8_t>(1u, step.cycleCount);

    if (_gatherCompletedCycles >= requiredCycles || _activityTimer >= durationMs)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activity_complete",
            step.label + " cycles=" + std::to_string(_gatherCompletedCycles)
                + "/" + std::to_string(requiredCycles));
        _activityTimer = 0;
        AdvanceStep();
        return;
    }

    GameObject* target = ResolveGatherTarget();
    if (target && !IsGatherNodeForStep(target, step))
    {
        _gatherTargetGuid.Clear();
        _gatherMovingToNode = false;
        target = nullptr;
    }

    if (!target)
    {
        target = FindNearestGatherNode(step);
        if (target)
        {
            _gatherTargetGuid = target->GetGUID();
            _gatherMovingToNode = false;
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "gather_target_acquired",
                step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter()));
        }
    }

    if (!target)
    {
        if ((_activityTimer % 60000) < TickIntervalMs)
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "activity_tick",
                step.label + " waiting_for_node cycles=" + std::to_string(_gatherCompletedCycles));
        }

        if (!_gatherMovingToNode && me->GetDistance(step.x, step.y, step.z) > ArrivalThreshold)
        {
            me->GetMotionMaster()->MovePoint(static_cast<uint32>(_currentStep), step.x, step.y, step.z);
            _gatherMovingToNode = true;
        }
        return;
    }

    float const dist = me->GetDistance(target);
    if (dist > GatherInteractRange)
    {
        if (!_gatherMovingToNode)
        {
            me->GetMotionMaster()->MovePoint(
                static_cast<uint32>(_currentStep),
                target->GetPositionX(),
                target->GetPositionY(),
                target->GetPositionZ());
            _gatherMovingToNode = true;
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "gather_move_start",
                step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter()));
        }
        return;
    }

    me->StopMoving();
    me->GetMotionMaster()->Clear();
    _gatherMovingToNode = false;
    target->SetLootState(GO_JUST_DEACTIVATED, me);
    ++_gatherCompletedCycles;
    _gatherTargetGuid.Clear();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "gather_interact",
        step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " cycles=" + std::to_string(_gatherCompletedCycles)
            + "/" + std::to_string(requiredCycles));
}

void WorldBotCreatureAI::TickCombat(uint32 /*diff*/)
{
    if (!me)
        return;

    SuspendCurrentStepForCombat(me->GetVictim());

    if (!UpdateVictim())
        return;

    Unit* target = me->GetVictim();
    if (!target)
        return;

    model::WorldBotHazardSnapshot const hazard = BuildWorldBotHazardSnapshot(me, _hazardEvaluationState);
    std::vector<service::WorldBotNearbyHostileSnapshot> const nearbyHostiles =
        CollectNearbyHostileSnapshots(me, target, 30.0f);
    model::WorldBotCombatSituation const situation = service::BuildWorldBotCombatSituation(
        _preparedBuild,
        true,
        me->GetDistance(target),
        me->GetHealthPct(),
        GetManaPct(me),
        CountNearbyHostileUnits(me, 10.0f),
        hazard);
    model::WorldBotMovementDecision const movementDecision =
        service::EvaluateWorldBotMovementDoctrine(situation);
    service::WorldBotMovementExecutionPlan const movementPlan =
        service::BuildWorldBotMovementExecutionPlan(
            situation,
            movementDecision,
            me->GetPositionX(),
            me->GetPositionY(),
            me->GetPositionZ(),
            target->GetPositionX(),
            target->GetPositionY(),
            target->GetPositionZ(),
            nearbyHostiles);

    auto const recordMovementDoctrineTrace =
        [&]()
        {
            std::ostringstream movementTrace;
            movementTrace << BuildCombatMovementTraceDetail("movement_doctrine", target)
                << " style='" << DescribeMovementStyle(situation.movementStyle) << "'"
                << " posture='" << DescribeCombatPosture(movementDecision.posture) << "'"
                << " source='" << DescribeMovementDecisionSource(movementDecision.source) << "'"
                << " plan='" << DescribeMovementPlanKind(movementPlan.kind) << "'"
                << " hard_casts=" << (movementDecision.allowHardCasts ? 1 : 0)
                << " cast_safe=" << (situation.canCastSafely ? 1 : 0)
                << " hazard_active=" << (situation.hazard.active ? 1 : 0)
                << " hazard_aura=" << (situation.hazard.explicitAuraTriggered ? 1 : 0)
                << " hazard_repeat=" << (situation.hazard.repeatedDamageTriggered ? 1 : 0)
                << " hazard_commit=" << (situation.hazard.commitWindowActive ? 1 : 0)
                << " hazard_spell=" << situation.hazard.hazardSpellId
                << " hazard_consec=" << situation.hazard.consecutiveDamageTicks
                << " move_score=" << movementPlan.safetyScore
                << " move_fresh_hostiles=" << movementPlan.freshHostilesNearDestination
                << " move_engaged_hostiles=" << movementPlan.engagedHostilesNearDestination;

            switch (movementPlan.kind)
            {
                case service::WorldBotMovementPlanKind::MovePoint:
                    movementTrace << " destination=(" << movementPlan.pointX << "," << movementPlan.pointY << "," << movementPlan.pointZ << ")";
                    break;
                case service::WorldBotMovementPlanKind::Chase:
                    movementTrace << " chase_distance=" << movementPlan.chaseDistance;
                    break;
                case service::WorldBotMovementPlanKind::None:
                default:
                    break;
            }

            RecordCombatTrace(movementTrace.str());
        };

    if (movementDecision.source == model::WorldBotMovementDecisionSource::HazardOverride)
    {
        if (me->IsNonMeleeSpellCast(false) && !movementDecision.allowHardCasts)
            me->InterruptNonMeleeSpells(false);

        recordMovementDoctrineTrace();

        switch (movementPlan.kind)
        {
            case service::WorldBotMovementPlanKind::MovePoint:
                me->GetMotionMaster()->MovePoint(0, movementPlan.pointX, movementPlan.pointY, movementPlan.pointZ);
                break;
            case service::WorldBotMovementPlanKind::Chase:
                me->GetMotionMaster()->MoveChase(target, movementPlan.chaseDistance);
                break;
            case service::WorldBotMovementPlanKind::None:
            default:
                break;
        }

        return;
    }

    EnsureCombatProfile();
    MaybeApplyDebugCombatManaDrain(target);

    bool acted = false;
    if (!_combatPreparedProfile.interruptEntries.empty() || !_combatPreparedProfile.rotationEntries.empty())
    {
        service::BotCombatRuntimeContext context;
        context.bot = me;
        context.owner = nullptr;
        context.primaryTarget = target;
        context.allowHardCasts = movementDecision.allowHardCasts;
        context.usedSimulatedItemsThisCombat = &_usedSimulatedItemsThisCombat;
        context.rotationWaitMs = _combatPreparedProfile.resolution.profile.settings.rotationWaitMs;
        context.defaultAoEMode = _combatPreparedProfile.resolution.profile.settings.defaultAoEMode;
        context.defaultAoEMinTargets = _combatPreparedProfile.resolution.profile.settings.defaultAoEMinTargets;
        context.defaultAoEScanRadius = _combatPreparedProfile.resolution.profile.settings.defaultAoEScanRadius;
        context.availableSpells = _combatPreparedProfile.availableSpells;

        auto const tryResult =
            [&](service::BotCombatEvaluationResult const& result) -> bool
            {
                if (result.disposition != service::BotCombatEvaluationDisposition::Cast || !result.action)
                    return result.disposition == service::BotCombatEvaluationDisposition::Wait;

                service::BotCombatEvaluatedAction const& action = *result.action;
                if (action.breaksCurrentCast && me->IsNonMeleeSpellCast(false))
                    me->InterruptNonMeleeSpells(false);

                bool casted = service::CastEvaluatedAction(me, action);

                if (casted && action.actionType == model::BotCombatActionType::Spell)
                {
                    if (Creature* creature = me->ToCreature())
                    {
                        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(action.spellId))
                        {
                            std::uint32_t const cooldownMs = std::max<std::uint32_t>(
                                spellInfo->RecoveryTime,
                                spellInfo->CategoryRecoveryTime);
                            if (cooldownMs > 0 && !creature->HasSpellCooldown(action.spellId))
                                creature->AddSpellCooldown(action.spellId, 0, cooldownMs);
                        }
                    }
                }

                if (casted
                    && action.actionType == model::BotCombatActionType::Item)
                {
                    if (action.simulatedItemUse)
                        _usedSimulatedItemsThisCombat.insert(action.itemId);

                    if (action.itemId == DebugManaGemItemId)
                    {
                        _debugCombatManaGemObserved = true;
                        RecordCombatTrace(BuildCombatMovementTraceDetail("debug_mana_gem_observed", target));
                    }
                }

                return casted;
            };

        service::BotCombatEvaluationResult const interruptResult =
            GetRuntimeEvaluator().EvaluateInterrupts(_combatPreparedProfile, context);
        if (interruptResult.disposition != service::BotCombatEvaluationDisposition::None)
            RecordCombatTrace(BuildCombatTraceDetail("interrupt", interruptResult, target));

        acted = tryResult(interruptResult);
        if (!acted)
        {
            service::BotCombatEvaluationResult const rotationResult =
                GetRuntimeEvaluator().EvaluateRotation(_combatPreparedProfile, context);
            if (rotationResult.disposition != service::BotCombatEvaluationDisposition::None)
                RecordCombatTrace(BuildCombatTraceDetail("rotation", rotationResult, target));
            acted = tryResult(rotationResult);
        }
    }
    else
    {
        RecordCombatTrace(BuildCombatMovementTraceDetail("no_prepared_entries", target));
    }

    if (!acted)
    {
        if (movementPlan.kind != service::WorldBotMovementPlanKind::None && me->IsNonMeleeSpellCast(false) && !movementDecision.allowHardCasts)
            me->InterruptNonMeleeSpells(false);

        switch (movementPlan.kind)
        {
            case service::WorldBotMovementPlanKind::MovePoint:
                recordMovementDoctrineTrace();
                me->GetMotionMaster()->MovePoint(0, movementPlan.pointX, movementPlan.pointY, movementPlan.pointZ);
                break;
            case service::WorldBotMovementPlanKind::Chase:
                recordMovementDoctrineTrace();
                me->GetMotionMaster()->MoveChase(target, movementPlan.chaseDistance);
                break;
            case service::WorldBotMovementPlanKind::None:
            default:
                recordMovementDoctrineTrace();
                break;
        }
    }

    DoMeleeAttackIfReady();
}

void WorldBotCreatureAI::MaybeApplyDebugCombatManaDrain(Unit* target)
{
    std::uint32_t const intervalMs = std::max<std::uint32_t>(
        250u,
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugCombatManaDrainIntervalMs", 1500));
    if ((_worldOnlineMs - _lastDebugCombatManaDrainWorldMs) < intervalMs)
        return;

    ApplyDebugCombatManaTarget(target, "debug_mana_drain");
}

void WorldBotCreatureAI::TickStep(uint32 /*diff*/)
{
    if (_currentStep >= _session.steps.size())
    {
        CompletSession();
        return;
    }

    service::AmbientStep const& step = _session.steps[_currentStep];

    if (step.type == service::AmbientStepType::Travel)
    {
        if (!_traveling)
        {
            ClearActiveRouteTravelPlan();
            ClearActiveTaxiTravel();
            _activeTravelStepStartKnown = true;
            _activeTravelStepStartMapId = static_cast<std::uint16_t>(me->GetMapId());
            _activeTravelStepStartX = me->GetPositionX();
            _activeTravelStepStartY = me->GetPositionY();
            _activeTravelStepStartZ = me->GetPositionZ();
            service::WorldBotTravelCapabilityTier const travelTier = ResolveTravelCapabilityTier();
            // Same-map only for now — skip cross-map travel steps.
            if (step.mapId != me->GetMapId())
            {
                LOG_INFO("server.worldserver",
                    "[WorldBotAI] bot='{}' identity={} skipping cross-map travel step={} "
                    "step_map={} bot_map={}",
                    _identity.name, _identity.id,
                    _currentStep, step.mapId, me->GetMapId());
                AdvanceStep();
                return;
            }

            service::WorldBotResolvedTravelOption travelOption;
            if (TryBuildBestTravelOption(step, travelOption))
            {
                _activeTravelOptionMode = travelOption.mode;
                if (travelOption.usesTaxi()
                    && travelOption.taxiJourney.has_value()
                    && !travelOption.taxiJourney->empty())
                {
                    _activeTaxiJourney = *travelOption.taxiJourney;
                    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::TaxiApproach;
                    ActivateRouteTravelPlan(_activeTaxiJourney.sourceGroundPlan);

                    integration::BotActivityLog::Record(
                        me, _identity.name, _identity.id,
                        "travel_option",
                        BuildTravelNarrative(
                            _session,
                            step,
                            DescribeTravelOptionChoice(travelOption)));
                }
                else if (travelOption.groundPlan.has_value() && !travelOption.groundPlan->empty())
                {
                    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                    ActivateRouteTravelPlan(*travelOption.groundPlan);

                    integration::BotActivityLog::Record(
                        me, _identity.name, _identity.id,
                        "travel_option",
                        BuildTravelNarrative(
                            _session,
                            step,
                            DescribeTravelOptionChoice(travelOption)));
                }
            }
            else
            {
                service::WorldBotResolvedTravelPlan routePlan;
                if (TryBuildRouteTravelPlan(step, routePlan))
                {
                    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                    _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
                    ActivateRouteTravelPlan(routePlan);
                }
            }

            if (_routeTravelPlanActive)
            {
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_plan",
                    BuildTravelNarrative(
                        _session,
                        step,
                        "nearest node found"
                            " attach_yd=" + std::to_string(_routeTravelPlan.attachDistanceYards)
                            + " route_yd=" + std::to_string(_routeTravelPlan.routeDistanceYards)
                            + " final_leg_yd=" + std::to_string(_routeTravelPlan.detachDistanceYards)
                            + " total_yd=" + std::to_string(_routeTravelPlan.totalDistanceYards)
                            + " eta=" + FormatDurationMs(_routeTravelPlan.etaMs)
                            + " waypoints=" + std::to_string(_routeTravelPlan.waypoints.size())));
            }

            _travelWatchdogConfig = BuildActiveTravelWatchdogConfig(step, travelTier);
            ApplyVisibleTravelMode(travelTier);
            MoveToActiveTravelTarget(step);
            _traveling = true;

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_start",
                BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step)));

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "status_change",
                std::string("Starting travel | mode=") + DescribeTravelCapabilityTier(travelTier)
                    + " option=" + DescribeTravelOptionMode(_activeTravelOptionMode)
                    + " spell=" + std::to_string(_visibleTravelModeSpellId)
                    + " speed_rate=" + std::to_string(_visibleTravelSpeedRate)
                    + " | " + BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step)));

            PersistRuntimeLedgerState();
        }
        else
        {
            if (_activeTravelExecutionPhase == ActiveTravelExecutionPhase::TaxiTransit)
            {
                _activeTaxiTransitElapsedMs += TickIntervalMs;
                if (_activeTaxiTransitElapsedMs >= _activeTaxiJourney.taxiCandidate.route.totalEtaMs)
                    CompleteActiveTaxiTransit(step);
                return;
            }

            std::uint32_t const currentZoneId = me->GetZoneId();
            if (_routeTravelPlanActive
                && _routeTravelLastZoneId != 0
                && currentZoneId != 0
                && currentZoneId != _routeTravelLastZoneId)
            {
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_zone_transition",
                    BuildTravelNarrative(
                        _session,
                        step,
                        "zone change " + std::to_string(_routeTravelLastZoneId)
                            + " -> " + std::to_string(currentZoneId)
                            + " | reattaching to local network"));

                if (TryReanchorActiveRouteTravelPlan(step, "zone transition reanchor"))
                    return;

                _routeTravelLastZoneId = currentZoneId;
            }

            // Check arrival
            float const dist = GetActiveTravelTargetDistance(step);
            if (dist <= ArrivalThreshold)
            {
                if (_routeTravelPlanActive && AdvanceAlongActiveRouteTravelPlan())
                {
                    MoveToActiveTravelTarget(step);
                    ResetTravelWatchdog(_travelWatchdog);
                    integration::BotActivityLog::Record(
                        me, _identity.name, _identity.id,
                        "travel_waypoint",
                        BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step)));
                    return;
                }

                if (_activeTravelExecutionPhase == ActiveTravelExecutionPhase::TaxiApproach)
                {
                    BeginActiveTaxiTransit(step);
                    return;
                }

                _traveling = false;
                _activeTravelStepStartKnown = false;
                ClearVisibleTravelMode();
                ClearActiveRouteTravelPlan();
                ClearActiveTaxiTravel();
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_arrive",
                    "Arrived in " + ResolveZoneName(
                        step.taskIndex >= 0 && static_cast<std::size_t>(step.taskIndex) < _session.tasks.size()
                            ? _session.tasks[static_cast<std::size_t>(step.taskIndex)].targetZoneId
                            : me->GetZoneId())
                        + " for " + ResolveStepObjectiveLabel(_session, step));

                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "status_change",
                    "Destination reached -> beginning "
                        + DescribeNextTask(_session, _currentStep + 1));

                AdvanceStep();
                ResetTravelWatchdog(_travelWatchdog);
                return;
            }

            TravelWatchdogSignal const signal = UpdateTravelWatchdog(
                _travelWatchdog,
                me->GetPositionX(),
                me->GetPositionY(),
                me->GetPositionZ(),
                TickIntervalMs,
                _travelWatchdogConfig);
            if (signal == TravelWatchdogSignal::Stuck || signal == TravelWatchdogSignal::Timeout)
            {
                if (_routeTravelPlanActive
                    && TryReanchorActiveRouteTravelPlan(
                        step,
                        signal == TravelWatchdogSignal::Timeout
                            ? "timeout reanchor"
                            : "stuck reanchor"))
                {
                    return;
                }

                char const* eventType = signal == TravelWatchdogSignal::Timeout
                    ? "travel_timeout"
                    : "travel_stuck";
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    eventType,
                    DescribeTravelRecovery(step, eventType));
                if (_session.sourceKind == "debug_route_harness")
                {
                    integration::BotActivityLog::Record(
                        me, _identity.name, _identity.id,
                        "session_abort",
                        std::string(eventType) + " -> ending debug route harness session");
                    _traveling = false;
                    ClearVisibleTravelMode();
                    ClearActiveRouteTravelPlan();
                    ClearActiveTaxiTravel();
                    ResetTravelWatchdog(_travelWatchdog);
                    _sessionDone = true;
                    return;
                }

                float targetX = step.x;
                float targetY = step.y;
                float targetZ = step.z;
                if (_routeTravelPlanActive && _routeTravelWaypointIndex < _routeTravelPlan.waypoints.size())
                {
                    service::WorldBotRouteWaypoint const& waypoint =
                        _routeTravelPlan.waypoints[_routeTravelWaypointIndex];
                    targetX = waypoint.x;
                    targetY = waypoint.y;
                    targetZ = waypoint.z;
                }
                me->NearTeleportTo(targetX, targetY, targetZ, me->GetOrientation());
                _traveling = false;
                ClearVisibleTravelMode();
                ClearActiveRouteTravelPlan();
                ClearActiveTaxiTravel();
                ResetTravelWatchdog(_travelWatchdog);
                AdvanceStep();
            }
        }
        return;
    }

    if (step.type == service::AmbientStepType::Transit)
    {
        if (_activityTimer == 0 && _activeTransitExecutionPhase == ActiveTransitExecutionPhase::None)
        {
            if (!TryBeginPhysicalTransit(step))
            {
                ClearVisibleTravelMode();
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_transit_start",
                    DescribeRuntimeStateDetail());

                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "status_change",
                    "In transit -> " + DescribeRuntimeStateDetail());

                PersistRuntimeLedgerState();
            }
        }

        _activityTimer += TickIntervalMs;
        if (_activeTransitExecutionPhase != ActiveTransitExecutionPhase::None)
        {
            uint32 const physicalTimeoutMs = std::max<uint32>(180000u, std::max<uint32>(1000u, step.durationSec * 1000u) + 60000u);
            if (_activityTimer >= physicalTimeoutMs)
            {
                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_transit_timeout",
                    "Physical transit timeout -> retrying " + DescribeRuntimeStateDetail());
                ClearActivePhysicalTransit();
                _activityTimer = 0;
                PersistRuntimeLedgerState("Retrying transit after timeout");
                return;
            }

            if (TickPhysicalTransit(step))
                return;
        }

        uint32 const durationMs = std::max<uint32>(1000u, step.durationSec * 1000u);
        if (_activityTimer >= durationMs)
        {
            if (step.mapId == me->GetMapId())
            {
                me->NearTeleportTo(step.x, step.y, step.z, me->GetOrientation());
            }

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_transit_arrive",
                DescribeRuntimeStateDetail());

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "status_change",
                "Transit complete -> " + DescribeNextTask(_session, _currentStep + 1));

            _activityTimer = 0;
            ClearActivePhysicalTransit();
            AdvanceStep();
        }
        return;
    }

    if (step.type == service::AmbientStepType::GatherHerb
        || step.type == service::AmbientStepType::GatherOre)
    {
        TickGatherStep(step);
        return;
    }

    // Activity step — count down duration.
    if (_activityTimer == 0)
    {
        integration::BotActivityLog::Record(
            me, _identity.name, _identity.id,
            "status_change",
            "beginning task -> " + step.label);
        PersistRuntimeLedgerState();
    }

    _activityTimer += TickIntervalMs;
    uint32 const durationMs = step.durationSec * 1000u;

    // Heartbeat log every 60 seconds.
    if (_activityTimer % 60000 < TickIntervalMs)
    {
        integration::BotActivityLog::Record(
            me, _identity.name, _identity.id,
            "activity_tick", step.label);
    }

    if (_activityTimer >= durationMs)
    {
        integration::BotActivityLog::Record(
            me, _identity.name, _identity.id,
            "activity_complete", step.label);
        _activityTimer = 0;
        AdvanceStep();
    }
}

void WorldBotCreatureAI::AdvanceStep()
{
    ++_currentStep;
    _traveling     = false;
    _activityTimer = 0;
    ClearVisibleTravelMode();
    ClearActiveRouteTravelPlan();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);

    if (_currentStep >= _session.steps.size())
        CompletSession();
    else
        PersistRuntimeLedgerState();
}

void WorldBotCreatureAI::CompletSession()
{
    if (_sessionDone)
        return;
    _sessionDone = true;
    ClearVisibleTravelMode();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();

    std::uint32_t const zoneId = me->GetZoneId();

    integration::BotActivityLog::Record(
        me, _identity.name, _identity.id,
        "session_complete",
        "zone=" + std::to_string(zoneId) +
        " online_ms=" + std::to_string(_worldOnlineMs));

    GetIdentityRepo().CompleteWorldSession(_identity.id, zoneId, _worldOnlineMs);

    me->DespawnOrUnsummon(Milliseconds(1000));
}

void WorldBotCreatureAI::JustDied(Unit* /*killer*/)
{
    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    ClearVisibleTravelMode();
    ClearActiveTaxiTravel();

    integration::BotActivityLog::Record(
        me, _identity.name, _identity.id,
        "status_change",
        "was attacked - Died. waiting to respawn.");

    // Guard: if session ended cleanly this is already done.
    // If the creature was forcibly removed (e.g. server shutdown), still release.
    if (!_sessionDone && _sessionReady)
    {
        GetIdentityRepo().CompleteWorldSession(
            _identity.id,
            me ? me->GetZoneId() : 0,
            _worldOnlineMs);
    }
}

} // namespace ai
} // namespace living_world

// ---------------------------------------------------------------------------
// CreatureScript registration
// ---------------------------------------------------------------------------

using namespace living_world::ai;

class WorldBotCreatureScript : public CreatureScript
{
public:
    WorldBotCreatureScript()
        : CreatureScript("worldbot_ai")
    {
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new WorldBotCreatureAI(creature);
    }
};

void AddSC_WorldBotCreatureAI()
{
    new WorldBotCreatureScript();
}
