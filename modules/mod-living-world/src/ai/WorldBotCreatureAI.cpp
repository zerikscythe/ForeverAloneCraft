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
#include <cstdlib>
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
constexpr std::uint32_t WorldBotEntry = 9900001u;
constexpr std::uint32_t DebugManaGemItemId = 33312;
constexpr float CrossMapTransitAbstractSourceDistanceYards = 300.0f;
constexpr std::uint32_t CrossMapTransitAbstractMinElapsedMs = 20000u;

char const* ToConservationModeKey(model::BotCombatConservationMode mode)
{
    switch (mode)
    {
        case model::BotCombatConservationMode::Reserve:
            return "reserve";
        case model::BotCombatConservationMode::Conservative:
            return "conservative";
        case model::BotCombatConservationMode::JitCasting:
            return "jit";
        case model::BotCombatConservationMode::FullForce:
        default:
            return "full_force";
    }
}

bool IsOffenseSuppressed(
    model::BotCombatConservationMode mode,
    bool conserving)
{
    if (mode == model::BotCombatConservationMode::JitCasting)
        return true;

    return conserving &&
        (mode == model::BotCombatConservationMode::Conservative ||
         mode == model::BotCombatConservationMode::Reserve);
}

float GetUnitManaPct(Unit const* unit)
{
    if (!unit)
        return 0.0f;

    if (unit->GetMaxPower(POWER_MANA) == 0)
        return 100.0f;

    return 100.0f * static_cast<float>(unit->GetPower(POWER_MANA)) /
        static_cast<float>(unit->GetMaxPower(POWER_MANA));
}

struct EffectiveConservationSettings
{
    model::BotCombatConservationMode mode = model::BotCombatConservationMode::FullForce;
    std::uint8_t lowWater = 0;
    std::uint8_t highWater = 100;
};

EffectiveConservationSettings ResolveEffectiveConservationSettings(
    model::BotCombatProfileSettings const& settings,
    model::WorldBotCombatSituation const& situation)
{
    EffectiveConservationSettings effective;
    effective.mode = settings.conservationMode;
    effective.lowWater = settings.resourceLowWater;
    effective.highWater = settings.resourceHighWater;

    // Open-world healers should contribute more freely than dungeon/raid healers.
    // Keep the same doctrine family, but soften the floor outdoors so they
    // still pressure targets and judge/holy-shock when the group is healthy.
    if (situation.environment == model::WorldBotCombatEnvironment::OpenWorld &&
        situation.isHealerStyle)
    {
        if (effective.mode == model::BotCombatConservationMode::Conservative)
            effective.mode = model::BotCombatConservationMode::Reserve;

        effective.lowWater = std::min<std::uint8_t>(effective.lowWater, 25);
        effective.highWater = std::min<std::uint8_t>(effective.highWater, 45);
    }

    return effective;
}

void UpdateConservationState(
    EffectiveConservationSettings const& settings,
    Unit const* bot,
    bool& conserving)
{
    if (!bot)
    {
        conserving = false;
        return;
    }

    if (settings.mode == model::BotCombatConservationMode::FullForce ||
        settings.mode == model::BotCombatConservationMode::JitCasting)
    {
        conserving = false;
        return;
    }

    if (bot->GetMaxPower(POWER_MANA) == 0)
    {
        conserving = false;
        return;
    }

    float const manaPct = GetUnitManaPct(bot);
    if (settings.mode == model::BotCombatConservationMode::Reserve)
    {
        conserving = manaPct < static_cast<float>(settings.lowWater);
        return;
    }

    if (conserving)
    {
        if (manaPct >= static_cast<float>(settings.highWater))
            conserving = false;
    }
    else if (manaPct < static_cast<float>(settings.lowWater))
    {
        conserving = true;
    }
}

struct SessionCompletionMetadata
{
    std::string   sourceKind;
    std::string   sourceKey;
    std::string   taskFamily;
    std::uint32_t targetZoneId = 0;
};

SessionCompletionMetadata BuildSessionCompletionMetadata(
    service::AmbientSession const& session,
    std::size_t currentStep)
{
    SessionCompletionMetadata metadata;
    metadata.sourceKind = session.sourceKind;
    metadata.sourceKey = session.sourceKey.empty() ? session.activityKey : session.sourceKey;

    if (session.steps.empty() || session.tasks.empty())
        return metadata;

    std::size_t stepIndex = session.steps.size() - 1;
    if (currentStep < session.steps.size())
        stepIndex = currentStep;

    while (true)
    {
        service::AmbientStep const& step = session.steps[stepIndex];
        if (step.taskIndex >= 0)
        {
            std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
            if (taskIndex < session.tasks.size())
            {
                service::AmbientSessionTask const& task = session.tasks[taskIndex];
                metadata.taskFamily = task.taskFamily;
                metadata.targetZoneId = task.targetZoneId;
                break;
            }
        }

        if (stepIndex == 0)
            break;
        --stepIndex;
    }

    return metadata;
}

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
    if (transitType != "boat" && transitType != "zeppelin")
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

    if (step.transitRouteKey == "orgrimmar_to_tirisfal_zeppelin"
        || step.transitRouteKey == "undercity_to_durotar_zeppelin")
    {
        // The Thundercaller transport-local passenger positions sampled from
        // the transport map so bots stand on the gondola instead of clustering
        // at a single guessed point.
        return PhysicalTransitRouteSpec{
            step.transitRouteKey.c_str(),
            "zeppelin",
            164871u,
            55.0f,
            60.0f,
            90.0f,
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

    if (routeSpec.transportEntry == 164871u)
    {
        // The Thundercaller transport-local passenger positions from map 591.
        static constexpr std::array<PhysicalTransitDeckSpot, 6> spots{{
            { -9.40787f, -8.02398f, -17.1578f, 3.1765f  },
            {  7.24887f, -5.48033f, -17.6859f, 4.81711f },
            {  8.00807f, -10.7134f, -17.6737f, 1.16937f },
            {  5.02375f, -7.69781f, -17.7888f, 5.98648f },
            { -5.1094f,  -11.1466f, -17.606f,  4.4855f  },
            { -5.2125f,  -4.92702f, -17.5966f, 1.43117f },
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
            case service::AmbientStepType::Grind:
                oss << "grind";
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
        case service::AmbientStepType::Grind:
            return "grind";
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

std::vector<Unit*> CollectNearbyFriendlyAmbientWorldBots(Unit* bot, float radius, bool includeSelf)
{
    std::vector<Unit*> allies;
    if (!bot || radius <= 0.0f)
        return allies;

    if (includeSelf)
        allies.push_back(bot);

    Acore::AnyFriendlyUnitInObjectRangeCheck check(bot, bot, radius);
    Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(bot, allies, check);
    Cell::VisitObjects(bot, searcher, radius);

    allies.erase(
        std::remove_if(
            allies.begin(),
            allies.end(),
            [&](Unit* candidate)
            {
                if (!candidate || !candidate->IsAlive() || !candidate->IsInWorld())
                    return true;
                if (candidate == bot)
                    return !includeSelf;

                Creature* creature = candidate->ToCreature();
                if (!creature || creature->GetEntry() != WorldBotEntry)
                    return true;

                return !bot->IsFriendlyTo(candidate);
            }),
        allies.end());

    std::sort(allies.begin(), allies.end());
    allies.erase(std::unique(allies.begin(), allies.end()), allies.end());
    return allies;
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

std::uint32_t ComputeSyntheticCreatureGlobalCooldownMs(Unit* bot, SpellInfo const* spellInfo)
{
    if (!bot || !spellInfo)
        return 0;

    if (!bot->ToCreature())
        return 0;

    std::uint32_t gcdMs = spellInfo->StartRecoveryTime;
    if (gcdMs == 0)
        return 0;

    constexpr std::uint32_t MinGcdMs = 1000u;
    constexpr std::uint32_t MaxGcdMs = 1500u;
    if (gcdMs >= MinGcdMs && gcdMs <= MaxGcdMs)
    {
        if (spellInfo->StartRecoveryCategory == 133
            && spellInfo->StartRecoveryTime == 1500
            && spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE
            && spellInfo->DmgClass != SPELL_DAMAGE_CLASS_RANGED
            && !spellInfo->HasAttribute(SPELL_ATTR0_USES_RANGED_SLOT)
            && !spellInfo->HasAttribute(SPELL_ATTR0_IS_ABILITY))
        {
            gcdMs = static_cast<std::uint32_t>(float(gcdMs) * bot->GetFloatValue(UNIT_MOD_CAST_SPEED));
        }

        gcdMs = std::clamp(gcdMs, MinGcdMs, MaxGcdMs);
    }

    return gcdMs;
}

std::string DescribeCombatActionForTrace(service::BotCombatEvaluatedAction const& action)
{
    if (action.actionType == model::BotCombatActionType::Item)
        return "Item(" + std::to_string(action.itemId) + ")";

    return DescribeSpellForTrace(action.spellId);
}

std::string DescribeTraceUnit(Unit const* unit)
{
    if (!unit)
        return "none";

    std::ostringstream oss;
    oss << "'" << unit->GetName() << "'"
        << " guid=" << unit->GetGUID().GetCounter();
    return oss.str();
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

char const* DescribeCombatEnvironment(model::WorldBotCombatEnvironment environment)
{
    switch (environment)
    {
        case model::WorldBotCombatEnvironment::DungeonOrRaid:
            return "dungeon_or_raid";
        case model::WorldBotCombatEnvironment::OpenWorld:
        default:
            return "open_world";
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
    model::HazardTuning tuning = GetHazardConfigService().GetTuning();
    if (Map const* map = bot->GetMap())
    {
        if (!map->IsDungeon() && !map->IsRaid())
        {
            tuning.damageThresholdPct = std::max(5.0f, tuning.damageThresholdPct * 2.5f);
            tuning.consecutiveDamageTicks = std::max(4, tuning.consecutiveDamageTicks + 2);
            tuning.commitWindowMs = std::min(1000, tuning.commitWindowMs);
        }
    }

    service::SharedHazardEvaluationResult const evaluation =
        service::EvaluateSharedHazard(
            hazardState,
            std::chrono::steady_clock::now(),
            bot->GetHealthPct(),
            currentPosition,
            hasKnownAura,
            hazardSpellId,
            hasKnownAura ? 1.0f : 0.0f,
            tuning);

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
    _combatInterrupt = {};
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);
    _knownExploredZoneIds.clear();
    _preparedBuild = {};
    _preparedBuildReady = false;
    InvalidateCombatProfile();
    _lastDebugCombatManaDrainWorldMs = 0;
    _debugCombatManaGemObserved = false;
    _syntheticGlobalCooldownRemainingMs = 0;
    _pendingCorpseRecovery = false;
    _corpseRecoveryCount = 0;
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

    MaybeStartDebugForcedCombat();

    if (me->IsInCombat() || me->GetVictim())
    {
        TickCombat(TickIntervalMs);
        return;
    }

    if (_combatInterrupt.active)
    {
        if (TrySustainAmbientCombat("combat_resume_from_nearby_ally"))
        {
            TickCombat(TickIntervalMs);
            return;
        }

        std::uint32_t const resumeDelayMs =
            _combatInterrupt.allClearRequiredMs != 0
                ? _combatInterrupt.allClearRequiredMs
                : ResolveCombatResumeDelayMs();
        _combatInterrupt.allClearElapsedMs = std::min(
            resumeDelayMs,
            _combatInterrupt.allClearElapsedMs + TickIntervalMs);
        if (_combatInterrupt.allClearElapsedMs < resumeDelayMs)
        {
            if (IsDebugForcedCombatIdentity())
            {
                std::ostringstream graceTrace;
                graceTrace << "phase='combat' decision='hold_resume' "
                           << "reason='disengage_grace' "
                           << "grace_ms=" << _combatInterrupt.allClearElapsedMs << " "
                           << "victim=" << DescribeTraceUnit(nullptr);
                RecordCombatTrace(graceTrace.str());
            }
            return;
        }

        ResumeSuspendedStepAfterCombat();
    }

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
    if (_pendingCorpseRecovery || (me && !me->IsAlive()))
        return "dead_pending_recovery";
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
    if (_pendingCorpseRecovery || (me && !me->IsAlive()))
    {
        return "Downed - waiting "
            + std::to_string(CorpseRecoveryCorpseDelaySec)
            + "s for rez, then "
            + std::to_string(CorpseRecoveryRunbackDelaySec)
            + "s corpse run";
    }
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
        bool const directBoard = _activePhysicalTransit.transitType == "zeppelin";
        if (!directBoard
            && me->GetDistance(boardX, boardY, boardZ) > _activePhysicalTransit.boardArriveRadius)
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
    if (_combatInterrupt.active && !me->IsInCombat() && !me->GetVictim())
        ResumeSuspendedStepAfterCombat();
}

void WorldBotCreatureAI::JustRespawned()
{
    ApplyIdentityToCreature();

    if (!_pendingCorpseRecovery)
        return;

    _pendingCorpseRecovery = false;
    _combatInterrupt = {};
    _syntheticGlobalCooldownRemainingMs = 0;
    _combatDisengageGraceMs = 0;
    _usedSimulatedItemsThisCombat.clear();
    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    ClearVisibleTravelMode();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();
    ResetCombatMetricsSegment();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        "Recovered after corpse run simulation; resuming session.");
    PersistRuntimeLedgerState("Recovered after corpse run simulation");
}

void WorldBotCreatureAI::CorpseRemoved(uint32& respawnDelay)
{
    if (!_pendingCorpseRecovery || !me)
        return;

    Position const deathPosition = me->GetPosition();
    me->SetHomePosition(deathPosition);
    respawnDelay = CorpseRecoveryRunbackDelaySec;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "status_change",
        "No rez arrived; simulating corpse run in "
            + std::to_string(respawnDelay) + "s.");
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

bool WorldBotCreatureAI::IsDebugForcedCombatIdentity() const
{
    if (_identity.id == 0)
        return false;

    std::uint32_t const forcedIdentityId =
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceCombatTargetIdentityId", 0);
    return forcedIdentityId != 0 && forcedIdentityId == _identity.id;
}

namespace
{

bool IsTrainingDummyTarget(Creature const* creature)
{
    if (!creature)
        return false;

    switch (creature->GetEntry())
    {
        case 31144u: // Grandmaster's Training Dummy
        case 31146u: // Heroic Training Dummy
        case 32666u: // Expert's Training Dummy
        case 32667u: // Master's Training Dummy
            return true;
        default:
            break;
    }

    return creature->GetScriptName() == "npc_training_dummy";
}

std::vector<std::uint32_t> ParseSubjectEntryList(service::AmbientStep const& step)
{
    std::vector<std::uint32_t> entries;
    if (step.subjectId != 0)
        entries.push_back(step.subjectId);

    std::string token;
    std::istringstream input(step.subjectKey);
    while (std::getline(input, token, ','))
    {
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char c) { return std::isspace(c) != 0; }), token.end());
        if (token.empty())
            continue;

        char* end = nullptr;
        unsigned long parsed = std::strtoul(token.c_str(), &end, 10);
        if (end && *end == '\0' && parsed > 0ul)
            entries.push_back(static_cast<std::uint32_t>(parsed));
    }

    std::sort(entries.begin(), entries.end());
    entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
    return entries;
}

bool ShouldBypassDebugForcedAttack(Creature* me, Creature* target)
{
    if (!me || !target || !target->IsAlive())
        return false;

    if (IsTrainingDummyTarget(target))
    {
        target->SetFaction(14u); // hostile monster faction for harness sparring
        return true;
    }

    return !target->IsFriendlyTo(me);
}

std::size_t StartDebugForcedCreaturePack(Creature* me, Creature* primaryTarget, std::uint32_t entry, float radius)
{
    if (!me || !primaryTarget || entry == 0u)
        return 0u;

    std::list<Creature*> nearbyCreatures;
    me->GetCreatureListWithEntryInGrid(nearbyCreatures, entry, radius);

    std::size_t engagedCount = 0u;
    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive() || creature == primaryTarget)
            continue;

        me->SetInCombatWith(creature);
        creature->SetInCombatWith(me);
        me->AddThreat(creature, 1.0f);
        creature->AddThreat(me, 1.0f);

        if (creature->IsAIEnabled)
            creature->AI()->AttackStart(me);

        ++engagedCount;
    }

    return engagedCount;
}

} // namespace

void WorldBotCreatureAI::MaybeStartDebugForcedCombat()
{
    if (!me || !IsDebugForcedCombatIdentity() || me->IsInCombat() || me->GetVictim())
        return;
    if (_sessionReady && !_sessionDone && _currentStep < _session.steps.size())
    {
        service::AmbientStep const& currentStep = _session.steps[_currentStep];
        if (currentStep.type == service::AmbientStepType::Travel)
            return;
    }

    std::uint32_t const targetEntry =
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceCombatTargetEntry", 0);
    if (targetEntry == 0)
        return;

    float const searchRadius = std::max(
        5.0f,
        sConfigMgr->GetOption<float>("LivingWorld.DebugForceCombatTargetSearchRadius", 40.0f));

    Creature* target = me->FindNearestCreature(targetEntry, searchRadius, true);
    if (!target)
    {
        RecordCombatTrace(
            std::string("phase='debug' decision='force_target_scan' result='no_target' target_entry=")
            + std::to_string(targetEntry)
            + " radius=" + std::to_string(searchRadius));
        return;
    }

    bool canStartAttack = me->CanStartAttack(target, true);
    bool usedBypass = false;
    if (!canStartAttack && ShouldBypassDebugForcedAttack(me, target))
    {
        RecordCombatTrace(
            std::string("phase='debug' decision='force_target_override' result='bypass_can_start_attack' target='")
            + target->GetName()
            + "' target_entry=" + std::to_string(target->GetEntry())
            + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " target_faction=" + std::to_string(target->GetFaction())
            + " distance=" + std::to_string(me->GetDistance(target)));
        canStartAttack = true;
        usedBypass = true;
    }

    if (!canStartAttack)
    {
        RecordCombatTrace(
            std::string("phase='debug' decision='force_target_scan' result='invalid_target' target='")
            + target->GetName()
            + "' target_entry=" + std::to_string(target->GetEntry())
            + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " target_faction=" + std::to_string(target->GetFaction())
            + " distance=" + std::to_string(me->GetDistance(target)));
        return;
    }

    RecordCombatTrace(
        std::string("phase='debug' decision='force_target' target='")
        + target->GetName()
        + "' target_entry=" + std::to_string(targetEntry)
        + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
        + " distance=" + std::to_string(me->GetDistance(target)));

    me->SetInCombatWith(target);
    target->SetInCombatWith(me);
    AttackStart(target);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);

    if (usedBypass && target->IsAIEnabled)
        target->AI()->AttackStart(me);

    std::size_t const packAssistCount = StartDebugForcedCreaturePack(me, target, targetEntry, 18.0f);
    if (packAssistCount > 0u)
    {
        RecordCombatTrace(
            std::string("phase='debug' decision='force_target_pack' target='")
            + target->GetName()
            + "' target_entry=" + std::to_string(targetEntry)
            + " pack_assist_count=" + std::to_string(packAssistCount));
    }

    if (me->GetVictim() == target || me->IsInCombat())
        TickCombat(0);
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
    if (_combatInterrupt.active || !_sessionReady || _sessionDone)
        return;

    _combatInterrupt.active = true;
    _combatInterrupt.reason =
        (_currentStep < _session.steps.size() && IsCombatAreaStep(_session.steps[_currentStep]))
            ? CombatInterruptionReason::AuthoredGrind
            : CombatInterruptionReason::ReactiveDefense;
    _combatInterrupt.suspendedStepIndex = _currentStep;
    _combatInterrupt.allClearElapsedMs = 0;
    _combatInterrupt.allClearRequiredMs = ResolveCombatResumeDelayMs();
    _combatDisengageGraceMs = 0;
    ResetCombatMetricsSegment();
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
    if (!_combatInterrupt.active)
        return;

    RecordCombatSummary("combat_exit");
    _combatInterrupt = {};
    _combatDisengageGraceMs = 0;
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

void WorldBotCreatureAI::ResetCombatMetricsSegment()
{
    _combatMetricsCurrent = {};
}

void WorldBotCreatureAI::RecordCombatDamageDone(std::uint32_t amount)
{
    if (amount == 0)
        return;

    _combatMetricsCurrent.outgoingDamage += amount;
    _combatMetricsSession.outgoingDamage += amount;
}

void WorldBotCreatureAI::RecordCombatDamageTaken(std::uint32_t amount)
{
    if (amount == 0)
        return;

    _combatMetricsCurrent.incomingDamage += amount;
    _combatMetricsSession.incomingDamage += amount;
}

void WorldBotCreatureAI::RecordCombatHealingDone(std::uint32_t amount)
{
    if (amount == 0)
        return;

    _combatMetricsCurrent.outgoingHealing += amount;
    _combatMetricsSession.outgoingHealing += amount;
}

void WorldBotCreatureAI::RecordCombatHealingTaken(std::uint32_t amount)
{
    if (amount == 0)
        return;

    _combatMetricsCurrent.incomingHealing += amount;
    _combatMetricsSession.incomingHealing += amount;
}

void WorldBotCreatureAI::RecordCombatSummary(char const* reason)
{
    if (!me || !_combatInterrupt.active)
        return;

    if (_combatMetricsCurrent.outgoingDamage == 0
        && _combatMetricsCurrent.incomingDamage == 0
        && _combatMetricsCurrent.outgoingHealing == 0
        && _combatMetricsCurrent.incomingHealing == 0)
    {
        return;
    }

    std::ostringstream detail;
    detail << "reason='" << (reason ? reason : "unknown") << "' "
           << "step='" << DescribeCurrentStep() << "' "
           << "outgoing_damage=" << _combatMetricsCurrent.outgoingDamage << " "
           << "incoming_damage=" << _combatMetricsCurrent.incomingDamage << " "
           << "outgoing_healing=" << _combatMetricsCurrent.outgoingHealing << " "
           << "incoming_healing=" << _combatMetricsCurrent.incomingHealing << " "
           << "session_outgoing_damage=" << _combatMetricsSession.outgoingDamage << " "
           << "session_incoming_damage=" << _combatMetricsSession.incomingDamage << " "
           << "session_outgoing_healing=" << _combatMetricsSession.outgoingHealing << " "
           << "session_incoming_healing=" << _combatMetricsSession.incomingHealing;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_summary",
        detail.str());
}

Unit* WorldBotCreatureAI::FindNearbyAmbientCombatTarget(float radius) const
{
    if (!me || radius <= 0.0f)
        return nullptr;

    auto const isViableTarget =
        [&](Unit* candidate) -> bool
        {
            if (!candidate || candidate == me || !candidate->IsAlive() || !candidate->IsInWorld())
                return false;
            if (me->IsFriendlyTo(candidate))
                return false;
            if (!candidate->isTargetableForAttack(false, me))
                return false;
            return true;
        };

    auto const considerClosest =
        [&](std::vector<Unit*> const& candidates) -> Unit*
        {
            Unit* best = nullptr;
            float bestDistance = std::numeric_limits<float>::max();
            for (Unit* candidate : candidates)
            {
                if (!isViableTarget(candidate))
                    continue;

                float const distance = me->GetDistance(candidate);
                if (!best || distance < bestDistance)
                {
                    best = candidate;
                    bestDistance = distance;
                }
            }

            return best;
        };

    enum class PvPTargetRole : std::uint8_t
    {
        Unknown = 0,
        Tank = 1,
        Healer = 2,
        Damage = 3,
    };

    auto const classifyWorldBotRole =
        [&](Unit* candidate) -> PvPTargetRole
        {
            if (!candidate)
                return PvPTargetRole::Unknown;

            if (candidate == me)
            {
                if (_preparedBuild.resolvedRoleKey == "TANK")
                    return PvPTargetRole::Tank;
                if (_preparedBuild.resolvedRoleKey == "HEAL")
                    return PvPTargetRole::Healer;
                if (_preparedBuild.resolvedRoleKey == "DPS")
                    return PvPTargetRole::Damage;
                return PvPTargetRole::Unknown;
            }

            Creature* creature = candidate->ToCreature();
            if (!creature || creature->GetEntry() != WorldBotEntry || !creature->AI())
                return PvPTargetRole::Unknown;

            auto* worldBotAi = static_cast<WorldBotCreatureAI*>(creature->AI());
            if (!worldBotAi->_preparedBuild.IsReady())
                return PvPTargetRole::Unknown;

            if (worldBotAi->_preparedBuild.resolvedRoleKey == "TANK")
                return PvPTargetRole::Tank;
            if (worldBotAi->_preparedBuild.resolvedRoleKey == "HEAL")
                return PvPTargetRole::Healer;
            if (worldBotAi->_preparedBuild.resolvedRoleKey == "DPS")
                return PvPTargetRole::Damage;

            return PvPTargetRole::Unknown;
        };

    auto const isPvPLikeTarget =
        [&](Unit* candidate) -> bool
        {
            if (!candidate)
                return false;
            if (candidate->ToPlayer())
                return true;

            Creature* creature = candidate->ToCreature();
            return creature && creature->GetEntry() == WorldBotEntry;
        };

    std::vector<Unit*> allyTargets;
    std::vector<Unit*> friendlyAllies = CollectNearbyFriendlyAmbientWorldBots(me, radius, false);
    for (Unit* ally : friendlyAllies)
    {
        if (!ally || !ally->IsAlive())
            continue;

        if (Unit* victim = ally->GetVictim())
            allyTargets.push_back(victim);

        for (Unit* attacker : ally->getAttackers())
            allyTargets.push_back(attacker);
    }

    if (Unit* best = considerClosest(allyTargets))
        return best;

    std::vector<Unit*> nearbyHostiles;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(me, me, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, nearbyHostiles, check);
    Cell::VisitObjects(me, searcher, radius);

    nearbyHostiles.erase(
        std::remove_if(
            nearbyHostiles.begin(),
            nearbyHostiles.end(),
            [&](Unit* candidate)
            {
                if (!isViableTarget(candidate))
                    return true;
                return !candidate->IsInCombat() && !candidate->GetVictim();
            }),
        nearbyHostiles.end());

    std::vector<Unit*> candidates = allyTargets;
    candidates.insert(candidates.end(), nearbyHostiles.begin(), nearbyHostiles.end());
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    model::BotCombatProfileSettings::TargetingSettings targetingSettings;
    if (_combatPreparedProfile.resolution.source != service::BotCombatDoctrineSource::None)
        targetingSettings = _combatPreparedProfile.resolution.profile.settings.targeting;

    auto const countAlliedFocus =
        [&](Unit* candidate) -> std::uint32_t
        {
            std::uint32_t alliedFocusCount = 0;
            for (Unit* ally : friendlyAllies)
            {
                if (!ally || !ally->IsAlive())
                    continue;
                if (ally->GetVictim() == candidate)
                    ++alliedFocusCount;
            }
            if (me->GetVictim() == candidate)
                ++alliedFocusCount;
            return alliedFocusCount;
        };

    std::unordered_map<Unit*, std::uint32_t> assistVictimCounts;
    for (Unit* ally : friendlyAllies)
    {
        if (!ally || !ally->IsAlive())
            continue;
        if (Unit* victim = ally->GetVictim())
        {
            if (isViableTarget(victim))
                ++assistVictimCounts[victim];
        }
    }
    if (Unit* victim = me->GetVictim())
    {
        if (isViableTarget(victim))
            ++assistVictimCounts[victim];
    }

    Unit* preferredAssistTarget = nullptr;
    std::uint32_t preferredAssistCount = 0;
    for (auto const& [candidate, count] : assistVictimCounts)
    {
        if (!preferredAssistTarget || count > preferredAssistCount)
        {
            preferredAssistTarget = candidate;
            preferredAssistCount = count;
        }
    }

    Unit* tankAssistTarget = nullptr;
    auto const considerTankVictim =
        [&](Unit* ally)
        {
            if (!ally || !ally->IsAlive())
                return;
            if (classifyWorldBotRole(ally) != PvPTargetRole::Tank)
                return;
            Unit* victim = ally->GetVictim();
            if (isViableTarget(victim))
                tankAssistTarget = victim;
        };
    considerTankVictim(me);
    if (!tankAssistTarget)
    {
        for (Unit* ally : friendlyAllies)
        {
            considerTankVictim(ally);
            if (tankAssistTarget)
                break;
        }
    }

    auto const scoreAssistCandidate =
        [&](Unit* candidate) -> float
        {
            if (!isViableTarget(candidate))
                return -100000.0f;

            float score = 0.0f;
            float const distance = me->GetDistance(candidate);
            score += std::max(0.0f, 120.0f - distance * 4.0f);
            score += std::clamp(100.0f - candidate->GetHealthPct(), 0.0f, 100.0f) * 2.0f;

            if (candidate == me->GetVictim())
                score += targetingSettings.currentTargetBias;
            if (candidate == tankAssistTarget)
                score += targetingSettings.assistTargetBias;
            else if (candidate == preferredAssistTarget)
                score += targetingSettings.assistTargetBias * 0.75f;

            Unit* victim = candidate->GetVictim();
            if (victim && me->IsFriendlyTo(victim))
            {
                PvPTargetRole const victimRole = classifyWorldBotRole(victim);
                if (victimRole == PvPTargetRole::Healer)
                    score += targetingSettings.protectAllyBias;
                else if (victimRole == PvPTargetRole::Damage)
                    score += targetingSettings.protectAllyBias * 0.8f;
                else if (victimRole == PvPTargetRole::Tank)
                    score += targetingSettings.protectAllyBias * 0.5f;
                else
                    score += targetingSettings.protectAllyBias * 0.35f;
            }

            score += static_cast<float>(countAlliedFocus(candidate)) * targetingSettings.focusFireBias;
            return score;
        };

    bool const pvpLikeFight = std::any_of(
        candidates.begin(),
        candidates.end(),
        [&](Unit* candidate) { return isPvPLikeTarget(candidate); });
    if (!pvpLikeFight)
    {
        if (targetingSettings.mode == model::BotCombatTargetingMode::Assist
            || targetingSettings.mode == model::BotCombatTargetingMode::Skirmish)
        {
            Unit* best = nullptr;
            float bestScore = -100000.0f;
            for (Unit* candidate : candidates)
            {
                float const score = scoreAssistCandidate(candidate);
                if (!best || score > bestScore)
                {
                    best = candidate;
                    bestScore = score;
                }
            }
            if (best)
                return best;
        }
        return considerClosest(candidates);
    }

    bool const hasEnemyHealer = std::any_of(
        candidates.begin(),
        candidates.end(),
        [&](Unit* candidate) { return classifyWorldBotRole(candidate) == PvPTargetRole::Healer; });
    bool const hasEnemyDamage = std::any_of(
        candidates.begin(),
        candidates.end(),
        [&](Unit* candidate) { return classifyWorldBotRole(candidate) == PvPTargetRole::Damage; });

    PvPTargetRole const selfRole = classifyWorldBotRole(me);
    auto const scoreCandidate =
        [&](Unit* candidate) -> float
        {
            if (!isViableTarget(candidate))
                return -100000.0f;

            float score = 0.0f;
            float const distance = me->GetDistance(candidate);
            PvPTargetRole const candidateRole = classifyWorldBotRole(candidate);
            bool const assistMode = targetingSettings.mode == model::BotCombatTargetingMode::Assist;
            bool const skirmishMode = targetingSettings.mode == model::BotCombatTargetingMode::Skirmish;
            float const assistBias = skirmishMode
                ? (targetingSettings.assistTargetBias * 0.55f)
                : (assistMode ? targetingSettings.assistTargetBias * 1.35f : targetingSettings.assistTargetBias);
            float const healerBias = skirmishMode
                ? (targetingSettings.preferHealerBias * 1.5f)
                : (assistMode ? targetingSettings.preferHealerBias * 0.6f : targetingSettings.preferHealerBias);
            float const dpsBias = skirmishMode
                ? (targetingSettings.preferDpsBias * 1.15f)
                : targetingSettings.preferDpsBias;
            float const tankPenalty = skirmishMode
                ? (targetingSettings.avoidTankBias * 1.2f)
                : targetingSettings.avoidTankBias;

            score += std::max(0.0f, 120.0f - distance * 4.0f);
            score += std::clamp(100.0f - candidate->GetHealthPct(), 0.0f, 100.0f) * 3.0f;

            if (candidate == me->GetVictim())
                score += targetingSettings.currentTargetBias;
            if (candidate == tankAssistTarget)
                score += assistBias;
            else if (candidate == preferredAssistTarget)
                score += assistBias * 0.75f;

            if (candidateRole == PvPTargetRole::Healer && candidate->GetHealthPct() <= 55.0f)
                score += 120.0f;

            switch (selfRole)
            {
                case PvPTargetRole::Tank:
                    switch (candidateRole)
                    {
                        case PvPTargetRole::Damage: score += dpsBias * 2.0f; break;
                        case PvPTargetRole::Healer: score += healerBias; break;
                        case PvPTargetRole::Tank:   score += 20.0f; break;
                        case PvPTargetRole::Unknown:score += dpsBias; break;
                    }
                    if (hasEnemyDamage && candidateRole == PvPTargetRole::Healer)
                        score -= healerBias * 0.35f;
                    if (hasEnemyDamage && candidateRole == PvPTargetRole::Tank)
                    {
                        score -= tankPenalty * 1.5f;
                        if (candidate == me->GetVictim())
                            score -= targetingSettings.currentTargetBias * 0.9f;
                    }
                    break;
                case PvPTargetRole::Healer:
                    switch (candidateRole)
                    {
                        case PvPTargetRole::Damage: score += dpsBias; break;
                        case PvPTargetRole::Healer: score += healerBias * 0.4f; break;
                        case PvPTargetRole::Tank:   score += 40.0f; break;
                        case PvPTargetRole::Unknown:score += dpsBias * 0.65f; break;
                    }
                    break;
                case PvPTargetRole::Damage:
                case PvPTargetRole::Unknown:
                default:
                    switch (candidateRole)
                    {
                        case PvPTargetRole::Healer: score += healerBias * 2.0f; break;
                        case PvPTargetRole::Damage: score += dpsBias * 1.4f; break;
                        case PvPTargetRole::Tank:   score += 10.0f; break;
                        case PvPTargetRole::Unknown:score += dpsBias * 0.85f; break;
                    }
                    if (hasEnemyHealer && candidateRole != PvPTargetRole::Healer)
                        score -= healerBias * 0.75f;
                    else if (!hasEnemyHealer && hasEnemyDamage && candidateRole == PvPTargetRole::Tank)
                        score -= tankPenalty;
                    break;
            }

            Unit* victim = candidate->GetVictim();
            if (victim && me->IsFriendlyTo(victim))
            {
                PvPTargetRole const victimRole = classifyWorldBotRole(victim);
                if (victim == me)
                    score += (selfRole == PvPTargetRole::Healer)
                        ? (targetingSettings.protectAllyBias * 1.35f)
                        : targetingSettings.protectAllyBias;

                switch (victimRole)
                {
                    case PvPTargetRole::Healer: score += targetingSettings.protectAllyBias; break;
                    case PvPTargetRole::Damage: score += targetingSettings.protectAllyBias * 0.8f; break;
                    case PvPTargetRole::Tank:   score += targetingSettings.protectAllyBias * 0.5f; break;
                    case PvPTargetRole::Unknown:score += targetingSettings.protectAllyBias * 0.35f; break;
                }
            }

            score += static_cast<float>(countAlliedFocus(candidate)) * targetingSettings.focusFireBias;
            return score;
        };

    Unit* best = nullptr;
    float bestScore = -100000.0f;
    for (Unit* candidate : candidates)
    {
        float const score = scoreCandidate(candidate);
        if (!best || score > bestScore)
        {
            best = candidate;
            bestScore = score;
        }
    }

    if (best)
        return best;

    return considerClosest(candidates);
}

bool WorldBotCreatureAI::IsCombatAreaStep(service::AmbientStep const& step) const
{
    return step.type == service::AmbientStepType::Grind;
}

std::uint32_t WorldBotCreatureAI::ResolveCombatResumeDelayMs() const
{
    if (!_combatInterrupt.active)
        return ReactiveCombatResumeDelayMs;

    return _combatInterrupt.reason == CombatInterruptionReason::AuthoredGrind
        ? AuthoredCombatResumeDelayMs
        : ReactiveCombatResumeDelayMs;
}

Creature* WorldBotCreatureAI::FindNearestGrindTarget(service::AmbientStep const& step) const
{
    if (!me)
        return nullptr;

    std::vector<std::uint32_t> const entries = ParseSubjectEntryList(step);
    float const searchRadius = std::max(5.0f, step.combatRadius);
    Creature* bestTarget = nullptr;
    float bestDistance = std::numeric_limits<float>::max();

    auto considerTarget =
        [&](Creature* target)
        {
            if (!target || !target->IsAlive() || target == me || target->IsFriendlyTo(me))
                return;

            float const distance = me->GetDistance(target);
            if (distance < bestDistance)
            {
                bestTarget = target;
                bestDistance = distance;
            }
        };

    if (!entries.empty())
    {
        for (std::uint32_t entry : entries)
            considerTarget(me->FindNearestCreature(entry, searchRadius, true));
    }
    else
    {
        Unit* nearbyTarget = FindNearbyAmbientCombatTarget(searchRadius);
        considerTarget(nearbyTarget ? nearbyTarget->ToCreature() : nullptr);
    }

    return bestTarget;
}

bool WorldBotCreatureAI::TryStartGrindCombat(service::AmbientStep const& step)
{
    if (!me || me->IsInCombat() || me->GetVictim())
        return false;

    Creature* target = FindNearestGrindTarget(step);
    if (!target)
        return false;

    SuspendCurrentStepForCombat(target);
    me->SetInCombatWith(target);
    target->SetInCombatWith(me);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    AttackStart(target);

    if (target->IsAIEnabled)
        target->AI()->AttackStart(me);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_pull",
        "authored_grind target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " target='" + target->GetName() + "'");
    return true;
}

bool WorldBotCreatureAI::TrySustainAmbientCombat(char const* reason)
{
    if (!me || !_combatInterrupt.active)
        return false;

    Unit* target = FindNearbyAmbientCombatTarget(AmbientCombatAssistRadius);
    if (!target)
        return false;

    _combatDisengageGraceMs = 0;
    _combatInterrupt.allClearElapsedMs = 0;
    me->SetInCombatWith(target);
    target->SetInCombatWith(me);
    AttackStart(target);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);

    if (target->IsCreature() && target->ToCreature()->IsAIEnabled)
        target->ToCreature()->AI()->AttackStart(me);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_reassist",
        std::string("reason='") + (reason ? reason : "unknown")
            + "' target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " target='" + target->GetName() + "'");

    if (IsDebugForcedCombatIdentity())
        RecordCombatTrace(BuildCombatMovementTraceDetail(reason ? reason : "combat_reassist", target));

    return true;
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

void WorldBotCreatureAI::TickGrindStep(service::AmbientStep const& step)
{
    if (_activityTimer == 0)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "status_change",
            "beginning task -> " + step.label);
        PersistRuntimeLedgerState();
    }

    _activityTimer += TickIntervalMs;
    uint32 const durationMs = step.durationSec * 1000u;

    if (!me->IsInCombat() && !me->GetVictim() && !_combatInterrupt.active)
        TryStartGrindCombat(step);

    if (_activityTimer % 60000 < TickIntervalMs)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activity_tick",
            step.label + " | patrolling for grind targets");
    }

    if (_activityTimer >= durationMs)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activity_complete",
            step.label);
        _activityTimer = 0;
        AdvanceStep();
    }
}

void WorldBotCreatureAI::TickCombat(uint32 diff)
{
    if (!me)
        return;

    if (_syntheticGlobalCooldownRemainingMs > 0)
        _syntheticGlobalCooldownRemainingMs =
            (_syntheticGlobalCooldownRemainingMs > diff)
                ? (_syntheticGlobalCooldownRemainingMs - diff)
                : 0u;

    auto const recordDebugCombatFlowTrace =
        [&](std::string const& reason, Unit* traceTarget = nullptr)
        {
            if (!IsDebugForcedCombatIdentity())
                return;

            Unit* targetForTrace = traceTarget ? traceTarget : me->GetVictim();
            std::ostringstream oss;
            oss << "phase='combat' decision='flow' "
                << "reason='" << reason << "' "
                << "bot_in_combat=" << (me->IsInCombat() ? 1 : 0) << " "
                << "bot_casting=" << (me->IsNonMeleeSpellCast(false) ? 1 : 0) << " "
                << "victim=" << DescribeTraceUnit(targetForTrace);
            RecordCombatTrace(oss.str());
        };

    SuspendCurrentStepForCombat(me->GetVictim());

    if (!UpdateVictim())
    {
        if (TrySustainAmbientCombat("update_victim_false_reassist"))
            return;

        _combatDisengageGraceMs = std::min(CombatDisengageGraceMs, _combatDisengageGraceMs + diff);
        recordDebugCombatFlowTrace("update_victim_false");
        return;
    }

    Unit* target = me->GetVictim();
    if (!target)
    {
        if (TrySustainAmbientCombat("missing_victim_reassist"))
            return;

        _combatDisengageGraceMs = std::min(CombatDisengageGraceMs, _combatDisengageGraceMs + diff);
        recordDebugCombatFlowTrace("missing_victim_after_update");
        return;
    }

    _combatDisengageGraceMs = 0;

    model::WorldBotHazardSnapshot const hazard = BuildWorldBotHazardSnapshot(me, _hazardEvaluationState);
    std::vector<service::WorldBotNearbyHostileSnapshot> const nearbyHostiles =
        CollectNearbyHostileSnapshots(me, target, 30.0f);
    model::WorldBotCombatSituation const situation = service::BuildWorldBotCombatSituation(
        _preparedBuild,
        me,
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
                << " environment='" << DescribeCombatEnvironment(situation.environment) << "'"
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

    auto const applyMovementDoctrinePlan =
        [&]()
        {
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
        };

    if (movementDecision.source == model::WorldBotMovementDecisionSource::HazardOverride)
    {
        if (me->IsNonMeleeSpellCast(false) && !movementDecision.allowHardCasts)
            me->InterruptNonMeleeSpells(false);

        recordMovementDoctrineTrace();

        switch (movementPlan.kind)
        {
            case service::WorldBotMovementPlanKind::MovePoint:
                applyMovementDoctrinePlan();
                break;
            case service::WorldBotMovementPlanKind::Chase:
                applyMovementDoctrinePlan();
                break;
            case service::WorldBotMovementPlanKind::None:
            default:
                break;
        }

        return;
    }

    EnsureCombatProfile();
    EffectiveConservationSettings const effectiveConservation =
        ResolveEffectiveConservationSettings(
            _combatPreparedProfile.resolution.profile.settings,
            situation);
    UpdateConservationState(effectiveConservation, me, _combatConserving);
    bool const offenseSuppressed =
        IsOffenseSuppressed(effectiveConservation.mode, _combatConserving);

    if (IsDebugForcedCombatIdentity())
    {
        std::ostringstream profileTrace;
        profileTrace << "phase='combat' decision='profile_state' "
            << "prepared=" << (_combatProfilePrepared ? 1 : 0) << " "
            << "build_ready=" << (_preparedBuildReady ? 1 : 0) << " "
            << "interrupt_entries=" << _combatPreparedProfile.interruptEntries.size() << " "
            << "rotation_entries=" << _combatPreparedProfile.rotationEntries.size() << " "
            << "available_spells=" << _combatPreparedProfile.availableSpells.size() << " "
            << "conservation_mode='" << ToConservationModeKey(effectiveConservation.mode) << "' "
            << "conserving=" << (_combatConserving ? 1 : 0) << " "
            << "offense_suppressed=" << (offenseSuppressed ? 1 : 0) << " "
            << "mana_pct=" << std::fixed << std::setprecision(1) << GetUnitManaPct(me) << " "
            << "victim=" << DescribeTraceUnit(target);
        RecordCombatTrace(profileTrace.str());
    }
    MaybeApplyDebugCombatManaDrain(target);

    bool acted = false;
    service::BotCombatEvaluationResult interruptResult;
    service::BotCombatEvaluationResult rotationResult;
    if (!_combatPreparedProfile.interruptEntries.empty() || !_combatPreparedProfile.rotationEntries.empty())
    {
        service::BotCombatRuntimeContext context;
        context.bot = me;
        context.owner = nullptr;
        context.primaryTarget = target;
        context.allowHardCasts = movementDecision.allowHardCasts;
        context.syntheticGlobalCooldownRemainingMs = _syntheticGlobalCooldownRemainingMs;
        context.usedSimulatedItemsThisCombat = &_usedSimulatedItemsThisCombat;
        context.rotationWaitMs = _combatPreparedProfile.resolution.profile.settings.rotationWaitMs;
        context.defaultAoEMode = _combatPreparedProfile.resolution.profile.settings.defaultAoEMode;
        context.defaultAoEMinTargets = _combatPreparedProfile.resolution.profile.settings.defaultAoEMinTargets;
        context.defaultAoEScanRadius = _combatPreparedProfile.resolution.profile.settings.defaultAoEScanRadius;
        context.situation = situation;
        context.conservationMode = effectiveConservation.mode;
        context.conserving = _combatConserving;
        context.offenseSuppressed = offenseSuppressed;
        context.availableSpells = _combatPreparedProfile.availableSpells;

        auto const tryResult =
            [&](service::BotCombatEvaluationResult const& result) -> bool
            {
                if (result.disposition != service::BotCombatEvaluationDisposition::Cast || !result.action)
                    return result.disposition == service::BotCombatEvaluationDisposition::Wait;

                service::BotCombatEvaluatedAction const& action = *result.action;
                if (action.breaksCurrentCast && me->IsNonMeleeSpellCast(false))
                    me->InterruptNonMeleeSpells(false);

                service::BotCombatActionDispatchResult const dispatchResult =
                    service::DispatchEvaluatedAction(me, action);
                bool const casted = dispatchResult.dispatched;

                if (IsDebugForcedCombatIdentity())
                {
                    std::ostringstream castTrace;
                    castTrace << "phase='cast' "
                        << "decision='dispatch' "
                        << "entry='" << (!action.entryLabel.empty() ? action.entryLabel : "unnamed") << "' "
                        << "action='" << DescribeCombatActionForTrace(action) << "' "
                        << "action_type='" << (action.actionType == model::BotCombatActionType::Item ? "item" : "spell") << "' "
                        << "target=" << DescribeTraceUnit(action.target) << " "
                        << "primary_target=" << DescribeTraceUnit(target) << " "
                        << "dispatched=" << (casted ? 1 : 0) << " "
                        << "reason='" << dispatchResult.reason << "' "
                        << "resolved_spell=" << dispatchResult.resolvedSpellId << " "
                        << "synthetic_gcd_ms=" << _syntheticGlobalCooldownRemainingMs << " "
                        << "breaks_cast=" << (action.breaksCurrentCast ? 1 : 0) << " "
                        << "bot_casting=" << (me->IsNonMeleeSpellCast(false) ? 1 : 0);
                    RecordCombatTrace(castTrace.str());
                }

                if (casted && action.actionType == model::BotCombatActionType::Spell)
                {
                    if (Creature* creature = me->ToCreature())
                    {
                        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(action.spellId))
                        {
                            _syntheticGlobalCooldownRemainingMs =
                                ComputeSyntheticCreatureGlobalCooldownMs(me, spellInfo);
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

        interruptResult = GetRuntimeEvaluator().EvaluateInterrupts(_combatPreparedProfile, context);
        if (interruptResult.disposition != service::BotCombatEvaluationDisposition::None)
            RecordCombatTrace(BuildCombatTraceDetail("interrupt", interruptResult, target));

        acted = tryResult(interruptResult);
        if (!acted)
        {
            rotationResult = GetRuntimeEvaluator().EvaluateRotation(_combatPreparedProfile, context);
            if (rotationResult.disposition != service::BotCombatEvaluationDisposition::None)
                RecordCombatTrace(BuildCombatTraceDetail("rotation", rotationResult, target));
            acted = tryResult(rotationResult);
        }
    }
    else
    {
        RecordCombatTrace(BuildCombatMovementTraceDetail("no_prepared_entries", target));
    }

    if (acted
        && movementPlan.kind != service::WorldBotMovementPlanKind::None
        && (!me->IsNonMeleeSpellCast(false) || !movementDecision.allowHardCasts))
    {
        recordMovementDoctrineTrace();
        applyMovementDoctrinePlan();
    }

    if (!acted)
    {
        if (IsDebugForcedCombatIdentity())
        {
            std::ostringstream noActionTrace;
            noActionTrace << "phase='combat' decision='no_action' "
                << "interrupt_disposition=" << static_cast<int>(interruptResult.disposition) << " "
                << "rotation_disposition=" << static_cast<int>(rotationResult.disposition) << " "
                << "movement_plan='" << DescribeMovementPlanKind(movementPlan.kind) << "' "
                << "hard_casts=" << (movementDecision.allowHardCasts ? 1 : 0) << " "
                << "distance=" << me->GetDistance(target) << " "
                << "victim=" << DescribeTraceUnit(target);
            RecordCombatTrace(noActionTrace.str());
        }

        if (movementPlan.kind != service::WorldBotMovementPlanKind::None && me->IsNonMeleeSpellCast(false) && !movementDecision.allowHardCasts)
            me->InterruptNonMeleeSpells(false);

        switch (movementPlan.kind)
        {
            case service::WorldBotMovementPlanKind::MovePoint:
                recordMovementDoctrineTrace();
                applyMovementDoctrinePlan();
                break;
            case service::WorldBotMovementPlanKind::Chase:
                recordMovementDoctrineTrace();
                applyMovementDoctrinePlan();
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

    if (step.type == service::AmbientStepType::Grind)
    {
        TickGrindStep(step);
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

    SessionCompletionMetadata const completionMetadata =
        BuildSessionCompletionMetadata(_session, _currentStep);
    GetIdentityRepo().CompleteWorldSession(
        _identity.id,
        zoneId,
        _worldOnlineMs,
        completionMetadata.sourceKind,
        completionMetadata.sourceKey,
        completionMetadata.taskFamily,
        completionMetadata.targetZoneId);

    me->DespawnOrUnsummon(Milliseconds(1000));
}

void WorldBotCreatureAI::JustDied(Unit* /*killer*/)
{
    if (_combatInterrupt.active)
        RecordCombatSummary("death");

    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    ClearVisibleTravelMode();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();
    _syntheticGlobalCooldownRemainingMs = 0;
    _combatDisengageGraceMs = 0;
    _usedSimulatedItemsThisCombat.clear();
    _pendingCorpseRecovery = _sessionReady && !_sessionDone;
    if (_pendingCorpseRecovery)
    {
        ++_corpseRecoveryCount;
        me->SetCorpseDelay(CorpseRecoveryCorpseDelaySec);
        me->SetRespawnDelay(CorpseRecoveryRunbackDelaySec);
    }

    integration::BotActivityLog::Record(
        me, _identity.name, _identity.id,
        "status_change",
        _pendingCorpseRecovery
            ? ("Died. Leaving corpse for "
                + std::to_string(CorpseRecoveryCorpseDelaySec)
                + "s; simulating corpse run in "
                + std::to_string(CorpseRecoveryRunbackDelaySec)
                + "s if no rez arrives.")
            : "was attacked - Died. waiting to respawn.");
    PersistRuntimeLedgerState();
    ResetCombatMetricsSegment();

    // Guard: if session ended cleanly this is already done.
    // If the creature was forcibly removed (e.g. server shutdown), still release.
    if (!_sessionDone && _sessionReady && !_pendingCorpseRecovery)
    {
        SessionCompletionMetadata const completionMetadata =
            BuildSessionCompletionMetadata(_session, _currentStep);
        GetIdentityRepo().CompleteWorldSession(
            _identity.id,
            me ? me->GetZoneId() : 0,
            _worldOnlineMs,
            completionMetadata.sourceKind,
            completionMetadata.sourceKey,
            completionMetadata.taskFamily,
            completionMetadata.targetZoneId);
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
