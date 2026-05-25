#include "ai/WorldBotCreatureAI.h"
#include "ai/CompanionFollowFormation.h"

#include "Config.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "CellImpl.h"
#include "CharmInfo.h"
#include "DataStores/DBCStores.h"
#include "Map.h"
#include "Globals/ObjectMgr.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "PetDefines.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "DataStores/DBCStores.h"
#include "Time/GameTime.h"
#include "World.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlBotCombatDefaultProfileRepository.h"
#include "integration/SqlBotCombatProfileRepository.h"
#include "integration/SqlBotCombatProfileSelectionRepository.h"
#include "integration/SqlBotGlyphTemplateRepository.h"
#include "integration/SqlBotAssignedGearRepository.h"
#include "integration/SqlBotAssignedGearTemplateRepository.h"
#include "integration/SqlBotExploredZoneRepository.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "integration/SqlBotGlobalConfigRepository.h"
#include "integration/SqlTaskPointLinkRepository.h"
#include "integration/SqlTaskPointRepository.h"
#include "integration/SqlBotTalentTemplateRepository.h"
#include "integration/SqlBotVirtualLoadoutRepository.h"
#include "service/BotCombatDoctrineResolver.h"
#include "service/AmbientGroupCombatStateService.h"
#include "service/BotHazardConfigService.h"
#include "service/BotGlobalConfigService.h"
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
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <iomanip>
#include <random>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace living_world
{
namespace ai
{

namespace
{
constexpr std::uint32_t kLevelUpUiSoundKitId = 888; // UISoundLookups.LEVELUP -> SoundEntries LevelUp.wav

service::AmbientGroupCombatStateService& GetAmbientGroupCombatStateService()
{
    static service::AmbientGroupCombatStateService service;
    return service;
}

char const* ToString(service::AmbientGroupDistressTier tier)
{
    switch (tier)
    {
        case service::AmbientGroupDistressTier::Alert:
            return "alert";
        case service::AmbientGroupDistressTier::AssistRequested:
            return "assist_requested";
        case service::AmbientGroupDistressTier::Urgent:
            return "urgent";
        case service::AmbientGroupDistressTier::Critical:
            return "critical";
        case service::AmbientGroupDistressTier::None:
        default:
            return "none";
    }
}

struct AmbientTruceBubble
{
    char const* name = "";
    std::uint32_t mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float radius = 0.0f;
};

std::string NormalizeAmbientPersonalityKey(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

char const* ResolveAmbientTruceBubbleName(Unit const* unit)
{
    if (!unit)
        return nullptr;

    static constexpr std::array<AmbientTruceBubble, 4> bubbles = { {
        { "Gadgetzan", 1u, -7147.3f, -3746.7f, 8.8f, 150.0f },
        { "Booty Bay", 0u, -14457.2f, 492.1f, 15.1f, 190.0f },
        { "Shattrath", 530u, -1887.2f, 5765.3f, -12.4f, 260.0f },
        { "Dalaran", 571u, 5858.6f, 596.9f, 651.0f, 240.0f },
    } };

    for (AmbientTruceBubble const& bubble : bubbles)
    {
        if (unit->GetMapId() != bubble.mapId)
            continue;

        float const dx = unit->GetPositionX() - bubble.x;
        float const dy = unit->GetPositionY() - bubble.y;
        if ((dx * dx + dy * dy) <= (bubble.radius * bubble.radius))
            return bubble.name;
    }

    return nullptr;
}

std::uint32_t RollAmbientEncounterPercent()
{
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<std::uint32_t> dist(1u, 100u);
    return dist(rng);
}

bool SessionSourceAllowsFollowup(std::string const& sourceKind, std::string const& sourceKey)
{
    return !sourceKind.starts_with("debug_")
        && !sourceKey.starts_with("debug_");
}

std::string DescribePathTypeFlags(PathType type)
{
    std::vector<char const*> flags;
    if (type & PATHFIND_NORMAL)
        flags.push_back("NORMAL");
    if (type & PATHFIND_SHORTCUT)
        flags.push_back("SHORTCUT");
    if (type & PATHFIND_INCOMPLETE)
        flags.push_back("INCOMPLETE");
    if (type & PATHFIND_NOPATH)
        flags.push_back("NOPATH");
    if (type & PATHFIND_NOT_USING_PATH)
        flags.push_back("NOT_USING_PATH");
    if (type & PATHFIND_SHORT)
        flags.push_back("SHORT");
    if (type & PATHFIND_FARFROMPOLY_START)
        flags.push_back("FARFROMPOLY_START");
    if (type & PATHFIND_FARFROMPOLY_END)
        flags.push_back("FARFROMPOLY_END");

    if (flags.empty())
        return "BLANK";

    std::ostringstream oss;
    for (std::size_t index = 0; index < flags.size(); ++index)
    {
        if (index != 0)
            oss << '|';
        oss << flags[index];
    }
    return oss.str();
}

struct StrictTravelPathCheckResult
{
    bool calculateResult = false;
    PathType pathType = static_cast<PathType>(0);
    std::size_t pointCount = 0;
    float pathLengthYards = 0.0f;
};

StrictTravelPathCheckResult EvaluateStrictGroundTravelPath(
    Creature* mover,
    float destX,
    float destY,
    float destZ)
{
    StrictTravelPathCheckResult result;
    if (!mover)
        return result;

    float startX = mover->GetPositionX();
    float startY = mover->GetPositionY();
    float startZ = mover->GetPositionZ();
    mover->UpdateGroundPositionZ(startX, startY, startZ);

    PathGenerator path(mover);
    path.SetSlopeCheck(true);
    path.SetUseStraightPath(false);
    path.SetUseRaycast(false);

    result.calculateResult = path.CalculatePath(startX, startY, startZ, destX, destY, destZ, true);
    result.pathType = path.GetPathType();
    result.pointCount = path.GetPath().size();
    result.pathLengthYards = path.getPathLength();
    return result;
}

bool IsStrictGroundTravelPathAccepted(StrictTravelPathCheckResult const& result)
{
    return result.calculateResult
        && result.pointCount > 2u
        && !(result.pathType & PATHFIND_NOPATH)
        && !(result.pathType & PATHFIND_NOT_USING_PATH)
        && !(result.pathType & PATHFIND_SHORTCUT);
}

std::uint32_t ComputeOpportunisticAttackChance(std::uint8_t selfLevel, std::uint8_t targetLevel)
{
    int const delta = static_cast<int>(selfLevel) - static_cast<int>(targetLevel);
    if (delta >= 0)
        return static_cast<std::uint32_t>(std::clamp(75 + (delta * 3), 75, 99));

    return static_cast<std::uint32_t>(std::clamp(75 + (delta * 10), 5, 74));
}

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

bool IsTravelConnectorPointType(std::string const& pointType)
{
    if (pointType.empty())
        return false;

    return pointType == "poi"
        || pointType.ends_with("_access");
}

service::WorldBotTaxiNetwork& GetWorldBotTaxiNetwork()
{
    static service::WorldBotTaxiNetwork network = service::LoadWorldBotTaxiNetwork();
    return network;
}

service::BotGlobalConfigService& GetWorldBotGlobalConfigService()
{
    static integration::SqlBotGlobalConfigRepository repo;
    static service::BotGlobalConfigService service(repo);
    return service;
}

constexpr float GatherSearchRadius = 200.0f;
constexpr float GatherInteractRange = 6.0f;
constexpr float GatherAnchorReturnDistance = 60.0f;
constexpr float AmbientGroupTravelFollowRadius = 80.0f;
constexpr float AmbientGroupTravelCatchupDistance = 35.0f;
constexpr float AmbientGroupBuffRadius = 60.0f;
constexpr float AmbientPullReadinessRadius = 80.0f;
constexpr float AmbientPullStageMeleeFollowDistance = 6.0f;
constexpr float AmbientPullStageRangedFollowDistance = 10.0f;
constexpr float AmbientPullStageHealerFollowDistance = 12.0f;
constexpr float AmbientPullStageFallbackFollowDistance = 8.0f;
constexpr float AmbientPullStandoffMinRangeYards = 10.0f;
constexpr float AmbientPullStandoffMaxRangeYards = 15.0f;
constexpr float AmbientPullStandoffPreferredRangeYards = 12.5f;
constexpr float AmbientHostilePullStandoffMinRangeYards = 20.0f;
constexpr float AmbientHostilePullStandoffMaxRangeYards = 28.0f;
constexpr float AmbientHostilePullStandoffPreferredRangeYards = 24.0f;
constexpr std::uint32_t AmbientPullStandoffReissueMs = 700u;
constexpr float LocalTravelNavigationDistanceThresholdYards = 350.0f;
constexpr std::uint32_t WorldBotEntry = 9900001u;
constexpr std::uint32_t SummonWaterElementalSpellId = 31687u;
constexpr std::uint32_t SummonImpSpellId = 688u;
constexpr std::uint32_t SummonFelhunterSpellId = 691u;
constexpr std::uint32_t SummonVoidwalkerSpellId = 697u;
constexpr std::uint32_t SummonSuccubusSpellId = 712u;
constexpr std::uint32_t SummonFelguardSpellId = 30146u;
constexpr std::uint32_t RaiseDeadSpellId = 46584u;
constexpr std::uint32_t RaiseDeadPetSummonSpellId = 52150u;
constexpr std::uint32_t MasterOfGhoulsSpellId = 52143u;
constexpr std::uint32_t GlyphOfTheGhoulSpellId = 58686u;
constexpr std::uint32_t GlyphOfFelguardSpellId = 56246u;
constexpr std::uint32_t GlyphOfFelhunterSpellId = 56249u;
constexpr std::uint32_t GlyphOfVoidwalkerSpellId = 56247u;
constexpr std::uint32_t HunterWorldBotDefaultPetEntry = 5449u; // Tamed Wolf
constexpr std::uint32_t ControlledPetSummonPropertiesId = 67u;
constexpr std::uint32_t DebugManaGemItemId = 33312;
constexpr float CrossMapTransitAbstractSourceDistanceYards = 300.0f;
constexpr std::uint32_t CrossMapTransitAbstractMinElapsedMs = 20000u;
constexpr std::uint32_t ConsecrationBaseSpellId = 20116u;
constexpr std::uint32_t JudgementOfWisdomBaseSpellId = 20186u;
constexpr std::uint32_t MechanoKickSpellId = 61110u;
constexpr std::uint32_t ConsecrationLifetimeMs = 8000u;
constexpr std::uint32_t GroupCombatHandoffRefreshMs = 1500u;
constexpr float ConsecrationRecenterDistanceYards = 10.0f;
constexpr float TerrainProbeNearYards = 3.0f;
constexpr float TerrainProbeFarYards = 6.0f;
constexpr float TerrainDangerBackDropYards = 5.0f;
constexpr float TerrainPuntAwareBackDropYards = 2.5f;
constexpr float TerrainImprovementRequiredYards = 1.5f;
constexpr float TerrainPuntAwareImprovementRequiredYards = 0.75f;
constexpr float TerrainRearSupportProbeYards = 4.0f;
constexpr float TerrainPreferredRearSupportYards = 1.0f;
constexpr float TerrainSurveyRadiusYards = 30.0f;
constexpr float TerrainRelaxedProjectionMinMoveYards = 0.5f;
constexpr float HalfPi = 1.57079632679f;
constexpr float QuarterPi = 0.78539816339f;
constexpr float Pi = 3.14159265359f;

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

    // Reserve is intentionally not a "stop doing offense" mode. It means
    // "stay above a floor for utility if possible," while tanks and hybrids
    // still keep pressure/threat tools flowing and let spell costs naturally
    // reject actions they can no longer afford.
    return conserving && mode == model::BotCombatConservationMode::Conservative;
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

bool IsConsecrationSpell(std::uint32_t spellId)
{
    if (spellId == 0)
        return false;

    std::uint32_t const firstRank = sSpellMgr->GetFirstSpellInChain(spellId);
    return (firstRank != 0 ? firstRank : spellId) == ConsecrationBaseSpellId;
}

bool IsJudgementOfWisdomSpell(std::uint32_t spellId)
{
    if (spellId == 0)
        return false;

    std::uint32_t const firstRank = sSpellMgr->GetFirstSpellInChain(spellId);
    return (firstRank != 0 ? firstRank : spellId) == JudgementOfWisdomBaseSpellId;
}

bool CreatureHasKnownSpell(Creature const* creature, std::uint32_t spellId)
{
    if (!creature || spellId == 0)
        return false;

    for (uint32 i = 0; i < MAX_CREATURE_SPELLS; ++i)
    {
        if (creature->m_spells[i] == spellId)
            return true;
    }

    return false;
}

bool IsProjectedWorldBotSpellCandidate(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    if (service::ShouldAutoCastWorldBotPassiveSpell(spellInfo))
        return false;

    if (spellInfo->IsPassive())
        return false;

    return true;
}

bool NeedsPullStandoff(float distance)
{
    return distance < AmbientPullStandoffMinRangeYards
        || distance > AmbientPullStandoffMaxRangeYards;
}

struct PullStandoffBand
{
    float minRange = AmbientPullStandoffMinRangeYards;
    float maxRange = AmbientPullStandoffMaxRangeYards;
    float preferredRange = AmbientPullStandoffPreferredRangeYards;
};

PullStandoffBand ResolvePullStandoffBand(Unit const* actor, Unit const* target)
{
    PullStandoffBand band;
    if (!actor || !target)
        return band;

    if (target->IsHostileTo(actor) && !target->IsInCombat())
    {
        band.minRange = AmbientHostilePullStandoffMinRangeYards;
        band.maxRange = AmbientHostilePullStandoffMaxRangeYards;
        band.preferredRange = AmbientHostilePullStandoffPreferredRangeYards;
    }

    return band;
}

bool NeedsPullStandoff(float distance, PullStandoffBand const& band)
{
    return distance < band.minRange || distance > band.maxRange;
}

bool TryComputePullStandoffPoint(Creature* mover, Unit* target, float desiredRange, Position& out)
{
    if (!mover || !target)
        return false;

    float angle = target->GetAngle(mover);
    if (mover->GetDistance(target) < 0.5f)
        angle = target->GetOrientation() + Pi;

    out.Relocate(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), angle);
    out.m_positionX = target->GetPositionX() + std::cos(angle) * desiredRange;
    out.m_positionY = target->GetPositionY() + std::sin(angle) * desiredRange;
    out.m_positionZ = target->GetPositionZ();
    mover->UpdateGroundPositionZ(out.m_positionX, out.m_positionY, out.m_positionZ);
    return true;
}

void EnsureMutualThreatEngagement(Creature* attacker, Unit* victim);

std::uint32_t FindBestKnownSpellInChain(
    std::unordered_set<std::uint32_t> const& knownSpells,
    std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (knownSpells.count(candidate))
            return candidate;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }

    return 0u;
}

int32 AuraRemainingMsFromChain(Unit const* target, std::uint32_t baseSpellId)
{
    if (!target || baseSpellId == 0)
        return 0;

    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (Aura const* aura = target->GetAura(candidate))
        {
            int32 const remainingMs = aura->GetDuration();
            return remainingMs > 0 ? remainingMs : 0;
        }

        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }

    return 0;
}

bool HasAuraFromChain(Unit const* target, std::uint32_t baseSpellId)
{
    return AuraRemainingMsFromChain(target, baseSpellId) > 0;
}

std::uint8_t GetAuraStackCountFromChain(
    Unit const* target,
    std::uint32_t baseSpellId,
    ObjectGuid const& casterGuid = ObjectGuid::Empty)
{
    if (!target || baseSpellId == 0)
        return 0;

    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        Aura const* aura = casterGuid.IsEmpty()
            ? target->GetAura(candidate)
            : target->GetAura(candidate, casterGuid);
        if (aura)
            return aura->GetStackAmount();

        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }

    return 0;
}

bool AuraNeedsRefresh(Unit const* target, std::uint32_t baseSpellId, std::uint16_t thresholdSecs)
{
    int32 const remainingMs = AuraRemainingMsFromChain(target, baseSpellId);
    if (remainingMs == 0)
        return true;

    return remainingMs < static_cast<int32>(thresholdSecs) * 1000;
}

std::uint32_t GetPreferredSeal(std::unordered_set<std::uint32_t> const& knownSpells)
{
    if (std::uint32_t const spellId = FindBestKnownSpellInChain(knownSpells, 31801))
        return spellId;
    if (std::uint32_t const spellId = FindBestKnownSpellInChain(knownSpells, 53736))
        return spellId;
    if (std::uint32_t const spellId = FindBestKnownSpellInChain(knownSpells, 20375))
        return spellId;
    return FindBestKnownSpellInChain(knownSpells, 20154);
}

bool HasSealActive(Unit const* unit)
{
    if (!unit)
        return false;

    static constexpr std::array<std::uint32_t, 7> SealBases = {
        20154, // Seal of Righteousness
        20375, // Seal of Command
        31801, // Seal of Vengeance
        53736, // Seal of Corruption
        19854, // Seal of Wisdom
        20165, // Seal of Light
        20164, // Seal of Justice
    };

    for (std::uint32_t const baseSpellId : SealBases)
    {
        if (HasAuraFromChain(unit, baseSpellId))
            return true;
    }

    return false;
}

struct RoguePoisonLoadout
{
    std::uint32_t primaryBaseSpellId = 0;
    std::uint32_t primarySpellId = 0;
    std::uint32_t secondaryBaseSpellId = 0;
    std::uint32_t secondarySpellId = 0;
};

RoguePoisonLoadout ResolvePreferredRoguePoisons(
    std::string const& canonicalSpecKey,
    std::unordered_set<std::uint32_t> const& knownSpells)
{
    constexpr std::uint32_t DeadlyPoisonBaseSpellId = 2818u;
    constexpr std::uint32_t InstantPoisonBaseSpellId = 8680u;

    RoguePoisonLoadout result;
    std::uint32_t const deadlyPoisonSpellId =
        FindBestKnownSpellInChain(knownSpells, DeadlyPoisonBaseSpellId);
    std::uint32_t const instantPoisonSpellId =
        FindBestKnownSpellInChain(knownSpells, InstantPoisonBaseSpellId);

    if (canonicalSpecKey == "Assassination")
    {
        result.primaryBaseSpellId = DeadlyPoisonBaseSpellId;
        result.primarySpellId = deadlyPoisonSpellId;
        result.secondaryBaseSpellId = InstantPoisonBaseSpellId;
        result.secondarySpellId = instantPoisonSpellId;
    }
    else
    {
        result.primaryBaseSpellId = InstantPoisonBaseSpellId;
        result.primarySpellId = instantPoisonSpellId;
        result.secondaryBaseSpellId = DeadlyPoisonBaseSpellId;
        result.secondarySpellId = deadlyPoisonSpellId;
    }

    if (result.primarySpellId == 0)
    {
        result.primaryBaseSpellId = result.secondaryBaseSpellId;
        result.primarySpellId = result.secondarySpellId;
    }

    if (result.secondarySpellId == 0 || result.secondarySpellId == result.primarySpellId)
    {
        result.secondaryBaseSpellId = 0;
        result.secondarySpellId = 0;
    }

    return result;
}

bool RecentlyCastTimedSpell(
    living_world::ai::WorldBotCreatureAI::TimedSpellMemory const& memory,
    std::uint32_t nowMs,
    std::uint32_t spellBaseId,
    ObjectGuid const& targetGuid,
    std::uint32_t windowMs)
{
    return memory.active
        && memory.spellBaseId == spellBaseId
        && memory.targetGuid == targetGuid
        && nowMs >= memory.castWorldMs
        && (nowMs - memory.castWorldMs) < windowMs;
}

void RememberTimedSpell(
    living_world::ai::WorldBotCreatureAI::TimedSpellMemory& memory,
    std::uint32_t nowMs,
    std::uint32_t spellBaseId,
    ObjectGuid const& targetGuid)
{
    memory.active = true;
    memory.spellBaseId = spellBaseId;
    memory.targetGuid = targetGuid;
    memory.castWorldMs = nowMs;
}

struct TerrainFootingSample
{
    enum class RejectReason : std::uint8_t
    {
        None = 0,
        NoMap = 1,
        NoHeight = 2,
        ReachFailed = 3,
        ProjectionStuck = 4,
    };

    enum class ValidationMode : std::uint8_t
    {
        StrictReach = 0,
        RelaxedProjection = 1,
    };

    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float backDrop = 0.0f;
    float sideDrop = 0.0f;
    float rearSupportDistance = TerrainRearSupportProbeYards;
    float travelDelta = 0.0f;
    RejectReason rejectReason = RejectReason::None;
    ValidationMode validationMode = ValidationMode::StrictReach;
};

struct TerrainSurveyDiagnostics
{
    std::uint32_t attempted = 0;
    std::uint32_t valid = 0;
    std::uint32_t tooFar = 0;
    std::uint32_t noMap = 0;
    std::uint32_t noHeight = 0;
    std::uint32_t reachFailed = 0;
    std::uint32_t projectionStuck = 0;
    std::uint32_t relaxedProjection = 0;
};

char const* DescribeTerrainRejectReason(TerrainFootingSample::RejectReason reason)
{
    switch (reason)
    {
        case TerrainFootingSample::RejectReason::NoMap:
            return "no_map";
        case TerrainFootingSample::RejectReason::NoHeight:
            return "no_height";
        case TerrainFootingSample::RejectReason::ReachFailed:
            return "reach_failed";
        case TerrainFootingSample::RejectReason::ProjectionStuck:
            return "projection_stuck";
        case TerrainFootingSample::RejectReason::None:
        default:
            return "none";
    }
}

void RecordTerrainSampleReject(
    TerrainSurveyDiagnostics* diagnostics,
    TerrainFootingSample::RejectReason reason)
{
    if (!diagnostics)
        return;

    switch (reason)
    {
        case TerrainFootingSample::RejectReason::NoMap:
            ++diagnostics->noMap;
            break;
        case TerrainFootingSample::RejectReason::NoHeight:
            ++diagnostics->noHeight;
            break;
        case TerrainFootingSample::RejectReason::ReachFailed:
            ++diagnostics->reachFailed;
            break;
        case TerrainFootingSample::RejectReason::ProjectionStuck:
            ++diagnostics->projectionStuck;
            break;
        case TerrainFootingSample::RejectReason::None:
        default:
            break;
    }
}

std::string DescribeTerrainDiagnostics(TerrainSurveyDiagnostics const& diagnostics)
{
    std::ostringstream oss;
    oss << " attempted=" << diagnostics.attempted
        << " valid=" << diagnostics.valid
        << " too_far=" << diagnostics.tooFar
        << " no_map=" << diagnostics.noMap
        << " no_height=" << diagnostics.noHeight
        << " reach_failed=" << diagnostics.reachFailed
        << " projection_stuck=" << diagnostics.projectionStuck
        << " relaxed_projection=" << diagnostics.relaxedProjection;
    return oss.str();
}

TerrainFootingSample EvaluateCombatFootingAt(
    Creature* mover,
    Unit const* target,
    float candidateX,
    float candidateY,
    float facingAngle)
{
    TerrainFootingSample sample;
    if (!mover || !target || !mover->GetMap())
    {
        sample.rejectReason = TerrainFootingSample::RejectReason::NoMap;
        return sample;
    }

    float const candidateZ = mover->GetMapHeight(
        candidateX,
        candidateY,
        std::max(mover->GetPositionZ(), target->GetPositionZ()) + 5.0f,
        true,
        25.0f);
    if (!std::isfinite(candidateZ))
    {
        sample.rejectReason = TerrainFootingSample::RejectReason::NoHeight;
        return sample;
    }

    float reachX = candidateX;
    float reachY = candidateY;
    float reachZ = candidateZ;
    if (!mover->GetMap()->CanReachPositionAndGetValidCoords(mover, reachX, reachY, reachZ, true, true))
    {
        Position projected = mover->GetPosition();
        float const stepDistance = std::clamp(
            std::hypot(candidateX - mover->GetPositionX(), candidateY - mover->GetPositionY()),
            0.0f,
            8.0f);
        if (stepDistance < TerrainRelaxedProjectionMinMoveYards)
        {
            sample.rejectReason = TerrainFootingSample::RejectReason::ProjectionStuck;
            return sample;
        }

        mover->MovePositionToFirstCollision(projected, stepDistance, mover->GetAngle(candidateX, candidateY));
        float const projectedMoveDistance = std::hypot(
            projected.GetPositionX() - mover->GetPositionX(),
            projected.GetPositionY() - mover->GetPositionY());
        if (projectedMoveDistance < TerrainRelaxedProjectionMinMoveYards)
        {
            sample.rejectReason = TerrainFootingSample::RejectReason::ProjectionStuck;
            return sample;
        }

        reachX = projected.GetPositionX();
        reachY = projected.GetPositionY();
        reachZ = projected.GetPositionZ();
        sample.validationMode = TerrainFootingSample::ValidationMode::RelaxedProjection;
    }

    float worstBackDrop = 0.0f;
    for (float const backOffset : { 0.0f, QuarterPi, -QuarterPi })
    {
        float const backAngle = facingAngle + backOffset;
        for (float const probeDistance : { TerrainProbeNearYards, TerrainProbeFarYards })
        {
            float const probeX = reachX + (std::cos(backAngle) * probeDistance);
            float const probeY = reachY + (std::sin(backAngle) * probeDistance);
            float const probeZ = mover->GetMapHeight(probeX, probeY, reachZ + 5.0f, true, 25.0f);
            if (!std::isfinite(probeZ))
                continue;

            worstBackDrop = std::max(worstBackDrop, std::max(0.0f, reachZ - probeZ));
        }
    }

    float worstSideDrop = 0.0f;
    for (float const sideOffset : { HalfPi, -HalfPi })
    {
        float const sideAngle = facingAngle + sideOffset;
        for (float const probeDistance : { TerrainProbeNearYards, TerrainProbeFarYards })
        {
            float const probeX = reachX + (std::cos(sideAngle) * probeDistance);
            float const probeY = reachY + (std::sin(sideAngle) * probeDistance);
            float const probeZ = mover->GetMapHeight(probeX, probeY, reachZ + 5.0f, true, 25.0f);
            if (!std::isfinite(probeZ))
                continue;

            worstSideDrop = std::max(worstSideDrop, std::max(0.0f, reachZ - probeZ));
        }
    }

    sample.valid = true;
    sample.x = reachX;
    sample.y = reachY;
    sample.z = reachZ;
    sample.backDrop = worstBackDrop;
    sample.sideDrop = worstSideDrop;
    Position rearProbe;
    rearProbe.Relocate(reachX, reachY, reachZ, 0.0f);
    mover->MovePositionToFirstCollision(rearProbe, TerrainRearSupportProbeYards, facingAngle + Pi);
    sample.rearSupportDistance = std::hypot(
        rearProbe.GetPositionX() - reachX,
        rearProbe.GetPositionY() - reachY);
    sample.travelDelta = std::hypot(reachX - mover->GetPositionX(), reachY - mover->GetPositionY());
    sample.rejectReason = TerrainFootingSample::RejectReason::None;
    return sample;
}

struct TerrainSurveyCandidate
{
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float facingAngle = 0.0f;
    float orbitOffset = 0.0f;
    float orbitRadius = 0.0f;
    float backDrop = 1000.0f;
    float sideDrop = 1000.0f;
    float rearSupportDistance = TerrainRearSupportProbeYards;
    float travelDelta = 0.0f;
    float score = std::numeric_limits<float>::max();
};

bool CanReuseTerrainSurveyCache(
    WorldBotCreatureAI::TerrainSurveyCacheSnapshot const& cache,
    Creature const* mover,
    Unit const* target,
    float preferredRange,
    bool puntAwareTarget,
    float maxTravelDistance)
{
    if (!cache.valid || !mover || !target)
        return false;

    if (cache.targetGuid != target->GetGUID())
        return false;

    if (cache.puntAwareTarget != puntAwareTarget)
        return false;

    if (std::fabs(cache.preferredRange - preferredRange) > 2.0f)
        return false;

    if (std::fabs(cache.maxTravelDistance - maxTravelDistance) > 4.0f)
        return false;

    if (std::hypot(cache.moverX - mover->GetPositionX(), cache.moverY - mover->GetPositionY()) > 2.0f)
        return false;

    if (std::hypot(cache.targetX - target->GetPositionX(), cache.targetY - target->GetPositionY()) > 2.0f)
        return false;

    return true;
}

float ScoreTerrainSurveyCandidate(
    TerrainFootingSample const& sample,
    bool puntAwareTarget,
    float radiusAdjust,
    float angleOffset)
{
    float const desiredRearSupport = puntAwareTarget
        ? TerrainPreferredRearSupportYards
        : TerrainRearSupportProbeYards;
    float const rearSupportPenalty = std::fabs(sample.rearSupportDistance - desiredRearSupport) * 0.5f;
    float const orbitPenalty = std::fabs(angleOffset) * 0.15f;
    float const radiusPenalty = std::fabs(radiusAdjust - 2.0f) * 0.1f;

    return (sample.backDrop * 8.0f)
        + (sample.sideDrop * 2.0f)
        + rearSupportPenalty
        + (sample.travelDelta * 0.2f)
        + orbitPenalty
        + radiusPenalty;
}

TerrainSurveyCandidate FindBestCombatFootingSurveyAroundTarget(
    Creature* mover,
    Unit const* target,
    float preferredRange,
    bool puntAwareTarget,
    float maxTravelDistance,
    std::initializer_list<float> orbitRadii,
    std::initializer_list<float> orbitOffsets,
    TerrainSurveyDiagnostics* diagnostics = nullptr)
{
    TerrainSurveyCandidate best;
    if (!mover || !target || !mover->GetMap())
        return best;

    for (float const radiusAdjust : orbitRadii)
    {
        float const orbitRadius = std::max(1.25f, preferredRange + radiusAdjust);
        for (float const angleOffset : orbitOffsets)
        {
            float const angle = target->GetAngle(mover) + angleOffset;
            float const candidateX = target->GetPositionX() + (std::cos(angle) * orbitRadius);
            float const candidateY = target->GetPositionY() + (std::sin(angle) * orbitRadius);
            if (std::hypot(candidateX - mover->GetPositionX(), candidateY - mover->GetPositionY()) > maxTravelDistance)
            {
                if (diagnostics)
                    ++diagnostics->tooFar;
                continue;
            }

            if (diagnostics)
                ++diagnostics->attempted;

            TerrainFootingSample const sample =
                EvaluateCombatFootingAt(mover, target, candidateX, candidateY, angle);
            if (!sample.valid)
            {
                RecordTerrainSampleReject(diagnostics, sample.rejectReason);
                continue;
            }

            if (diagnostics)
            {
                ++diagnostics->valid;
                if (sample.validationMode == TerrainFootingSample::ValidationMode::RelaxedProjection)
                    ++diagnostics->relaxedProjection;
            }

            TerrainSurveyCandidate candidate;
            candidate.valid = true;
            candidate.x = sample.x;
            candidate.y = sample.y;
            candidate.z = sample.z;
            candidate.facingAngle = angle;
            candidate.orbitOffset = angleOffset;
            candidate.orbitRadius = orbitRadius;
            candidate.backDrop = sample.backDrop;
            candidate.sideDrop = sample.sideDrop;
            candidate.rearSupportDistance = sample.rearSupportDistance;
            candidate.travelDelta = sample.travelDelta;
            candidate.score = ScoreTerrainSurveyCandidate(
                sample,
                puntAwareTarget,
                radiusAdjust,
                angleOffset);

            if (!best.valid || candidate.score < best.score)
                best = candidate;
        }
    }

    return best;
}

void ConsiderTerrainSurveyCandidate(
    TerrainSurveyCandidate& best,
    Creature* mover,
    Unit const* target,
    bool puntAwareTarget,
    float anchorX,
    float anchorY,
    float orbitRadius,
    float angleOffset,
    float scoreRadiusAdjust = 0.0f,
    TerrainSurveyDiagnostics* diagnostics = nullptr)
{
    if (!mover || !target)
        return;

    float const angle = target->GetAngle(anchorX, anchorY);
    if (diagnostics)
        ++diagnostics->attempted;

    TerrainFootingSample const sample =
        EvaluateCombatFootingAt(mover, target, anchorX, anchorY, angle);
    if (!sample.valid)
    {
        RecordTerrainSampleReject(diagnostics, sample.rejectReason);
        return;
    }

    if (diagnostics)
    {
        ++diagnostics->valid;
        if (sample.validationMode == TerrainFootingSample::ValidationMode::RelaxedProjection)
            ++diagnostics->relaxedProjection;
    }

    TerrainSurveyCandidate candidate;
    candidate.valid = true;
    candidate.x = sample.x;
    candidate.y = sample.y;
    candidate.z = sample.z;
    candidate.facingAngle = angle;
    candidate.orbitOffset = angleOffset;
    candidate.orbitRadius = orbitRadius;
    candidate.backDrop = sample.backDrop;
    candidate.sideDrop = sample.sideDrop;
    candidate.rearSupportDistance = sample.rearSupportDistance;
    candidate.travelDelta = sample.travelDelta;
    candidate.score = ScoreTerrainSurveyCandidate(
        sample,
        puntAwareTarget,
        scoreRadiusAdjust,
        angleOffset);

    if (!best.valid || candidate.score < best.score)
        best = candidate;
}

TerrainSurveyCandidate FindBestCombatFootingSurveyNearFight(
    Creature* mover,
    Unit const* target,
    bool puntAwareTarget,
    float maxTravelDistance,
    TerrainSurveyDiagnostics* diagnostics = nullptr)
{
    TerrainSurveyCandidate best;
    if (!mover || !target)
        return best;

    float const moverX = mover->GetPositionX();
    float const moverY = mover->GetPositionY();
    float const targetX = target->GetPositionX();
    float const targetY = target->GetPositionY();
    float const midpointX = (moverX + targetX) * 0.5f;
    float const midpointY = (moverY + targetY) * 0.5f;

    auto const samplePatch =
        [&](float centerX, float centerY, std::initializer_list<float> radii)
        {
            for (float const radius : radii)
            {
                if (radius <= 0.001f)
                {
                    if (std::hypot(centerX - moverX, centerY - moverY) <= maxTravelDistance)
                    {
                        ConsiderTerrainSurveyCandidate(
                            best,
                            mover,
                            target,
                            puntAwareTarget,
                            centerX,
                            centerY,
                            0.0f,
                            0.0f,
                            0.0f,
                            diagnostics);
                    }
                    else if (diagnostics)
                    {
                        ++diagnostics->tooFar;
                    }
                    continue;
                }

                for (float const angleOffset : {
                         0.0f,
                         QuarterPi,
                         -QuarterPi,
                         HalfPi,
                         -HalfPi,
                         HalfPi + QuarterPi,
                         -(HalfPi + QuarterPi),
                         Pi })
                {
                    float const candidateX = centerX + (std::cos(angleOffset) * radius);
                    float const candidateY = centerY + (std::sin(angleOffset) * radius);
                    if (std::hypot(candidateX - moverX, candidateY - moverY) > maxTravelDistance)
                    {
                        if (diagnostics)
                            ++diagnostics->tooFar;
                        continue;
                    }

                    ConsiderTerrainSurveyCandidate(
                        best,
                        mover,
                        target,
                        puntAwareTarget,
                        candidateX,
                        candidateY,
                        radius,
                        angleOffset,
                        radius,
                        diagnostics);
                }
            }
        };

    samplePatch(moverX, moverY, { 0.0f, 3.0f, 6.0f, 9.0f });
    samplePatch(midpointX, midpointY, { 0.0f, 3.0f, 6.0f });

    if (mover->GetDistance2d(target) > 12.0f)
    {
        float const corridorFractions[] = { 0.25f, 0.5f, 0.75f };
        for (float const t : corridorFractions)
        {
            float const cx = moverX + ((targetX - moverX) * t);
            float const cy = moverY + ((targetY - moverY) * t);
            samplePatch(cx, cy, { 0.0f, 2.5f, 5.0f });
        }
    }

    return best;
}

TerrainSurveyCandidate MakeTerrainSurveyCandidateFromCache(
    WorldBotCreatureAI::TerrainSurveyCacheSnapshot const& cache)
{
    TerrainSurveyCandidate candidate;
    if (!cache.valid)
        return candidate;

    candidate.valid = true;
    candidate.x = cache.bestX;
    candidate.y = cache.bestY;
    candidate.z = cache.bestZ;
    candidate.facingAngle = cache.bestFacingAngle;
    candidate.orbitOffset = cache.bestOrbitOffset;
    candidate.orbitRadius = cache.bestOrbitRadius;
    candidate.backDrop = cache.bestBackDrop;
    candidate.sideDrop = cache.bestSideDrop;
    candidate.rearSupportDistance = cache.bestRearSupportDistance;
    candidate.travelDelta = cache.bestTravelDelta;
    candidate.score = cache.bestScore;
    return candidate;
}

void StoreTerrainSurveyCache(
    WorldBotCreatureAI::TerrainSurveyCacheSnapshot& cache,
    Creature const* mover,
    Unit const* target,
    float preferredRange,
    bool puntAwareTarget,
    float maxTravelDistance,
    TerrainSurveyCandidate const& candidate)
{
    cache.Reset();
    if (!mover || !target || !candidate.valid)
        return;

    cache.valid = true;
    cache.targetGuid = target->GetGUID();
    cache.puntAwareTarget = puntAwareTarget;
    cache.moverX = mover->GetPositionX();
    cache.moverY = mover->GetPositionY();
    cache.targetX = target->GetPositionX();
    cache.targetY = target->GetPositionY();
    cache.preferredRange = preferredRange;
    cache.maxTravelDistance = maxTravelDistance;
    cache.bestX = candidate.x;
    cache.bestY = candidate.y;
    cache.bestZ = candidate.z;
    cache.bestFacingAngle = candidate.facingAngle;
    cache.bestOrbitOffset = candidate.orbitOffset;
    cache.bestOrbitRadius = candidate.orbitRadius;
    cache.bestBackDrop = candidate.backDrop;
    cache.bestSideDrop = candidate.sideDrop;
    cache.bestRearSupportDistance = candidate.rearSupportDistance;
    cache.bestTravelDelta = candidate.travelDelta;
    cache.bestScore = candidate.score;
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
    std::string   subjectKind;
    std::string   subjectKey;
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
                metadata.subjectKind = step.subjectKind;
                metadata.subjectKey = step.subjectKey;
                break;
            }
        }

        if (stepIndex == 0)
            break;
        --stepIndex;
    }

    return metadata;
}

struct RuntimeLedgerBreadcrumbs
{
    std::string taskActivityKey;
    std::string questHubKey;
    std::uint64_t questHubElapsedMs = 0;
};

bool IsQuestHubSubjectKeyLocal(std::string const& subjectKey)
{
    return subjectKey.starts_with("quest_hub:");
}

std::string ExtractQuestHubIdLocal(std::string const& subjectKey)
{
    if (!IsQuestHubSubjectKeyLocal(subjectKey))
        return {};

    std::string const tail = subjectKey.substr(std::string("quest_hub:").size());
    std::size_t const comma = tail.find(',');
    return comma == std::string::npos ? tail : tail.substr(0, comma);
}

RuntimeLedgerBreadcrumbs BuildRuntimeLedgerBreadcrumbs(
    service::AmbientSession const& session,
    std::size_t currentStep,
    std::uint32_t stepElapsedMs)
{
    RuntimeLedgerBreadcrumbs breadcrumbs;
    if (session.steps.empty())
        return breadcrumbs;

    std::size_t stepIndex = session.steps.size() - 1u;
    std::uint64_t effectiveStepElapsedMs = stepElapsedMs;
    if (currentStep < session.steps.size())
        stepIndex = currentStep;
    else
        effectiveStepElapsedMs = static_cast<std::uint64_t>(session.steps[stepIndex].durationSec) * 1000ull;

    service::AmbientStep const& activeStep = session.steps[stepIndex];
    if (activeStep.taskIndex >= 0)
    {
        std::size_t const taskIndex = static_cast<std::size_t>(activeStep.taskIndex);
        if (taskIndex < session.tasks.size())
            breadcrumbs.taskActivityKey = session.tasks[taskIndex].activityKey;
    }

    breadcrumbs.questHubKey = ExtractQuestHubIdLocal(activeStep.subjectKey);
    if (breadcrumbs.questHubKey.empty())
        return breadcrumbs;

    breadcrumbs.questHubElapsedMs = effectiveStepElapsedMs;
    while (stepIndex > 0u)
    {
        std::size_t const previousIndex = stepIndex - 1u;
        service::AmbientStep const& previousStep = session.steps[previousIndex];
        if (ExtractQuestHubIdLocal(previousStep.subjectKey) != breadcrumbs.questHubKey)
            break;

        breadcrumbs.questHubElapsedMs += static_cast<std::uint64_t>(previousStep.durationSec) * 1000ull;
        stepIndex = previousIndex;
    }

    return breadcrumbs;
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
    static integration::SqlBotGlyphTemplateRepository glyphTemplateRepository;
    static integration::SqlBotTalentTemplateRepository talentTemplateRepository;
    static integration::SqlBotVirtualLoadoutRepository virtualLoadoutRepository;
    static service::WorldBotPreparationService preparationService(
        defaultProfileRepository,
        glyphTemplateRepository,
        talentTemplateRepository,
        virtualLoadoutRepository);
    return preparationService;
}

service::WorldBotAssignedGearService& GetWorldBotAssignedGearService()
{
    static integration::SqlBotAssignedGearRepository assignedGearRepository;
    static integration::SqlBotAssignedGearTemplateRepository assignedGearTemplateRepository;
    static service::WorldBotAssignedGearService assignedGearService(
        assignedGearRepository,
        assignedGearTemplateRepository);
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

char const* DescribeTravelNavigationPolicy(WorldBotCreatureAI::TravelNavigationPolicy policy)
{
    switch (policy)
    {
        case WorldBotCreatureAI::TravelNavigationPolicy::LocalOnly:
            return "local_only";
        case WorldBotCreatureAI::TravelNavigationPolicy::LocalWithPoiConnector:
            return "local_with_poi_connector";
        case WorldBotCreatureAI::TravelNavigationPolicy::LocalWithAssist:
            return "local_with_assist";
        case WorldBotCreatureAI::TravelNavigationPolicy::MacroTravel:
        default:
            return "macro_travel";
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

std::string DescribeSessionProfile(service::AmbientSession const& session)
{
    std::uint64_t totalWorkSec = 0;
    std::uint64_t totalQuestWorkSec = 0;
    std::uint64_t totalTransitSec = 0;
    std::uint64_t totalTravelSec = 0;
    std::vector<std::string> families;

    for (service::AmbientSessionTask const& task : session.tasks)
    {
        if (!task.taskFamily.empty()
            && std::find(families.begin(), families.end(), task.taskFamily) == families.end())
        {
            families.push_back(task.taskFamily);
        }
    }

    for (service::AmbientStep const& step : session.steps)
    {
        switch (step.type)
        {
            case service::AmbientStepType::Travel:
                totalTravelSec += step.durationSec;
                break;
            case service::AmbientStepType::Transit:
                totalTransitSec += step.durationSec;
                break;
            default:
            {
                totalWorkSec += step.durationSec;
                if (step.taskIndex >= 0)
                {
                    std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
                    if (taskIndex < session.tasks.size())
                    {
                        std::string family = session.tasks[taskIndex].taskFamily;
                        std::transform(
                            family.begin(),
                            family.end(),
                            family.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (family == "quest" || family == "questing")
                            totalQuestWorkSec += step.durationSec;
                    }
                }
                break;
            }
        }
    }

    std::ostringstream oss;
    oss << "families='";
    for (std::size_t i = 0; i < families.size(); ++i)
    {
        if (i != 0)
            oss << ",";
        oss << families[i];
    }
    oss << "' work_sec=" << totalWorkSec
        << " quest_work_sec=" << totalQuestWorkSec
        << " travel_sec=" << totalTravelSec
        << " transit_sec=" << totalTransitSec;
    return oss.str();
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

std::string ResolveStepTargetPointKey(
    service::AmbientSession const& session,
    service::AmbientStep const& step)
{
    if (!step.targetPointKey.empty())
        return step.targetPointKey;

    if (step.taskIndex < 0)
        return {};

    std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
    if (taskIndex >= session.tasks.size())
        return {};

    return session.tasks[taskIndex].targetPointKey;
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

std::string DescribeDamageTypeForTrace(DamageEffectType damageType)
{
    switch (damageType)
    {
        case DIRECT_DAMAGE:
            return "direct";
        case SPELL_DIRECT_DAMAGE:
            return "spell_direct";
        case DOT:
            return "dot";
        case HEAL:
            return "heal";
        case NODAMAGE:
            return "nodamage";
        case SELF_DAMAGE:
            return "self_damage";
        default:
            return "unknown";
    }
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
    {
        if (!action.itemSelector.empty())
            return "ItemSelector(" + action.itemSelector + "->" + std::to_string(action.itemId) + "@slot=" + std::to_string(action.equippedSlot) + ")";
        return "Item(" + std::to_string(action.itemId) + ")";
    }

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

float GetPlayerStyleHealthBonusFromStamina(float stamina)
{
    float const baseStamina = stamina < 20.0f ? stamina : 20.0f;
    float const bonusStamina = stamina - baseStamina;
    return baseStamina + (bonusStamina * 10.0f);
}

float GetPlayerStyleManaBonusFromIntellect(float intellect)
{
    float const baseIntellect = intellect < 20.0f ? intellect : 20.0f;
    float const bonusIntellect = intellect - baseIntellect;
    return baseIntellect + (bonusIntellect * 15.0f);
}

void RefreshWorldBotPlayerLikeDerivedStats(Creature& bot)
{
    for (std::uint8_t statIndex = STAT_STRENGTH; statIndex < MAX_STATS; ++statIndex)
    {
        Stats const stat = Stats(statIndex);
        bot.SetStat(stat, static_cast<int32>(bot.GetTotalStatValue(stat)));
    }

    {
        float armor = bot.GetFlatModifierValue(UNIT_MOD_ARMOR, BASE_VALUE);
        armor *= bot.GetPctModifierValue(UNIT_MOD_ARMOR, BASE_PCT);
        armor += bot.GetStat(STAT_AGILITY) * 2.0f;
        armor += bot.GetFlatModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE);

        auto const& resistanceOfStatPct =
            bot.GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
        for (auto const* auraEffect : resistanceOfStatPct)
        {
            if (auraEffect && (auraEffect->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL))
                armor += static_cast<float>(bot.GetStat(Stats(auraEffect->GetMiscValueB())))
                    * static_cast<float>(auraEffect->GetAmount()) / 100.0f;
        }

        armor *= bot.GetPctModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT);
        bot.SetArmor(static_cast<int32>(std::max(0.0f, armor)));
    }

    {
        float health = bot.GetFlatModifierValue(UNIT_MOD_HEALTH, BASE_VALUE)
            + static_cast<float>(bot.GetCreateHealth());
        health *= bot.GetPctModifierValue(UNIT_MOD_HEALTH, BASE_PCT);
        health += bot.GetFlatModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE)
            + GetPlayerStyleHealthBonusFromStamina(bot.GetStat(STAT_STAMINA));
        health *= bot.GetPctModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT);
        bot.SetMaxHealth(static_cast<std::uint32_t>(std::max(1.0f, health)));
    }

    {
        float mana = bot.GetFlatModifierValue(UNIT_MOD_MANA, BASE_VALUE)
            + static_cast<float>(bot.GetCreateMana());
        mana *= bot.GetPctModifierValue(UNIT_MOD_MANA, BASE_PCT);
        if (bot.GetCreateMana() > 0)
            mana += GetPlayerStyleManaBonusFromIntellect(bot.GetStat(STAT_INTELLECT));
        mana += bot.GetFlatModifierValue(UNIT_MOD_MANA, TOTAL_VALUE);
        mana *= bot.GetPctModifierValue(UNIT_MOD_MANA, TOTAL_PCT);
        bot.SetMaxPower(POWER_MANA, static_cast<std::uint32_t>(std::max(0.0f, mana)));
    }

    for (std::uint8_t school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
    {
        float const value = bot.GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        bot.SetResistance(SpellSchools(school), static_cast<int32>(std::max(0.0f, value)));
    }

    bot.UpdateAttackPowerAndDamage();
    bot.UpdateAttackPowerAndDamage(true);
}

void ResetWorldBotUnitModifierState(Creature& bot)
{
    bot.InitStatBuffMods();

    for (std::uint8_t unitModIndex = 0; unitModIndex < UNIT_MOD_END; ++unitModIndex)
    {
        UnitMods const unitMod = UnitMods(unitModIndex);
        bot.SetStatFlatModifier(unitMod, BASE_VALUE, 0.0f);
        bot.SetStatFlatModifier(unitMod, TOTAL_VALUE, 0.0f);
        bot.SetStatPctModifier(unitMod, BASE_PCT, 1.0f);
        bot.SetStatPctModifier(unitMod, TOTAL_PCT, 1.0f);
    }
}

ItemTemplate const* FindAssignedWeaponTemplateForSlot(
    model::WorldBotPreparedBuild const& preparedBuild,
    std::uint8_t slot)
{
    for (model::WorldBotAssignedGearEntry const& entry : preparedBuild.assignedGear)
    {
        if (entry.slot != slot)
            continue;

        return sObjectMgr->GetItemTemplate(entry.itemId);
    }

    return nullptr;
}

void ApplyWorldBotWeaponTemplate(
    Creature& bot,
    WeaponAttackType attackType,
    ItemTemplate const* itemTemplate)
{
    for (std::uint8_t damageIndex = 0; damageIndex < MAX_ITEM_PROTO_DAMAGES; ++damageIndex)
    {
        bot.SetBaseWeaponDamage(attackType, MINDAMAGE, 0.0f, damageIndex);
        bot.SetBaseWeaponDamage(attackType, MAXDAMAGE, 0.0f, damageIndex);
    }

    if (attackType == BASE_ATTACK)
    {
        bot.SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, BASE_MINDAMAGE);
        bot.SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, BASE_MAXDAMAGE);
    }

    bot.SetAttackTime(attackType, BASE_ATTACK_TIME);

    if (!itemTemplate)
        return;

    for (std::uint8_t damageIndex = 0; damageIndex < MAX_ITEM_PROTO_DAMAGES; ++damageIndex)
    {
        float const minDamage = itemTemplate->Damage[damageIndex].DamageMin;
        float const maxDamage = itemTemplate->Damage[damageIndex].DamageMax;

        if (minDamage > 0.0f)
            bot.SetBaseWeaponDamage(attackType, MINDAMAGE, minDamage, damageIndex);
        if (maxDamage > 0.0f)
            bot.SetBaseWeaponDamage(attackType, MAXDAMAGE, maxDamage, damageIndex);
    }

    if (itemTemplate->Delay > 0)
        bot.SetAttackTime(attackType, itemTemplate->Delay);
}

struct ResolvedWorldBotItemSetBonus
{
    std::uint32_t setId = 0;
    std::uint32_t spellId = 0;
    std::uint32_t pieceCount = 0;
    std::uint32_t requiredPieces = 0;
    bool skillRestricted = false;
    bool missingSpellInfo = false;
    bool autocastEligible = false;
};

std::vector<ResolvedWorldBotItemSetBonus> ResolveWorldBotItemSetBonuses(
    model::WorldBotPreparedBuild const& preparedBuild)
{
    std::unordered_map<std::uint32_t, std::uint32_t> pieceCounts;
    for (model::WorldBotAssignedGearEntry const& entry : preparedBuild.assignedGear)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
        if (!itemTemplate || itemTemplate->ItemSet == 0)
            continue;

        ++pieceCounts[itemTemplate->ItemSet];
    }

    std::vector<ResolvedWorldBotItemSetBonus> bonuses;
    for (auto const& [setId, pieceCount] : pieceCounts)
    {
        ItemSetEntry const* setEntry = sItemSetStore.LookupEntry(setId);
        if (!setEntry)
            continue;

        bool const skillRestricted = setEntry->required_skill_id != 0;
        for (std::uint8_t index = 0; index < MAX_ITEM_SET_SPELLS; ++index)
        {
            std::uint32_t const spellId = setEntry->spells[index];
            std::uint32_t const requiredPieces = setEntry->items_to_triggerspell[index];
            if (spellId == 0 || requiredPieces == 0 || pieceCount < requiredPieces)
                continue;

            ResolvedWorldBotItemSetBonus bonus;
            bonus.setId = setId;
            bonus.spellId = spellId;
            bonus.pieceCount = pieceCount;
            bonus.requiredPieces = requiredPieces;
            bonus.skillRestricted = skillRestricted;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo)
            {
                bonus.missingSpellInfo = true;
            }
            else if (!skillRestricted
                && spellInfo->HasAnyAura()
                && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL)
                && service::ShouldAutoCastWorldBotPassiveSpell(spellInfo))
            {
                bonus.autocastEligible = true;
            }

            bonuses.push_back(bonus);
        }
    }

    std::sort(
        bonuses.begin(),
        bonuses.end(),
        [](ResolvedWorldBotItemSetBonus const& left, ResolvedWorldBotItemSetBonus const& right)
        {
            if (left.setId != right.setId)
                return left.setId < right.setId;
            if (left.requiredPieces != right.requiredPieces)
                return left.requiredPieces < right.requiredPieces;
            return left.spellId < right.spellId;
        });

    return bonuses;
}

std::string DescribeWorldBotItemSetBonuses(
    std::vector<ResolvedWorldBotItemSetBonus> const& bonuses)
{
    if (bonuses.empty())
        return "set_bonuses=none";

    std::ostringstream oss;
    std::size_t applied = 0;
    std::size_t deferred = 0;
    std::size_t skillLocked = 0;
    std::size_t missingSpell = 0;
    for (ResolvedWorldBotItemSetBonus const& bonus : bonuses)
    {
        if (bonus.autocastEligible)
            ++applied;
        else if (bonus.missingSpellInfo)
            ++missingSpell;
        else if (bonus.skillRestricted)
            ++skillLocked;
        else
            ++deferred;
    }

    oss << "set_bonuses_total=" << bonuses.size()
        << " applied=" << applied
        << " deferred=" << deferred
        << " skill_locked=" << skillLocked
        << " missing_spell=" << missingSpell
        << " details='";

    bool first = true;
    for (ResolvedWorldBotItemSetBonus const& bonus : bonuses)
    {
        if (!first)
            oss << "; ";
        first = false;

        oss << "set=" << bonus.setId
            << " spell=" << bonus.spellId
            << " pieces=" << bonus.pieceCount
            << "/" << bonus.requiredPieces
            << " mode=";

        if (bonus.autocastEligible)
            oss << "autocast";
        else if (bonus.missingSpellInfo)
            oss << "missing_spell";
        else if (bonus.skillRestricted)
            oss << "skill_locked";
        else
            oss << "deferred";
    }

    oss << "'";
    return oss.str();
}

std::vector<std::uint32_t> ParseWorldBotEnchantmentValues(std::string const& text)
{
    std::vector<std::uint32_t> values;
    std::size_t cursor = 0;

    while (cursor < text.size())
    {
        while (cursor < text.size()
            && std::isspace(static_cast<unsigned char>(text[cursor])))
        {
            ++cursor;
        }

        if (cursor >= text.size())
            break;

        char* end = nullptr;
        unsigned long const parsed = std::strtoul(text.c_str() + cursor, &end, 10);
        if (end == text.c_str() + cursor)
        {
            values.push_back(0u);
            ++cursor;
            continue;
        }

        values.push_back(static_cast<std::uint32_t>(parsed));
        cursor = static_cast<std::size_t>(end - text.c_str());
    }

    return values;
}

struct ResolvedWorldBotEnchantmentSpellBonus
{
    enum class Mode : std::uint8_t
    {
        AutoCast = 0,
        DeferredCombatProc = 1,
        DeferredWeaponDamage = 2,
        DeferredOther = 3,
        MissingSpellInfo = 4,
    };

    std::uint8_t equipmentSlot = 0;
    std::uint32_t itemId = 0;
    std::uint32_t enchantId = 0;
    std::uint32_t spellId = 0;
    Mode mode = Mode::DeferredOther;
};

struct ResolvedWorldBotItemEquipSpellBonus
{
    enum class Mode
    {
        AutoCast,
        DeferredChanceOnHit,
        DeferredOther,
        MissingSpellInfo
    };

    std::uint8_t equipmentSlot = 0;
    std::uint32_t itemId = 0;
    std::uint32_t spellId = 0;
    std::uint32_t triggerType = 0;
    Mode mode = Mode::DeferredOther;
};

std::vector<ResolvedWorldBotItemEquipSpellBonus> ResolveWorldBotItemEquipSpellBonuses(
    model::WorldBotPreparedBuild const& preparedBuild)
{
    std::vector<ResolvedWorldBotItemEquipSpellBonus> bonuses;

    for (model::WorldBotAssignedGearEntry const& entry : preparedBuild.assignedGear)
    {
        if (entry.itemId == 0)
            continue;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
        if (!itemTemplate)
            continue;

        for (std::uint8_t i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            _Spell const& spellData = itemTemplate->Spells[i];
            if (spellData.SpellId <= 0)
                continue;

            if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP
                && spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
            {
                continue;
            }

            ResolvedWorldBotItemEquipSpellBonus bonus;
            bonus.equipmentSlot = entry.slot;
            bonus.itemId = entry.itemId;
            bonus.spellId = static_cast<std::uint32_t>(spellData.SpellId);
            bonus.triggerType = spellData.SpellTrigger;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(bonus.spellId);
            if (!spellInfo)
            {
                bonus.mode = ResolvedWorldBotItemEquipSpellBonus::Mode::MissingSpellInfo;
            }
            else if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP
                && spellInfo->HasAnyAura()
                && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
            {
                bonus.mode = ResolvedWorldBotItemEquipSpellBonus::Mode::AutoCast;
            }
            else if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
            {
                bonus.mode = ResolvedWorldBotItemEquipSpellBonus::Mode::DeferredChanceOnHit;
            }
            else
            {
                bonus.mode = ResolvedWorldBotItemEquipSpellBonus::Mode::DeferredOther;
            }

            bonuses.push_back(bonus);
        }
    }

    std::sort(
        bonuses.begin(),
        bonuses.end(),
        [](ResolvedWorldBotItemEquipSpellBonus const& left,
           ResolvedWorldBotItemEquipSpellBonus const& right)
        {
            if (left.equipmentSlot != right.equipmentSlot)
                return left.equipmentSlot < right.equipmentSlot;
            if (left.itemId != right.itemId)
                return left.itemId < right.itemId;
            if (left.spellId != right.spellId)
                return left.spellId < right.spellId;
            return left.triggerType < right.triggerType;
        });

    bonuses.erase(
        std::unique(
            bonuses.begin(),
            bonuses.end(),
            [](ResolvedWorldBotItemEquipSpellBonus const& left,
               ResolvedWorldBotItemEquipSpellBonus const& right)
            {
                return left.equipmentSlot == right.equipmentSlot
                    && left.itemId == right.itemId
                    && left.spellId == right.spellId
                    && left.triggerType == right.triggerType
                    && left.mode == right.mode;
            }),
        bonuses.end());

    return bonuses;
}

char const* DescribeWorldBotItemEquipBonusMode(
    ResolvedWorldBotItemEquipSpellBonus::Mode mode)
{
    switch (mode)
    {
        case ResolvedWorldBotItemEquipSpellBonus::Mode::AutoCast:
            return "autocast";
        case ResolvedWorldBotItemEquipSpellBonus::Mode::DeferredChanceOnHit:
            return "deferred_chance_on_hit";
        case ResolvedWorldBotItemEquipSpellBonus::Mode::MissingSpellInfo:
            return "missing_spell";
        case ResolvedWorldBotItemEquipSpellBonus::Mode::DeferredOther:
        default:
            return "deferred_other";
    }
}

std::string DescribeWorldBotItemEquipSpellBonuses(
    std::vector<ResolvedWorldBotItemEquipSpellBonus> const& bonuses)
{
    if (bonuses.empty())
        return "item_equip_spell_bonuses=none";

    std::size_t applied = 0;
    std::size_t deferredChanceOnHit = 0;
    std::size_t deferredOther = 0;
    std::size_t missingSpell = 0;

    for (ResolvedWorldBotItemEquipSpellBonus const& bonus : bonuses)
    {
        switch (bonus.mode)
        {
            case ResolvedWorldBotItemEquipSpellBonus::Mode::AutoCast:
                ++applied;
                break;
            case ResolvedWorldBotItemEquipSpellBonus::Mode::DeferredChanceOnHit:
                ++deferredChanceOnHit;
                break;
            case ResolvedWorldBotItemEquipSpellBonus::Mode::MissingSpellInfo:
                ++missingSpell;
                break;
            case ResolvedWorldBotItemEquipSpellBonus::Mode::DeferredOther:
            default:
                ++deferredOther;
                break;
        }
    }

    std::ostringstream oss;
    oss << "item_equip_spell_bonuses_total=" << bonuses.size()
        << " applied=" << applied
        << " deferred_chance_on_hit=" << deferredChanceOnHit
        << " deferred_other=" << deferredOther
        << " missing_spell=" << missingSpell
        << " details='";

    bool first = true;
    for (ResolvedWorldBotItemEquipSpellBonus const& bonus : bonuses)
    {
        if (!first)
            oss << "; ";
        first = false;

        oss << "slot=" << static_cast<std::uint32_t>(bonus.equipmentSlot)
            << " item=" << bonus.itemId
            << " spell=" << bonus.spellId
            << " trigger=" << bonus.triggerType
            << " mode=" << DescribeWorldBotItemEquipBonusMode(bonus.mode);
    }

    oss << "'";
    return oss.str();
}

std::vector<ResolvedWorldBotEnchantmentSpellBonus> ResolveWorldBotEnchantmentSpellBonuses(
    model::WorldBotPreparedBuild const& preparedBuild)
{
    std::vector<ResolvedWorldBotEnchantmentSpellBonus> bonuses;

    for (model::WorldBotAssignedGearEntry const& entry : preparedBuild.assignedGear)
    {
        if (entry.enchantments.empty())
            continue;

        std::vector<std::uint32_t> const values =
            ParseWorldBotEnchantmentValues(entry.enchantments);
        if (values.empty())
            continue;

        for (std::size_t base = 0; base < values.size(); base += MAX_ENCHANTMENT_OFFSET)
        {
            std::uint32_t const enchantId = values[base];
            if (enchantId == 0)
                continue;

            SpellItemEnchantmentEntry const* enchantEntry =
                sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!enchantEntry)
                continue;

            for (std::size_t effectIndex = 0;
                 effectIndex < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS;
                 ++effectIndex)
            {
                std::uint32_t const effectType = enchantEntry->type[effectIndex];
                std::uint32_t const spellId = enchantEntry->spellid[effectIndex];

                if ((effectType != ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL
                    && effectType != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL
                    && effectType != ITEM_ENCHANTMENT_TYPE_DAMAGE)
                    || spellId == 0)
                {
                    continue;
                }

                ResolvedWorldBotEnchantmentSpellBonus bonus;
                bonus.equipmentSlot = entry.slot;
                bonus.itemId = entry.itemId;
                bonus.enchantId = enchantId;
                bonus.spellId = spellId;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
                if (!spellInfo)
                {
                    bonus.mode = ResolvedWorldBotEnchantmentSpellBonus::Mode::MissingSpellInfo;
                }
                else if (effectType == ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL
                    && spellInfo->HasAnyAura()
                    && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL)
                    && service::ShouldAutoCastWorldBotPassiveSpell(spellInfo))
                {
                    bonus.mode = ResolvedWorldBotEnchantmentSpellBonus::Mode::AutoCast;
                }
                else if (effectType == ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                {
                    bonus.mode = ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredCombatProc;
                }
                else if (effectType == ITEM_ENCHANTMENT_TYPE_DAMAGE)
                {
                    bonus.mode = ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredWeaponDamage;
                }
                else
                {
                    bonus.mode = ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredOther;
                }

                bonuses.push_back(bonus);
            }
        }
    }

    std::sort(
        bonuses.begin(),
        bonuses.end(),
        [](ResolvedWorldBotEnchantmentSpellBonus const& left,
           ResolvedWorldBotEnchantmentSpellBonus const& right)
        {
            if (left.equipmentSlot != right.equipmentSlot)
                return left.equipmentSlot < right.equipmentSlot;
            if (left.itemId != right.itemId)
                return left.itemId < right.itemId;
            if (left.enchantId != right.enchantId)
                return left.enchantId < right.enchantId;
            return left.spellId < right.spellId;
        });

    bonuses.erase(
        std::unique(
            bonuses.begin(),
            bonuses.end(),
            [](ResolvedWorldBotEnchantmentSpellBonus const& left,
               ResolvedWorldBotEnchantmentSpellBonus const& right)
            {
                return left.equipmentSlot == right.equipmentSlot
                    && left.itemId == right.itemId
                    && left.enchantId == right.enchantId
                    && left.spellId == right.spellId
                    && left.mode == right.mode;
            }),
        bonuses.end());

    return bonuses;
}

char const* DescribeWorldBotEnchantmentBonusMode(
    ResolvedWorldBotEnchantmentSpellBonus::Mode mode)
{
    switch (mode)
    {
        case ResolvedWorldBotEnchantmentSpellBonus::Mode::AutoCast:
            return "autocast";
        case ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredCombatProc:
            return "deferred_combat_proc";
        case ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredWeaponDamage:
            return "deferred_weapon_damage";
        case ResolvedWorldBotEnchantmentSpellBonus::Mode::MissingSpellInfo:
            return "missing_spell";
        case ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredOther:
        default:
            return "deferred_other";
    }
}

std::string DescribeWorldBotEnchantmentSpellBonuses(
    std::vector<ResolvedWorldBotEnchantmentSpellBonus> const& bonuses)
{
    if (bonuses.empty())
        return "enchant_spell_bonuses=none";

    std::size_t applied = 0;
    std::size_t deferredCombatProc = 0;
    std::size_t deferredWeaponDamage = 0;
    std::size_t deferredOther = 0;
    std::size_t missingSpell = 0;

    for (ResolvedWorldBotEnchantmentSpellBonus const& bonus : bonuses)
    {
        switch (bonus.mode)
        {
            case ResolvedWorldBotEnchantmentSpellBonus::Mode::AutoCast:
                ++applied;
                break;
            case ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredCombatProc:
                ++deferredCombatProc;
                break;
            case ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredWeaponDamage:
                ++deferredWeaponDamage;
                break;
            case ResolvedWorldBotEnchantmentSpellBonus::Mode::MissingSpellInfo:
                ++missingSpell;
                break;
            case ResolvedWorldBotEnchantmentSpellBonus::Mode::DeferredOther:
            default:
                ++deferredOther;
                break;
        }
    }

    std::ostringstream oss;
    oss << "enchant_spell_bonuses_total=" << bonuses.size()
        << " applied=" << applied
        << " deferred_combat_proc=" << deferredCombatProc
        << " deferred_weapon_damage=" << deferredWeaponDamage
        << " deferred_other=" << deferredOther
        << " missing_spell=" << missingSpell
        << " details='";

    bool first = true;
    for (ResolvedWorldBotEnchantmentSpellBonus const& bonus : bonuses)
    {
        if (!first)
            oss << "; ";
        first = false;

        oss << "slot=" << static_cast<std::uint32_t>(bonus.equipmentSlot)
            << " item=" << bonus.itemId
            << " enchant=" << bonus.enchantId
            << " spell=" << bonus.spellId
            << " mode=" << DescribeWorldBotEnchantmentBonusMode(bonus.mode);
    }

    oss << "'";
    return oss.str();
}

EquipmentSlots ResolveWorldBotReactiveProcEquipmentSlot(WeaponAttackType attackType)
{
    switch (attackType)
    {
        case BASE_ATTACK:
            return EQUIPMENT_SLOT_MAINHAND;
        case OFF_ATTACK:
            return EQUIPMENT_SLOT_OFFHAND;
        case RANGED_ATTACK:
            return EQUIPMENT_SLOT_RANGED;
        default:
            return EQUIPMENT_SLOT_END;
    }
}

char const* DescribeWorldBotReactiveAttackType(WeaponAttackType attackType)
{
    switch (attackType)
    {
        case BASE_ATTACK:
            return "mainhand";
        case OFF_ATTACK:
            return "offhand";
        case RANGED_ATTACK:
            return "ranged";
        default:
            return "unknown";
    }
}

bool IsWorldBotReactiveProcEligibleForAttackType(
    model::WorldBotAssignedGearEntry const& entry,
    ItemTemplate const* itemTemplate,
    WeaponAttackType attackType)
{
    if (!itemTemplate)
        return false;

    if (itemTemplate->Class != ITEM_CLASS_WEAPON)
        return true;

    return entry.slot == ResolveWorldBotReactiveProcEquipmentSlot(attackType);
}

bool HasWorldBotAssignedGearSlot(
    model::WorldBotPreparedBuild const& preparedBuild,
    std::uint8_t slot)
{
    auto const it = std::find_if(
        preparedBuild.assignedGear.begin(),
        preparedBuild.assignedGear.end(),
        [slot](model::WorldBotAssignedGearEntry const& entry)
        {
            return entry.slot == slot && entry.itemId != 0;
        });

    return it != preparedBuild.assignedGear.end();
}

WeaponAttackType ResolveWorldBotReactiveProcAttackType(
    Creature const* actor,
    Unit const* victim,
    model::WorldBotPreparedBuild const& preparedBuild,
    DamageEffectType damageType)
{
    bool const hasOffhand = HasWorldBotAssignedGearSlot(preparedBuild, EQUIPMENT_SLOT_OFFHAND);
    bool const hasRanged = HasWorldBotAssignedGearSlot(preparedBuild, EQUIPMENT_SLOT_RANGED);

    if (actor && victim && hasRanged && !actor->IsWithinMeleeRange(victim))
        return RANGED_ATTACK;

    if (actor && hasOffhand && damageType == DIRECT_DAMAGE)
    {
        bool const mainhandReady = actor->isAttackReady(BASE_ATTACK);
        bool const offhandReady = actor->isAttackReady(OFF_ATTACK);
        if (mainhandReady && !offhandReady)
            return OFF_ATTACK;
        if (!mainhandReady && offhandReady)
            return BASE_ATTACK;
    }

    return BASE_ATTACK;
}

struct WorldBotWeaponEnchantDamageBonus
{
    WeaponAttackType attackType = MAX_ATTACK;
    float amount = 0.0f;
    std::vector<std::string> details;
};

std::array<WorldBotWeaponEnchantDamageBonus, MAX_ATTACK> ResolveWorldBotWeaponEnchantDamageBonuses(
    model::WorldBotPreparedBuild const& preparedBuild,
    std::uint8_t classId)
{
    std::array<WorldBotWeaponEnchantDamageBonus, MAX_ATTACK> bonuses = {{
        { BASE_ATTACK, 0.0f, {} },
        { OFF_ATTACK, 0.0f, {} },
        { RANGED_ATTACK, 0.0f, {} },
    }};

    for (model::WorldBotAssignedGearEntry const& entry : preparedBuild.assignedGear)
    {
        if (entry.enchantments.empty())
            continue;

        WeaponAttackType const attackType = Player::GetAttackBySlot(entry.slot);
        if (attackType == MAX_ATTACK)
            continue;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
        if (!itemTemplate)
            continue;

        std::vector<std::uint32_t> const values =
            ParseWorldBotEnchantmentValues(entry.enchantments);
        if (values.empty())
            continue;

        for (std::size_t base = 0; base < values.size(); base += MAX_ENCHANTMENT_OFFSET)
        {
            std::uint32_t const enchantId = values[base];
            if (enchantId == 0)
                continue;

            SpellItemEnchantmentEntry const* enchantEntry =
                sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!enchantEntry)
                continue;

            for (std::size_t effectIndex = 0;
                 effectIndex < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS;
                 ++effectIndex)
            {
                float amount = 0.0f;
                switch (enchantEntry->type[effectIndex])
                {
                    case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                        amount = static_cast<float>(enchantEntry->amount[effectIndex]);
                        break;
                    case ITEM_ENCHANTMENT_TYPE_TOTEM:
                        if (classId == CLASS_SHAMAN)
                        {
                            amount = static_cast<float>(enchantEntry->amount[effectIndex])
                                * static_cast<float>(itemTemplate->Delay) / 1000.0f;
                        }
                        break;
                    default:
                        break;
                }

                if (amount == 0.0f)
                    continue;

                WorldBotWeaponEnchantDamageBonus& bonus = bonuses[attackType];
                bonus.amount += amount;

                std::ostringstream detail;
                detail << "slot=" << static_cast<std::uint32_t>(entry.slot)
                       << " item=" << entry.itemId
                       << " enchant=" << enchantId
                       << " amount=" << amount;
                bonus.details.push_back(detail.str());
            }
        }
    }

    return bonuses;
}

char const* DescribeWorldBotWeaponEnchantAttackType(WeaponAttackType attackType)
{
    switch (attackType)
    {
        case BASE_ATTACK:
            return "mainhand";
        case OFF_ATTACK:
            return "offhand";
        case RANGED_ATTACK:
            return "ranged";
        default:
            return "unknown";
    }
}

std::string DescribeWorldBotWeaponEnchantDamageBonuses(
    std::array<WorldBotWeaponEnchantDamageBonus, MAX_ATTACK> const& bonuses)
{
    bool any = false;
    for (WorldBotWeaponEnchantDamageBonus const& bonus : bonuses)
    {
        if (bonus.amount > 0.0f)
        {
            any = true;
            break;
        }
    }

    if (!any)
        return "weapon_enchant_damage=none";

    std::ostringstream oss;
    oss << "weapon_enchant_damage='";
    bool firstAttack = true;
    for (WorldBotWeaponEnchantDamageBonus const& bonus : bonuses)
    {
        if (bonus.amount <= 0.0f)
            continue;

        if (!firstAttack)
            oss << "; ";
        firstAttack = false;

        oss << DescribeWorldBotWeaponEnchantAttackType(bonus.attackType)
            << "=" << bonus.amount;
        if (!bonus.details.empty())
        {
            oss << " [";
            for (std::size_t i = 0; i < bonus.details.size(); ++i)
            {
                if (i != 0)
                    oss << ", ";
                oss << bonus.details[i];
            }
            oss << "]";
        }
    }
    oss << "'";
    return oss.str();
}

std::string DescribeWorldBotPreparedGlyphs(
    std::vector<model::WorldBotPreparedGlyphEntry> const& glyphs)
{
    if (glyphs.empty())
        return "glyphs=none";

    std::ostringstream oss;
    oss << "glyphs_total=" << glyphs.size() << " details='";
    bool first = true;
    for (model::WorldBotPreparedGlyphEntry const& glyph : glyphs)
    {
        if (!first)
            oss << "; ";
        first = false;

        oss << "slot=" << static_cast<std::uint32_t>(glyph.slotIndex)
            << " glyph=" << glyph.glyphId
            << " spell=" << glyph.spellId;
    }
    oss << "'";
    return oss.str();
}

std::string DescribeControlledPetEffectSnapshot(Creature* owner, Creature* pet)
{
    if (!owner)
        return "owner=none";

    std::ostringstream oss;
    oss << "owner='" << owner->GetName() << "'";
    oss << " owner_class=" << static_cast<std::uint32_t>(owner->getClass());
    oss << " owner_master_of_ghouls=" << (owner->HasAura(MasterOfGhoulsSpellId) ? 1 : 0);
    oss << " owner_glyph_of_the_ghoul=" << (owner->HasAura(GlyphOfTheGhoulSpellId) ? 1 : 0);
    oss << " owner_glyph_of_eternal_water=" << (owner->HasAura(70937u) ? 1 : 0);
    oss << " owner_glyph_of_felguard=" << (owner->HasAura(GlyphOfFelguardSpellId) ? 1 : 0);
    oss << " owner_glyph_of_felhunter=" << (owner->HasAura(GlyphOfFelhunterSpellId) ? 1 : 0);
    oss << " owner_glyph_of_voidwalker=" << (owner->HasAura(GlyphOfVoidwalkerSpellId) ? 1 : 0);

    if (!pet)
    {
        oss << " pet=none";
        return oss.str();
    }

    oss << " pet='" << pet->GetName() << "'";
    oss << " pet_entry=" << pet->GetEntry();
    oss << " pet_hp=" << pet->GetMaxHealth();
    oss << " pet_ap=" << pet->GetInt32Value(UNIT_FIELD_ATTACK_POWER);
    oss << " pet_str=" << pet->GetStat(STAT_STRENGTH);
    oss << " pet_sta=" << pet->GetStat(STAT_STAMINA);
    oss << " pet_owner=" << (pet->GetOwnerGUID() == owner->GetGUID() ? 1 : 0);
    return oss.str();
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
    _idleWatchdogEnabled =
        sConfigMgr->GetOption<bool>("LivingWorld.DebugIdleWatchdogEnabled", false);
    _idleWatchdogKillProcess =
        sConfigMgr->GetOption<bool>("LivingWorld.DebugIdleWatchdogAbortProcess", false);
    _idleWatchdogConfig.stagnantLimitMs =
        std::max<std::uint32_t>(
            1000u,
            sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugIdleWatchdogNoMovementMs", 300000u));
    _idleWatchdogConfig.timeoutMs = std::numeric_limits<std::uint32_t>::max();
    _idleWatchdogConfig.progressThreshold =
        std::max<float>(
            0.1f,
            sConfigMgr->GetOption<float>("LivingWorld.DebugIdleWatchdogProgressThreshold", 2.0f));
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
    bool resumedFromAbstract,
    std::uint32_t completedSessionsThisActivation)
{
    std::uint8_t const previousIdentityLevel =
        (_identity.id != 0 && _identity.id == identity.id) ? _identity.level : 0u;

    _identity     = identity;
    _identity.specKey = model::CanonicalizeBotSpecKey(_identity.specKey);
    _session      = session;
    _currentStep  = currentStep;
    _activityTimer = stepElapsedMs;
    _traveling    = false;
    _sessionDone  = false;
    _sessionReady = true;
    _worldOnlineMs = worldOnlineMsSoFar;
    _completedSessionsThisActivation = completedSessionsThisActivation;
    _pendingLevelUpCelebration = previousIdentityLevel != 0u && _identity.level > previousIdentityLevel;
    _pendingLevelUpFromLevel = _pendingLevelUpCelebration ? previousIdentityLevel : 0u;
    _combatInterrupt = {};
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);
    _knownExploredZoneIds.clear();
    _preparedBuild = {};
    _preparedBuildReady = false;
    _assignedGearItemIds.clear();
    InvalidateCombatProfile();
    _lastDebugCombatManaDrainWorldMs = 0;
    _debugCombatManaGemObserved = false;
    _syntheticGlobalCooldownRemainingMs = 0;
    _consecrationSnapshot = {};
    _pendingCorpseRecovery = false;
    _corpseRecoveryCount = 0;
    _usedSimulatedItemsThisCombat.clear();
    _genericPotionCharges = std::min<std::uint8_t>(MaxGenericPotionCharges, _identity.genericPotionCharges);
    _simulatedPotionUsesThisSession = 0;
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();
    _pendingPullArm.Reset();
    _recentOocBuff.Reset();
    _recentPullPrep.Reset();
    _controlledPetAssistTargets.clear();
    _lastControlledPetSummonAttemptWorldMs = 0;
    _lastControlledPetStatusLogWorldMs = 0;
    ResetIdleWatchdog();

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
        _assignedGearItemIds.clear();
        for (model::WorldBotAssignedGearEntry const& entry : _preparedBuild.assignedGear)
        {
            if (entry.itemId != 0)
                _assignedGearItemIds.insert(entry.itemId);

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
                + " glyphs=" + std::to_string(_preparedBuild.glyphs.size())
                + " self_states=" + std::to_string(_preparedBuild.selfStates.size())
                + " assigned_gear_slots=" + std::to_string(_preparedBuild.assignedGear.size())
                + " assigned_gear_band=" + std::to_string(_preparedBuild.assignedGearRefreshBand)
                + " assigned_gear_refreshed=" + std::to_string(_preparedBuild.assignedGearRefreshed ? 1 : 0)
                + " ooc_buff_scope=" + std::to_string(static_cast<std::uint32_t>(_preparedBuild.oocBehavior.buffScope))
                + " ooc_buff_reapply_secs=" + std::to_string(_preparedBuild.oocBehavior.buffReapplySecs)
                + " ooc_buff_on_spawn=" + std::to_string(_preparedBuild.oocBehavior.buffOnSpawn ? 1 : 0)
                + " " + DescribeWorldBotPreparedGlyphs(_preparedBuild.glyphs)
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

    {
        std::ostringstream snapshot;
        snapshot << "max_health=" << me->GetMaxHealth()
                 << " max_mana=" << me->GetMaxPower(POWER_MANA)
                 << " health=" << me->GetHealth()
                 << " mana=" << me->GetPower(POWER_MANA)
                 << " power_type=" << static_cast<std::uint32_t>(me->getPowerType())
                 << " mh_delay=" << me->GetAttackTime(BASE_ATTACK)
                 << " oh_delay=" << me->GetAttackTime(OFF_ATTACK)
                 << " ranged_delay=" << me->GetAttackTime(RANGED_ATTACK)
                 << " mh_min=" << me->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE)
                 << " mh_max=" << me->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE)
                 << " oh_min=" << me->GetWeaponDamageRange(OFF_ATTACK, MINDAMAGE)
                 << " oh_max=" << me->GetWeaponDamageRange(OFF_ATTACK, MAXDAMAGE)
                 << " ranged_min=" << me->GetWeaponDamageRange(RANGED_ATTACK, MINDAMAGE)
                 << " ranged_max=" << me->GetWeaponDamageRange(RANGED_ATTACK, MAXDAMAGE)
                 << " final_mh_min=" << me->GetFloatValue(UNIT_FIELD_MINDAMAGE)
                 << " final_mh_max=" << me->GetFloatValue(UNIT_FIELD_MAXDAMAGE)
                 << " final_oh_min=" << me->GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE)
                 << " final_oh_max=" << me->GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE)
                 << " final_ranged_min=" << me->GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE)
                 << " final_ranged_max=" << me->GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE);
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "resource_snapshot",
            snapshot.str());
    }

    {
        auto describeAssignedItem =
            [&](std::uint8_t slot) -> std::string
            {
                auto it = std::find_if(
                    _preparedBuild.assignedGear.begin(),
                    _preparedBuild.assignedGear.end(),
                    [slot](model::WorldBotAssignedGearEntry const& entry)
                    {
                        return entry.slot == slot;
                    });
                if (it == _preparedBuild.assignedGear.end() || it->itemId == 0)
                    return "none";

                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(it->itemId);
                std::ostringstream item;
                item << it->itemId;
                if (itemTemplate)
                {
                    item << ":'" << itemTemplate->Name1 << "'"
                         << ":subclass=" << static_cast<std::uint32_t>(itemTemplate->SubClass)
                         << ":delay=" << itemTemplate->Delay;
                }
                if (it->itemLevel > 0)
                    item << ":ilvl=" << it->itemLevel;
                return item.str();
            };

        std::ostringstream snapshot;
        snapshot << "mainhand=" << describeAssignedItem(EQUIPMENT_SLOT_MAINHAND)
                 << " offhand=" << describeAssignedItem(EQUIPMENT_SLOT_OFFHAND)
                 << " ranged=" << describeAssignedItem(EQUIPMENT_SLOT_RANGED);
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "weapon_loadout_snapshot",
            snapshot.str());
    }

    for (std::uint32_t const zoneId : GetExploredZoneRepo().LoadExploredZones(_identity.id))
        _knownExploredZoneIds.insert(zoneId);

    if (!alreadyMarkedActive)
    {
        GetIdentityRepo().MarkActive(_identity.id);
        if (auto refreshedIdentity = GetIdentityRepo().FindById(_identity.id))
            _identity = *refreshedIdentity;
    }

    ObserveCurrentZoneExploration();

    if (!alreadyMarkedActive)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "session_start",
            DescribeSessionOrigin(_session)
                + " " + DescribeSessionProfile(_session)
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

    _roguePoisonState.Reset();

    ResetWorldBotUnitModifierState(*me);

    std::uint32_t resolvedFactionTemplate = _identity.faction == 2 ? FACTION_HORDE_GENERIC : FACTION_ALLIANCE_GENERIC;
    if (ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(_identity.raceId))
        resolvedFactionTemplate = raceEntry->FactionID;

    me->SetName(_identity.name);

    if (_pendingLevelUpCelebration && _pendingLevelUpFromLevel != 0u && _pendingLevelUpFromLevel < _identity.level)
        me->SetLevel(_pendingLevelUpFromLevel, false);

    me->SetLevel(_identity.level);
    me->SetDisplayId(_identity.displayId);
    me->SetFaction(resolvedFactionTemplate);
    me->SetReactState(REACT_AGGRESSIVE);
    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);
    ApplyNamedDebugRunShell();

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
    ItemTemplate const* mainHandTemplate =
        FindAssignedWeaponTemplateForSlot(_preparedBuild, EQUIPMENT_SLOT_MAINHAND);
    ItemTemplate const* offHandTemplate =
        FindAssignedWeaponTemplateForSlot(_preparedBuild, EQUIPMENT_SLOT_OFFHAND);
    ItemTemplate const* rangedTemplate =
        FindAssignedWeaponTemplateForSlot(_preparedBuild, EQUIPMENT_SLOT_RANGED);
    bool const canUseGenericOneHandOffhand =
        _identity.classId == CLASS_ROGUE;

    if (_pendingLevelUpCelebration)
    {
        me->PlayDirectSound(kLevelUpUiSoundKitId);
        me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "level_up_celebrate",
            "from_level=" + std::to_string(_pendingLevelUpFromLevel)
            + ", to_level=" + std::to_string(_identity.level)
            + ", sound_kit=" + std::to_string(kLevelUpUiSoundKitId));
        _pendingLevelUpCelebration = false;
        _pendingLevelUpFromLevel = 0u;
    }

    me->SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
    me->SetAttackTime(OFF_ATTACK, BASE_ATTACK_TIME);
    me->SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);
    me->SetCanDualWield(
        offHandTemplate
        && (offHandTemplate->InventoryType == INVTYPE_WEAPONOFFHAND
            || (canUseGenericOneHandOffhand && offHandTemplate->InventoryType == INVTYPE_WEAPON)));

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
    if (mainHandTemplate || offHandTemplate || rangedTemplate)
    {
        ApplyWorldBotWeaponTemplate(*me, BASE_ATTACK, mainHandTemplate);
        ApplyWorldBotWeaponTemplate(
            *me,
            OFF_ATTACK,
            offHandTemplate && (
                offHandTemplate->InventoryType == INVTYPE_WEAPONOFFHAND
                || (canUseGenericOneHandOffhand && offHandTemplate->InventoryType == INVTYPE_WEAPON))
                ? offHandTemplate
                : nullptr);
        ApplyWorldBotWeaponTemplate(
            *me,
            RANGED_ATTACK,
            rangedTemplate && (
                rangedTemplate->InventoryType == INVTYPE_RANGED
                || rangedTemplate->InventoryType == INVTYPE_RANGEDRIGHT
                || rangedTemplate->InventoryType == INVTYPE_THROWN)
                ? rangedTemplate
                : nullptr);
    }
    else
    {
        me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, physicalDamageBaseline.mainHandMinDamage);
        me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, physicalDamageBaseline.mainHandMaxDamage);
        me->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, physicalDamageBaseline.offHandMinDamage);
        me->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, physicalDamageBaseline.offHandMaxDamage);
        me->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, physicalDamageBaseline.rangedMinDamage);
        me->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, physicalDamageBaseline.rangedMaxDamage);
    }

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

    std::vector<ResolvedWorldBotItemSetBonus> const itemSetBonuses =
        ResolveWorldBotItemSetBonuses(_preparedBuild);
    std::vector<ResolvedWorldBotItemEquipSpellBonus> const itemEquipSpellBonuses =
        ResolveWorldBotItemEquipSpellBonuses(_preparedBuild);
    std::vector<ResolvedWorldBotEnchantmentSpellBonus> const enchantmentSpellBonuses =
        ResolveWorldBotEnchantmentSpellBonuses(_preparedBuild);
    std::array<WorldBotWeaponEnchantDamageBonus, MAX_ATTACK> const weaponEnchantDamageBonuses =
        ResolveWorldBotWeaponEnchantDamageBonuses(_preparedBuild, _identity.classId);

    for (WorldBotWeaponEnchantDamageBonus const& bonus : weaponEnchantDamageBonuses)
    {
        if (bonus.amount <= 0.0f)
            continue;

        UnitMods unitMod = UNIT_MOD_DAMAGE_MAINHAND;
        switch (bonus.attackType)
        {
            case BASE_ATTACK:
                unitMod = UNIT_MOD_DAMAGE_MAINHAND;
                break;
            case OFF_ATTACK:
                unitMod = UNIT_MOD_DAMAGE_OFFHAND;
                break;
            case RANGED_ATTACK:
                unitMod = UNIT_MOD_DAMAGE_RANGED;
                break;
            default:
                continue;
        }

        me->HandleStatFlatModifier(unitMod, TOTAL_VALUE, bonus.amount, true);
    }

    PopulateProjectedCreatureSpellbook();

    for (std::uint32_t spellId : _preparedBuild.knownSpellIds)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!service::ShouldAutoCastWorldBotPassiveSpell(spellInfo))
            continue;

        if (me->HasAura(spellId))
            continue;

        me->CastSpell(me, spellId, true);
    }

    for (ResolvedWorldBotItemSetBonus const& bonus : itemSetBonuses)
    {
        if (!bonus.autocastEligible || me->HasAura(bonus.spellId))
            continue;

        me->CastSpell(me, bonus.spellId, true);
    }

    for (ResolvedWorldBotItemEquipSpellBonus const& bonus : itemEquipSpellBonuses)
    {
        if (bonus.mode != ResolvedWorldBotItemEquipSpellBonus::Mode::AutoCast
            || me->HasAura(bonus.spellId))
        {
            continue;
        }

        me->CastSpell(me, bonus.spellId, true);
    }

    for (ResolvedWorldBotEnchantmentSpellBonus const& bonus : enchantmentSpellBonuses)
    {
        if (bonus.mode != ResolvedWorldBotEnchantmentSpellBonus::Mode::AutoCast
            || me->HasAura(bonus.spellId))
        {
            continue;
        }

        me->CastSpell(me, bonus.spellId, true);
    }

    for (model::WorldBotPreparedGlyphEntry const& glyph : _preparedBuild.glyphs)
    {
        if (glyph.spellId == 0 || me->HasAura(glyph.spellId))
            continue;

        me->CastSpell(me, glyph.spellId, true);
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "set_bonus_snapshot",
        DescribeWorldBotItemSetBonuses(itemSetBonuses));
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "item_equip_bonus_snapshot",
        DescribeWorldBotItemEquipSpellBonuses(itemEquipSpellBonuses));
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "enchant_bonus_snapshot",
        DescribeWorldBotEnchantmentSpellBonuses(enchantmentSpellBonuses));
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "glyph_snapshot",
        DescribeWorldBotPreparedGlyphs(_preparedBuild.glyphs));
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "glyph_effect_snapshot",
        DescribeControlledPetEffectSnapshot(me, GetControlledGuardianPet()));
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "weapon_enchant_snapshot",
        DescribeWorldBotWeaponEnchantDamageBonuses(weaponEnchantDamageBonuses));

    RefreshWorldBotPlayerLikeDerivedStats(*me);
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

namespace
{
std::string DescribeThreatStateForTrace(Unit* actor, Unit* currentVictim);
std::string DescribeAttackValidityForTrace(Creature* actor, Unit* target);

bool IsEligibleWorldBotControlledPet(Creature const* owner, Creature const* pet)
{
    if (!owner || !pet)
        return false;

    if (pet->GetOwnerGUID() != owner->GetGUID())
        return false;

    if (!(pet->IsGuardian() || pet->HasUnitTypeMask(UNIT_MASK_CONTROLLABLE_GUARDIAN)))
        return false;

    return true;
}

void AppendWorldBotControlledPet(std::vector<Creature*>& pets, Creature* owner, Creature* pet)
{
    if (!IsEligibleWorldBotControlledPet(owner, pet))
        return;

    for (Creature* existing : pets)
    {
        if (existing && existing->GetGUID() == pet->GetGUID())
            return;
    }

    pets.push_back(pet);
}
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

    if (TickIdleWatchdog(TickIntervalMs))
        return;

    ObserveCurrentZoneExploration();
    TryApplyOutOfCombatBuff();
    if (TryMaintainBasicCompanionPet())
        return;
    SyncControlledGuardianPetFollow();
    model::BotCombatMode const forcedBotMode = ResolveDebugForcedBotMode();
    bool const suppressOwnerOffense =
        forcedBotMode == model::BotCombatMode::Passive
        || forcedBotMode == model::BotCombatMode::Hold;
    bool const suppressOwnerMovement =
        forcedBotMode == model::BotCombatMode::Hold;
    if (me->IsNonMeleeSpellCast(false))
        return;
    TickAmbientGroupDistressState();
    if (TickPendingPullArm(TickIntervalMs))
        return;

    if (!_debugForcedCombatProbeLogged)
    {
        std::uint32_t const forcedIdentityId =
            sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceCombatTargetIdentityId", 0);
        if (forcedIdentityId != 0 && forcedIdentityId == _identity.id)
        {
            std::ostringstream probe;
            probe << "phase='debug' decision='force_target_probe' "
                  << "self_identity=" << _identity.id << " "
                  << "forced_identity=" << forcedIdentityId << " "
                  << "target_entry=" << sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceCombatTargetEntry", 0) << " "
                  << "can_interrupt=" << (CanInterruptCurrentStepForCombat() ? 1 : 0) << " "
                  << "runtime_state='" << DescribeRuntimeStateKey() << "'";
            RecordCombatTrace(probe.str());
            _debugForcedCombatProbeLogged = true;
        }
    }

    if (!suppressOwnerOffense)
        MaybeStartDebugForcedCombat();

    auto const rememberCombatObservation =
        [this]()
        {
            _lastUpdateObservedCombat = me && (me->IsInCombat() || me->GetVictim());
            _lastUpdateObservedVictimGuid = (me && me->GetVictim()) ? me->GetVictim()->GetGUID() : ObjectGuid();
        };

    bool const hasCombatNow = me->IsInCombat() || me->GetVictim();
    if (_identity.activeWorldSessionBudgetMs != 0
        && _worldOnlineMs >= _identity.activeWorldSessionBudgetMs
        && !hasCombatNow
        && !_combatInterrupt.active)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "session_budget_elapsed",
            "world_online_ms=" + std::to_string(_worldOnlineMs)
                + " budget_ms=" + std::to_string(_identity.activeWorldSessionBudgetMs)
                + " current_step=" + std::to_string(_currentStep));
        CompletSession();
        return;
    }

    if (IsDebugForcedCombatIdentity() && _lastUpdateObservedCombat && !hasCombatNow)
    {
        Unit* lastVictim = !_lastUpdateObservedVictimGuid.IsEmpty()
            ? ObjectAccessor::GetUnit(*me, _lastUpdateObservedVictimGuid)
            : nullptr;
        std::ostringstream dropTrace;
        dropTrace << "phase='combat' decision='combat_drop_reason' "
                  << "last_victim=" << DescribeTraceUnit(lastVictim) << " "
                  << DescribeAttackValidityForTrace(me, lastVictim) << " "
                  << "bot_combat=" << (me->IsInCombat() ? 1 : 0) << " "
                  << "bot_engaged=" << (me->IsEngaged() ? 1 : 0) << " "
                  << "bot_attackers=" << me->getAttackers().size() << " "
                  << "interrupt=" << (_combatInterrupt.active ? 1 : 0);
        RecordCombatTrace(dropTrace.str());
    }

    if (hasCombatNow)
    {
        rememberCombatObservation();
        if (suppressOwnerOffense)
        {
            if (Unit* victim = me->GetVictim())
                SyncControlledGuardianPetAssist(victim);
            else if (!me->getAttackers().empty())
                SyncControlledGuardianPetDefend(*me->getAttackers().begin());

            me->AttackStop();
            return;
        }
        if (Unit* victim = me->GetVictim())
            RefreshAmbientPursuitState(victim);
        else
            _ambientPursuitState.Reset();
        if (TryHandleAggressivePursuitTimeout())
            return;
        if (TryHandleCowardCombatFlee())
            return;
        TickCombat(TickIntervalMs);
        return;
    }

    _ambientPursuitState.Reset();

    if (!suppressOwnerOffense && TryHandleAmbientPlayerEncounter())
    {
        rememberCombatObservation();
        if (me->IsInCombat() || me->GetVictim())
            TickCombat(TickIntervalMs);
        return;
    }

    if (!suppressOwnerOffense && TryJoinNearbyAmbientCombat("nearby_group_assist"))
    {
        rememberCombatObservation();
        TickCombat(TickIntervalMs);
        return;
    }

    if (!suppressOwnerOffense && _combatInterrupt.active)
    {
        if (TryAdoptGroupedCombatTarget("group_target_request_idle"))
        {
            rememberCombatObservation();
            TickCombat(TickIntervalMs);
            return;
        }

        if (TrySustainAmbientCombat("combat_resume_from_nearby_ally"))
        {
            rememberCombatObservation();
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

        if (!_combatInterrupt.resumePending)
        {
            _combatInterrupt.resumePending = true;
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "resume_pending",
                "step='" + DescribeCurrentStep()
                    + "' reason='combat_all_clear'"
                    + " mode='" + (_combatInterrupt.reason == CombatInterruptionReason::AuthoredGrind ? "authored_grind" : "reactive_defense") + "'");
        }
    }

    if (suppressOwnerMovement)
    {
        rememberCombatObservation();
        return;
    }

    if (TryRequestSuspendedStepResume())
    {
        TickStep(TickIntervalMs);
        return;
    }

    TickStep(TickIntervalMs);

    std::uint32_t const snapshotIntervalMs =
        _session.sourceKind == "debug_path_scout"
            ? 4000u
            : (_session.sourceKind == "debug_route_harness"
                ? 5000u
                : PositionSnapshotIntervalMs);

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

    rememberCombatObservation();
}

std::string WorldBotCreatureAI::DescribeRuntimeStateKey() const
{
    if (!_sessionReady)
        return "session_loading";
    if (_pendingCorpseRecovery || (me && !me->IsAlive()))
        return "dead_pending_recovery";
    if (_ambientFleeState.active)
        return (me && (me->IsInCombat() || me->GetVictim())) ? "combat_fleeing" : "activity_fleeing";
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
    if (_ambientFleeState.active)
    {
        Unit* threat = (!_ambientFleeState.threatGuid.IsEmpty() && me)
            ? ObjectAccessor::GetUnit(*me, _ambientFleeState.threatGuid)
            : nullptr;
        std::ostringstream detail;
        detail << "Fleeing from "
               << (threat ? threat->GetName() : "threat")
               << " distance="
               << (threat && me ? me->GetDistance(threat) : _ambientFleeState.lastThreatDistance)
               << "yd"
               << " safe_at=" << AmbientCowardAvoidDistance << "yd";
        return detail.str();
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
    RuntimeLedgerBreadcrumbs const breadcrumbs =
        BuildRuntimeLedgerBreadcrumbs(_session, _currentStep, _activityTimer);

    GetIdentityRepo().UpdateActiveRuntimeState(
        _identity.id,
        me ? me->GetZoneId() : 0u,
        _worldOnlineMs,
        DescribeRuntimeStateKey(),
        detail,
        breadcrumbs.taskActivityKey,
        breadcrumbs.questHubKey,
        breadcrumbs.questHubElapsedMs);
}

void WorldBotCreatureAI::ResetIdleWatchdog()
{
    ResetTravelWatchdog(_idleWatchdog);
    _idleWatchdogLastWarningBucket = 0;
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

bool WorldBotCreatureAI::IsIdleWatchdogEligible() const
{
    if (!_idleWatchdogEnabled || !me || !_sessionReady || _sessionDone)
        return false;

    if (_pendingCorpseRecovery || !me->IsAlive())
        return false;

    if (_combatInterrupt.active || _pendingPullArm.active)
        return false;

    if (me->IsInCombat() || me->GetVictim() || me->IsEngaged() || !me->getAttackers().empty())
        return false;

    if (_currentStep >= _session.steps.size())
        return false;

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type == service::AmbientStepType::Idle)
        return false;

    if (step.type == service::AmbientStepType::Transit)
        return false;

    return true;
}

std::string WorldBotCreatureAI::BuildIdleWatchdogDetail(char const* reason, std::uint32_t stagnantMs) const
{
    std::ostringstream oss;
    oss << "reason='" << (reason ? reason : "idle_watchdog") << "' "
        << "stagnant_ms=" << stagnantMs << " "
        << "threshold_ms=" << _idleWatchdogConfig.stagnantLimitMs << " "
        << "progress_threshold=" << _idleWatchdogConfig.progressThreshold << " "
        << "world_online_ms=" << _worldOnlineMs << " "
        << "step_elapsed_ms=" << _activityTimer << " "
        << "runtime_state='" << DescribeRuntimeStateKey() << "' "
        << "step='" << DescribeCurrentStep() << "' "
        << DescribeSessionOrigin(_session);
    return oss.str();
}

bool WorldBotCreatureAI::TickIdleWatchdog(std::uint32_t diff)
{
    if (!IsIdleWatchdogEligible())
    {
        ResetIdleWatchdog();
        return false;
    }

    std::uint32_t const priorStagnantMs = _idleWatchdog.stagnantMs;
    TravelWatchdogSignal const signal = UpdateTravelWatchdog(
        _idleWatchdog,
        me->GetPositionX(),
        me->GetPositionY(),
        me->GetPositionZ(),
        diff,
        _idleWatchdogConfig);

    if (_idleWatchdog.stagnantMs == 0)
    {
        _idleWatchdogLastWarningBucket = 0;
        return false;
    }

    std::uint32_t const warningBucket = _idleWatchdog.stagnantMs / 60000u;
    if (warningBucket > 0
        && warningBucket != _idleWatchdogLastWarningBucket
        && _idleWatchdog.stagnantMs < _idleWatchdogConfig.stagnantLimitMs)
    {
        _idleWatchdogLastWarningBucket = warningBucket;
        RecordPositionSnapshot(
            "idle_watchdog_warning",
            BuildIdleWatchdogDetail("stagnant_warning", _idleWatchdog.stagnantMs));
    }

    if (signal != TravelWatchdogSignal::Stuck)
        return false;

    std::string const detail = BuildIdleWatchdogDetail("stagnant_limit_reached", _idleWatchdog.stagnantMs);
    RecordPositionSnapshot(
        _idleWatchdogKillProcess ? "idle_watchdog_abort" : "idle_watchdog_stuck",
        detail);

    if (!_idleWatchdogKillProcess)
        return false;

    LOG_ERROR(
        "module.living_world",
        "LivingWorld idle watchdog abort requested for identity {} ({}) after {} ms without movement: {}",
        _identity.id,
        _identity.name,
        _idleWatchdog.stagnantMs,
        detail);
    World::StopNow(ERROR_EXIT_CODE);
    return true;
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

void WorldBotCreatureAI::RecordEncounterTrace(std::string const& detail)
{
    if (!me || detail.empty())
        return;

    if (detail == _lastEncounterTraceDetail && (_worldOnlineMs - _lastEncounterTraceWorldMs) < 2000)
        return;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "encounter_trace",
        detail);
    _lastEncounterTraceDetail = detail;
    _lastEncounterTraceWorldMs = _worldOnlineMs;
}

char const* WorldBotCreatureAI::ResolveAmbientTruceZoneName() const
{
    return ResolveAmbientTruceBubbleName(me);
}

bool WorldBotCreatureAI::IsAmbientPlayerLikeTarget(Unit const* candidate) const
{
    if (!candidate)
        return false;

    if (candidate->ToPlayer())
        return true;

    Creature const* creature = candidate->ToCreature();
    return creature && creature->GetEntry() == WorldBotEntry;
}

Unit* WorldBotCreatureAI::FindNearbyAmbientPlayerLikeTarget(float radius) const
{
    if (!me || radius <= 0.0f)
        return nullptr;

    std::vector<Unit*> candidates;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(me, me, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, candidates, check);
    Cell::VisitObjects(me, searcher, radius);

    Unit* best = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (Unit* candidate : candidates)
    {
        if (!candidate || candidate == me || !candidate->IsAlive() || !candidate->IsInWorld())
            continue;
        if (me->IsFriendlyTo(candidate))
            continue;
        if (!candidate->isTargetableForAttack(false, me))
            continue;
        if (!IsAmbientPlayerLikeTarget(candidate))
            continue;

        float const distance = me->GetDistance(candidate);
        if (!best || distance < bestDistance)
        {
            best = candidate;
            bestDistance = distance;
        }
    }

    return best;
}

WorldBotCreatureAI::AmbientEncounterIntent WorldBotCreatureAI::ResolveAmbientEncounterIntent(
    Unit* target,
    std::uint32_t* chanceOut,
    std::uint32_t* rollOut)
{
    if (chanceOut)
        *chanceOut = 0;
    if (rollOut)
        *rollOut = 0;

    if (!me || !target || !target->IsAlive() || !target->IsInWorld())
        return AmbientEncounterIntent::Ignore;
    if (me->IsFriendlyTo(target) || !target->isTargetableForAttack(false, me))
        return AmbientEncounterIntent::Ignore;
    if (!IsAmbientPlayerLikeTarget(target))
        return AmbientEncounterIntent::Attack;
    if (ResolveAmbientTruceZoneName())
        return AmbientEncounterIntent::Ignore;

    std::string personalityKey = !_preparedBuild.personalityKey.empty()
        ? _preparedBuild.personalityKey
        : _identity.personalityKey;
    personalityKey = NormalizeAmbientPersonalityKey(personalityKey);

    std::uint8_t const selfLevel = me->GetLevel();
    std::uint8_t const targetLevel = target->GetLevel();

    if (personalityKey == "aggressive")
        return targetLevel <= (selfLevel + 5u)
            ? AmbientEncounterIntent::Attack
            : AmbientEncounterIntent::Observe;

    if (personalityKey == "coward")
        return (targetLevel >= selfLevel && me->GetDistance(target) <= AmbientCowardTriggerRadius)
            ? AmbientEncounterIntent::Avoid
            : AmbientEncounterIntent::Ignore;

    if (personalityKey == "opportunistic")
    {
        if (_ambientEncounterDecision.targetGuid == target->GetGUID()
            && _ambientEncounterDecision.reevaluateWorldMs > _worldOnlineMs)
        {
            return _ambientEncounterDecision.intent;
        }

        std::uint32_t const chance = ComputeOpportunisticAttackChance(selfLevel, targetLevel);
        std::uint32_t const roll = RollAmbientEncounterPercent();
        AmbientEncounterIntent const intent =
            (roll <= chance) ? AmbientEncounterIntent::Attack : AmbientEncounterIntent::Observe;

        _ambientEncounterDecision.targetGuid = target->GetGUID();
        _ambientEncounterDecision.intent = intent;
        _ambientEncounterDecision.reevaluateWorldMs = _worldOnlineMs + AmbientEncounterDecisionWindowMs;

        if (chanceOut)
            *chanceOut = chance;
        if (rollOut)
            *rollOut = roll;
        return intent;
    }

    return AmbientEncounterIntent::Ignore;
}

bool WorldBotCreatureAI::CanInitiateAmbientCombatAgainstTarget(Unit* target, char const* /*reason*/)
{
    if (!me || !target || !target->IsAlive() || !target->IsInWorld())
        return false;
    if (me->IsFriendlyTo(target) || !target->isTargetableForAttack(false, me))
        return false;
    if (!IsAmbientPlayerLikeTarget(target))
        return true;

    return ResolveAmbientEncounterIntent(target) == AmbientEncounterIntent::Attack;
}

std::string WorldBotCreatureAI::ResolveAmbientPersonalityKeyFor(Unit const* unit) const
{
    if (!unit)
        return "uninterested";

    if (unit == me)
    {
        std::string personalityKey = !_preparedBuild.personalityKey.empty()
            ? _preparedBuild.personalityKey
            : _identity.personalityKey;
        return NormalizeAmbientPersonalityKey(std::move(personalityKey));
    }

    Creature const* creature = unit->ToCreature();
    if (!creature || creature->GetEntry() != WorldBotEntry || !creature->IsAIEnabled)
        return "uninterested";

    auto const* botAi = static_cast<WorldBotCreatureAI const*>(creature->AI());
    if (!botAi)
        return "uninterested";

    std::string personalityKey = !botAi->_preparedBuild.personalityKey.empty()
        ? botAi->_preparedBuild.personalityKey
        : botAi->_identity.personalityKey;
    return NormalizeAmbientPersonalityKey(std::move(personalityKey));
}

bool WorldBotCreatureAI::IsAmbientCowardWorldBotTarget(Unit const* target) const
{
    return target && ResolveAmbientPersonalityKeyFor(target) == "coward";
}

bool WorldBotCreatureAI::ShouldCowardFleeCombatAgainst(Unit* target) const
{
    if (!me || !target || !target->IsAlive() || !target->IsInWorld())
        return false;
    if (!IsAmbientPlayerLikeTarget(target))
        return false;

    std::string personalityKey = ResolveAmbientPersonalityKeyFor(me);
    if (personalityKey != "coward")
        return false;

    return target->GetLevel() > (me->GetLevel() + 5u);
}

void WorldBotCreatureAI::RefreshAmbientPursuitState(Unit* target)
{
    if (!me)
        return;

    if (ResolveAmbientPersonalityKeyFor(me) != "aggressive")
    {
        _ambientPursuitState.Reset();
        return;
    }

    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        _ambientPursuitState.Reset();
        return;
    }

    bool const targetIsCoward = IsAmbientCowardWorldBotTarget(target);
    if (!_ambientPursuitState.active
        || _ambientPursuitState.targetGuid != target->GetGUID()
        || _ambientPursuitState.targetWasCoward != targetIsCoward)
    {
        _ambientPursuitState.active = true;
        _ambientPursuitState.targetGuid = target->GetGUID();
        _ambientPursuitState.startedAtMs = _worldOnlineMs;
        _ambientPursuitState.targetWasCoward = targetIsCoward;
        return;
    }

    if (_ambientPursuitState.startedAtMs == 0)
        _ambientPursuitState.startedAtMs = _worldOnlineMs;
}

bool WorldBotCreatureAI::TryHandleAggressivePursuitTimeout()
{
    if (!me || ResolveAmbientPersonalityKeyFor(me) != "aggressive")
    {
        _ambientPursuitState.Reset();
        return false;
    }

    Unit* target = me->GetVictim();
    if (!target || !target->IsAlive() || !target->IsInWorld())
    {
        _ambientPursuitState.Reset();
        return false;
    }

    RefreshAmbientPursuitState(target);
    if (!_ambientPursuitState.active || !_ambientPursuitState.targetWasCoward)
        return false;

    if (_ambientPursuitState.targetGuid != target->GetGUID())
        return false;

    if ((_worldOnlineMs - _ambientPursuitState.startedAtMs) < AmbientAggressivePursuitBreakMs)
        return false;

    float const distance = me->GetDistance(target);
    if (distance < AmbientCowardTriggerRadius)
        return false;

    if (_combatInterrupt.active)
    {
        RecordCombatSummary("pursuit_break");
        _combatInterrupt.allClearElapsedMs = _combatInterrupt.allClearRequiredMs;
        _combatInterrupt.resumePending = true;
    }

    Creature* targetCreature = target->ToCreature();
    if (targetCreature && targetCreature->GetEntry() == WorldBotEntry && targetCreature->IsAIEnabled)
    {
        auto* targetAi = static_cast<WorldBotCreatureAI*>(targetCreature->AI());
        if (targetAi)
        {
            if (targetAi->_combatInterrupt.active)
            {
                targetAi->RecordCombatSummary("pursuit_break");
                targetAi->_combatInterrupt.allClearElapsedMs =
                    targetAi->_combatInterrupt.allClearRequiredMs;
                targetAi->_combatInterrupt.resumePending = true;
            }

            targetAi->_ambientFleeState.Reset();
            targetAi->_ambientPursuitState.Reset();
            targetAi->_ambientEncounterDecision.Reset();
            targetAi->_combatDisengageGraceMs = 0;
            targetAi->me->AttackStop();
            targetAi->me->CombatStop(false);
        }
    }

    me->AttackStop();
    me->CombatStop(false);

    std::ostringstream trace;
    trace << "phase='encounter' decision='pursuit_break' "
          << "personality='aggressive' "
          << "target=" << DescribeTraceUnit(target) << " "
          << "elapsed_ms=" << (_worldOnlineMs - _ambientPursuitState.startedAtMs) << " "
          << "distance=" << distance;
    RecordEncounterTrace(trace.str());

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "encounter_pursuit_break",
        std::string("target_guid=") + std::to_string(target->GetGUID().GetCounter())
            + " target='" + target->GetName() + "'"
            + " elapsed_ms=" + std::to_string(_worldOnlineMs - _ambientPursuitState.startedAtMs));

    _ambientPursuitState.Reset();
    return true;
}

bool WorldBotCreatureAI::TryHandleCowardCombatFlee()
{
    if (!me || !(me->IsInCombat() || me->GetVictim() || !me->getAttackers().empty()))
        return false;

    Unit* threat = me->GetVictim();
    if (!threat)
    {
        for (Unit* attacker : me->getAttackers())
        {
            if (!attacker || !attacker->IsAlive() || !attacker->IsInWorld())
                continue;
            threat = attacker;
            break;
        }
    }
    if (!threat && _lastIncomingDamageSnapshot.valid())
        threat = ObjectAccessor::GetUnit(*me, _lastIncomingDamageSnapshot.sourceGuid);

    if (!ShouldCowardFleeCombatAgainst(threat))
    {
        _ambientFleeState.Reset();
        return false;
    }

    me->AttackStop();
    return TryExecuteAmbientCowardAvoidance(threat);
}

bool WorldBotCreatureAI::TryExecuteAmbientCowardAvoidance(Unit* target)
{
    if (!me || !target)
    {
        _ambientFleeState.Reset();
        return false;
    }

    float const currentDistance = me->GetDistance(target);
    if (currentDistance >= AmbientCowardAvoidDistance)
    {
        _ambientFleeState.Reset();
        return false;
    }

    float const dx = me->GetPositionX() - target->GetPositionX();
    float const dy = me->GetPositionY() - target->GetPositionY();
    float const distance2d = std::max(0.1f, std::sqrt((dx * dx) + (dy * dy)));

    float dirX = dx / distance2d;
    float dirY = dy / distance2d;
    if (std::abs(dirX) < 0.001f && std::abs(dirY) < 0.001f)
    {
        float const fallbackAngle =
            static_cast<float>((target->GetGUID().GetCounter() % 628u) / 100.0);
        dirX = std::cos(fallbackAngle);
        dirY = std::sin(fallbackAngle);
    }

    float const destX = me->GetPositionX() + (dirX * AmbientCowardAvoidDistance);
    float const destY = me->GetPositionY() + (dirY * AmbientCowardAvoidDistance);
    float destZ = me->GetPositionZ();
    me->UpdateAllowedPositionZ(destX, destY, destZ);

    if (!_ambientFleeState.active || _ambientFleeState.threatGuid != target->GetGUID())
    {
        _ambientFleeState.active = true;
        _ambientFleeState.threatGuid = target->GetGUID();
        _ambientFleeState.startedAtMs = _worldOnlineMs;
    }
    _ambientFleeState.lastRefreshWorldMs = _worldOnlineMs;
    _ambientFleeState.lastThreatDistance = currentDistance;

    if ((_worldOnlineMs - _lastAmbientAvoidWorldMs) >= AmbientAvoidMoveThrottleMs)
    {
        me->StopMoving();
        me->GetMotionMaster()->MovePoint(0u, destX, destY, destZ);
        _lastAmbientAvoidWorldMs = _worldOnlineMs;
    }

    std::ostringstream trace;
    trace << "phase='encounter' decision='avoid' "
          << "personality='coward' "
          << "target=" << DescribeTraceUnit(target) << " "
          << "distance=" << currentDistance << " "
          << "destination=(" << destX << "," << destY << "," << destZ << ")";
    RecordEncounterTrace(trace.str());
    return true;
}

bool WorldBotCreatureAI::TryHandleAmbientPlayerEncounter()
{
    if (!me || me->IsInCombat() || me->GetVictim() || !CanInterruptCurrentStepForCombat())
        return false;

    Unit* target = FindNearbyAmbientPlayerLikeTarget(AmbientPersonalityEncounterRadius);
    if (!target)
    {
        if (_ambientFleeState.active && !_ambientFleeState.threatGuid.IsEmpty())
        {
            Unit* fleeThreat = ObjectAccessor::GetUnit(*me, _ambientFleeState.threatGuid);
            if (fleeThreat && fleeThreat->IsAlive() && fleeThreat->IsInWorld()
                && me->GetDistance(fleeThreat) < AmbientCowardAvoidDistance)
            {
                return TryExecuteAmbientCowardAvoidance(fleeThreat);
            }
        }

        if (!_ambientEncounterDecision.targetGuid.IsEmpty())
            _ambientEncounterDecision.Reset();
        _ambientFleeState.Reset();
        return false;
    }

    if (char const* truceZone = ResolveAmbientTruceZoneName())
    {
        std::ostringstream trace;
        trace << "phase='encounter' decision='truce_passive' "
              << "zone='" << truceZone << "' "
              << "target=" << DescribeTraceUnit(target);
        RecordEncounterTrace(trace.str());
        return false;
    }

    std::uint32_t chance = 0;
    std::uint32_t roll = 0;
    AmbientEncounterIntent const intent = ResolveAmbientEncounterIntent(target, &chance, &roll);

    if (intent == AmbientEncounterIntent::Attack)
    {
        SuspendCurrentStepForCombat(target);
        _combatDisengageGraceMs = 0;
        _combatInterrupt.allClearElapsedMs = 0;
        me->EngageWithTarget(target);
        target->EngageWithTarget(me);
        AttackStart(target);
        me->AddThreat(target, 1.0f);
        target->AddThreat(me, 1.0f);
        EnsureMutualThreatEngagement(me, target);
        PublishAmbientGroupPrimaryTarget(target);
        PublishAmbientGroupTankAnchor(target);

        if (Creature* creature = target->ToCreature(); creature && creature->IsAIEnabled)
            creature->AI()->AttackStart(me);

        std::ostringstream trace;
        trace << "phase='encounter' decision='attack' "
              << "personality='" << (!_preparedBuild.personalityKey.empty() ? _preparedBuild.personalityKey : _identity.personalityKey) << "' "
              << "target=" << DescribeTraceUnit(target);
        if (chance != 0)
            trace << " chance=" << chance << " roll=" << roll;
        RecordEncounterTrace(trace.str());

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "encounter_attack",
            std::string("target_guid=") + std::to_string(target->GetGUID().GetCounter())
                + " target='" + target->GetName() + "'"
                + " personality='" + (!_preparedBuild.personalityKey.empty() ? _preparedBuild.personalityKey : _identity.personalityKey) + "'");
        return true;
    }

    if (intent == AmbientEncounterIntent::Avoid)
        return TryExecuteAmbientCowardAvoidance(target);

    if (intent == AmbientEncounterIntent::Observe)
    {
        std::ostringstream trace;
        trace << "phase='encounter' decision='observe' "
              << "personality='" << (!_preparedBuild.personalityKey.empty() ? _preparedBuild.personalityKey : _identity.personalityKey) << "' "
              << "target=" << DescribeTraceUnit(target);
        if (chance != 0)
            trace << " chance=" << chance << " roll=" << roll;
        RecordEncounterTrace(trace.str());
    }

    return false;
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
    float const zDelta = (me && target) ? std::fabs(me->GetPositionZ() - target->GetPositionZ()) : 0.0f;
    oss << "phase='movement' decision='" << decision << "' "
        << "target='" << (target ? target->GetName() : "none") << "' "
        << "target_guid=" << (target ? target->GetGUID().GetCounter() : 0) << " "
        << "target_hp_pct=" << (target ? target->GetHealthPct() : 0.0f) << " "
        << "self_hp_pct=" << (me ? me->GetHealthPct() : 0.0f) << " "
        << "distance=" << distance << " "
        << "z_delta=" << zDelta << " "
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
    out.completedSessionsThisActivation = _completedSessionsThisActivation;
    out.inCombat = me->IsInCombat();
    out.isEngaged = me->IsEngaged();
    out.hasVictim = me->GetVictim() != nullptr;
    out.hasAttackers = !me->getAttackers().empty();
    out.combatInterruptActive = _combatInterrupt.active;
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

float WorldBotCreatureAI::ResolveTravelProofArrivalThreshold() const
{
    if (_session.sourceKind == "debug_route_harness")
    {
        return std::max(
            0.5f,
            sConfigMgr->GetOption<float>(
                "LivingWorld.DebugRouteHarnessArrivalThresholdYards",
                3.0f));
    }

    return 3.0f;
}

float WorldBotCreatureAI::ResolveTravelConnectorArrivalThreshold() const
{
    return std::max(
        0.5f,
        sConfigMgr->GetOption<float>(
            "LivingWorld.TravelArrivalThresholdConnectorYards",
            ArrivalThreshold));
}

float WorldBotCreatureAI::ResolveTravelDestinationArrivalThreshold(service::AmbientStep const& step) const
{
    if (_session.sourceKind == "debug_route_harness")
        return ResolveTravelProofArrivalThreshold();

    if (IsTravelConnectorPointType(step.targetPointType))
        return ResolveTravelConnectorArrivalThreshold();

    return std::max(
        0.5f,
        sConfigMgr->GetOption<float>(
            "LivingWorld.TravelArrivalThresholdServiceYards",
            5.0f));
}

float WorldBotCreatureAI::ResolveActiveTravelArrivalThreshold(service::AmbientStep const& step) const
{
    if (_routeTravelPlanActive && _routeTravelWaypointIndex < _routeTravelPlan.waypoints.size())
    {
        if ((_routeTravelWaypointIndex + 1u) < _routeTravelPlan.waypoints.size())
            return ResolveTravelConnectorArrivalThreshold();

        return ResolveTravelDestinationArrivalThreshold(step);
    }

    return ResolveTravelDestinationArrivalThreshold(step);
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

WorldBotCreatureAI::TravelNavigationPolicy WorldBotCreatureAI::ResolveTravelNavigationPolicy(
    service::AmbientStep const& step) const
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return TravelNavigationPolicy::MacroTravel;

    if (_session.sourceKind == "debug_path_scout")
        return TravelNavigationPolicy::LocalOnly;

    if (!step.transitType.empty() || !step.transitRouteKey.empty())
        return TravelNavigationPolicy::MacroTravel;

    if (step.mapId != me->GetMapId())
        return TravelNavigationPolicy::MacroTravel;

    std::uint32_t const stepZoneId = ResolveStepZoneId(_session, step);
    std::uint32_t const currentZoneId = me->GetZoneId();
    if (stepZoneId == 0 || currentZoneId == 0 || stepZoneId != currentZoneId)
        return TravelNavigationPolicy::MacroTravel;

    return TravelNavigationPolicy::LocalOnly;
}

bool WorldBotCreatureAI::TryBuildLocalAssistTravelPlan(
    service::AmbientStep const& step,
    service::WorldBotResolvedTravelPlan& outPlan) const
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;

    float const connectorArrivalThreshold = ResolveTravelConnectorArrivalThreshold();
    float const destinationArrivalThreshold = ResolveTravelDestinationArrivalThreshold(step);
    if (step.mapId != me->GetMapId())
        return false;

    std::uint32_t const zoneId = ResolveStepZoneId(_session, step);
    if (zoneId == 0 || zoneId != me->GetZoneId())
        return false;

    std::string const destinationKey = ResolveStepTargetPointKey(_session, step);
    if (destinationKey.empty())
        return false;

    auto const candidatePaths = GetWorldBotRoutePlanner().LoadZonePaths(
        step.mapId,
        zoneId,
        service::WorldBotRoutePathKind::SubRoute);

    if (candidatePaths.empty())
        return false;

    struct AssistCandidate
    {
        service::WorldBotResolvedTravelPlan plan;
        float score = std::numeric_limits<float>::max();
    };

    std::optional<AssistCandidate> bestCandidate;
    service::WorldBotTravelCapabilityConfig const capabilityConfig =
        service::LoadWorldBotTravelCapabilityConfig();
    float const speedYardsPerSecond = std::max(
        0.1f,
        service::ResolveWorldBotTravelSpeedYardsPerSecond(ResolveTravelCapabilityTier(), capabilityConfig));
    constexpr float AssistAttachDistanceThresholdYards = 120.0f;
    constexpr float AssistDirectionBiasYards = 5.0f;

    auto const buildCandidate =
        [&](service::WorldBotRoutePlanner::RoutePath const& path,
            bool reverseTraversal,
            float score) -> std::optional<AssistCandidate>
        {
            std::vector<service::WorldBotRoutePlanner::RoutePoint> orderedPoints = path.points;
            if (reverseTraversal)
                std::reverse(orderedPoints.begin(), orderedPoints.end());

            if (orderedPoints.size() < 2)
                return std::nullopt;

            service::WorldBotRoutePlanner::RoutePoint const& firstPoint = orderedPoints.front();
            service::WorldBotRoutePlanner::RoutePoint const& lastPoint = orderedPoints.back();

            StrictTravelPathCheckResult const attachCheck =
                EvaluateStrictGroundTravelPath(me, firstPoint.x, firstPoint.y, firstPoint.z);
            if (!IsStrictGroundTravelPathAccepted(attachCheck)
                && me->GetDistance(firstPoint.x, firstPoint.y, firstPoint.z) > connectorArrivalThreshold)
                return std::nullopt;

            service::WorldBotResolvedTravelPlan plan;
            plan.mapId = step.mapId;
            plan.zoneId = zoneId;
            plan.speedYardsPerSecond = speedYardsPerSecond;

            float previousX = me->GetPositionX();
            float previousY = me->GetPositionY();
            float previousZ = me->GetPositionZ();
            float cumulativeDistanceYards = 0.0f;
            bool firstWaypointAdded = false;

            for (std::size_t pointIndex = 0; pointIndex < orderedPoints.size(); ++pointIndex)
            {
                service::WorldBotRoutePlanner::RoutePoint const& point = orderedPoints[pointIndex];
                if (!firstWaypointAdded
                    && me->GetDistance(point.x, point.y, point.z) <= connectorArrivalThreshold)
                {
                    previousX = point.x;
                    previousY = point.y;
                    previousZ = point.z;
                    continue;
                }

                cumulativeDistanceYards += std::sqrt(
                    (point.x - previousX) * (point.x - previousX)
                    + (point.y - previousY) * (point.y - previousY)
                    + (point.z - previousZ) * (point.z - previousZ));
                previousX = point.x;
                previousY = point.y;
                previousZ = point.z;

                service::WorldBotRouteWaypoint waypoint;
                waypoint.mapId = point.mapId;
                waypoint.x = point.x;
                waypoint.y = point.y;
                waypoint.z = point.z;
                waypoint.cumulativeDistanceYards = cumulativeDistanceYards;
                waypoint.routeKey = path.routeKey;
                waypoint.pathIndex = -1;
                waypoint.pointIndex = static_cast<std::int32_t>(pointIndex);
                plan.waypoints.push_back(std::move(waypoint));
                firstWaypointAdded = true;
            }

            float const routeDistanceYards = cumulativeDistanceYards;
            float const finalLegDistance = std::sqrt(
                (step.x - previousX) * (step.x - previousX)
                + (step.y - previousY) * (step.y - previousY)
                + (step.z - previousZ) * (step.z - previousZ));

            if (finalLegDistance > destinationArrivalThreshold)
            {
                cumulativeDistanceYards += finalLegDistance;
                service::WorldBotRouteWaypoint finalWaypoint;
                finalWaypoint.mapId = step.mapId;
                finalWaypoint.x = step.x;
                finalWaypoint.y = step.y;
                finalWaypoint.z = step.z;
                finalWaypoint.cumulativeDistanceYards = cumulativeDistanceYards;
                finalWaypoint.routeKey = path.routeKey;
                finalWaypoint.pathIndex = -1;
                finalWaypoint.pointIndex = static_cast<std::int32_t>(orderedPoints.size());
                plan.waypoints.push_back(std::move(finalWaypoint));
            }

            if (plan.waypoints.empty())
                return std::nullopt;

            plan.attachDistanceYards = me->GetDistance(firstPoint.x, firstPoint.y, firstPoint.z);
            plan.routeDistanceYards = routeDistanceYards;
            plan.detachDistanceYards = finalLegDistance;
            plan.totalDistanceYards = cumulativeDistanceYards;
            plan.etaMs = static_cast<std::uint32_t>(
                std::clamp((cumulativeDistanceYards / speedYardsPerSecond) * 1000.0f, 1000.0f, 3600000.0f));

            AssistCandidate candidate;
            candidate.plan = std::move(plan);
            candidate.score = score;
            return candidate;
        };

    for (service::WorldBotRoutePlanner::RoutePath const& path : candidatePaths)
    {
        if (path.points.size() < 2 || path.assistKind.empty())
            continue;

        service::WorldBotRoutePlanner::RoutePoint const& front = path.points.front();
        service::WorldBotRoutePlanner::RoutePoint const& back = path.points.back();

        service::WorldBotRoutePlanner::RoutePoint const& lower = front.z <= back.z ? front : back;
        service::WorldBotRoutePlanner::RoutePoint const& upper = front.z <= back.z ? back : front;
        bool const forwardIsLowerToUpper = front.z <= back.z;
        bool const reverseIsLowerToUpper = !forwardIsLowerToUpper;

        float const botToLowerDistance = me->GetDistance(lower.x, lower.y, lower.z);
        float const botToUpperDistance = me->GetDistance(upper.x, upper.y, upper.z);
        float const destToLowerDistance = std::sqrt(
            (step.x - lower.x) * (step.x - lower.x)
            + (step.y - lower.y) * (step.y - lower.y)
            + (step.z - lower.z) * (step.z - lower.z));
        float const destToUpperDistance = std::sqrt(
            (step.x - upper.x) * (step.x - upper.x)
            + (step.y - upper.y) * (step.y - upper.y)
            + (step.z - upper.z) * (step.z - upper.z));

        bool const explicitUpperMatch = path.SupportsUpperContextKey(destinationKey);
        bool const explicitLowerMatch = path.SupportsLowerContextKey(destinationKey);
        bool const legacyMatch =
            !explicitUpperMatch
            && !explicitLowerMatch
            && path.SupportsDestinationKey(destinationKey);

        bool tryLowerToUpper = false;
        bool tryUpperToLower = false;

        if (explicitUpperMatch)
            tryLowerToUpper = true;
        if (explicitLowerMatch)
            tryUpperToLower = true;

        if (!tryLowerToUpper && !tryUpperToLower && legacyMatch)
        {
            tryLowerToUpper = true;
            tryUpperToLower = true;
        }

        if (!tryLowerToUpper && !tryUpperToLower)
        {
            bool const nearLower = botToLowerDistance <= AssistAttachDistanceThresholdYards;
            bool const nearUpper = botToUpperDistance <= AssistAttachDistanceThresholdYards;
            bool const upperSeemsUseful =
                nearLower
                && destToUpperDistance + AssistDirectionBiasYards < destToLowerDistance;
            bool const lowerSeemsUseful =
                nearUpper
                && destToLowerDistance + AssistDirectionBiasYards < destToUpperDistance;

            if (upperSeemsUseful)
                tryLowerToUpper = true;
            if (lowerSeemsUseful)
                tryUpperToLower = true;
        }

        if (!tryLowerToUpper && !tryUpperToLower)
            continue;

        float const lowerToUpperScore = botToLowerDistance + destToUpperDistance;
        float const upperToLowerScore = botToUpperDistance + destToLowerDistance;

        if (tryLowerToUpper)
        {
            if (auto candidate = buildCandidate(path, !forwardIsLowerToUpper, lowerToUpperScore))
            {
                if (!bestCandidate || candidate->score < bestCandidate->score)
                    bestCandidate = std::move(candidate);
            }
        }

        if (tryUpperToLower)
        {
            if (auto candidate = buildCandidate(path, reverseIsLowerToUpper, upperToLowerScore))
            {
                if (!bestCandidate || candidate->score < bestCandidate->score)
                    bestCandidate = std::move(candidate);
            }
        }
    }

    if (!bestCandidate.has_value())
        return false;

    outPlan = std::move(bestCandidate->plan);
    return true;
}

bool WorldBotCreatureAI::TryActivateLocalPoiConnectorFallback(service::AmbientStep const& step)
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;

    float const connectorArrivalThreshold = ResolveTravelConnectorArrivalThreshold();
    float const destinationArrivalThreshold = ResolveTravelDestinationArrivalThreshold(step);
    if (_localPoiConnectorAttemptCount >= 2)
        return false;
    if (step.mapId != me->GetMapId())
        return false;

    std::uint32_t const zoneId = ResolveStepZoneId(_session, step);
    if (zoneId == 0 || zoneId != me->GetZoneId())
        return false;

    integration::SqlTaskPointLinkRepository linkRepo;
    std::vector<model::TaskPointLinkEntry> const links =
        linkRepo.LoadLocalNavigationLinks(step.mapId, zoneId);
    if (links.empty())
        return false;

    struct LinkEdge
    {
        std::string toPointKey;
        float cost = 0.0f;
    };

    auto const pointDistance =
        [](float ax, float ay, float az, float bx, float by, float bz) -> float
        {
            float const dx = bx - ax;
            float const dy = by - ay;
            float const dz = bz - az;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        };

    std::unordered_map<std::string, model::TaskPointEntry> pointsByKey;
    std::unordered_map<std::string, std::vector<LinkEdge>> adjacency;
    pointsByKey.reserve(links.size() * 2u);
    adjacency.reserve(links.size());

    for (model::TaskPointLinkEntry const& link : links)
    {
        pointsByKey.emplace(link.fromPointKey, link.fromPoint);
        pointsByKey.emplace(link.toPointKey, link.toPoint);

        float const geometricDistance = pointDistance(
            link.fromPoint.x, link.fromPoint.y, link.fromPoint.z,
            link.toPoint.x, link.toPoint.y, link.toPoint.z);
        float const confidence = static_cast<float>(std::max<std::uint32_t>(
            1u,
            link.successCount + (link.manualVerified ? 2u : 0u)));
        float const failurePenalty =
            (static_cast<float>(link.failureCount) * 20.0f) / confidence;

        adjacency[link.fromPointKey].push_back(LinkEdge{
            link.toPointKey,
            geometricDistance + 8.0f + failurePenalty });
    }

    if (adjacency.empty() || pointsByKey.empty())
        return false;

    struct AnchorCandidate
    {
        std::string pointKey;
        model::TaskPointEntry point;
        float distanceYards = std::numeric_limits<float>::max();
    };

    std::vector<AnchorCandidate> startCandidates;
    startCandidates.reserve(adjacency.size());
    for (auto const& [pointKey, edges] : adjacency)
    {
        if (edges.empty())
            continue;

        auto const pointItr = pointsByKey.find(pointKey);
        if (pointItr == pointsByKey.end())
            continue;

        AnchorCandidate candidate;
        candidate.pointKey = pointKey;
        candidate.point = pointItr->second;
        candidate.distanceYards = pointDistance(
            me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(),
            candidate.point.x, candidate.point.y, candidate.point.z);
        startCandidates.push_back(std::move(candidate));
    }

    if (startCandidates.empty())
        return false;

    std::sort(
        startCandidates.begin(),
        startCandidates.end(),
        [](AnchorCandidate const& lhs, AnchorCandidate const& rhs)
        {
            if (lhs.distanceYards != rhs.distanceYards)
                return lhs.distanceYards < rhs.distanceYards;
            return lhs.pointKey < rhs.pointKey;
        });

    std::string const destinationKey = ResolveStepTargetPointKey(_session, step);
    std::vector<AnchorCandidate> destinationCandidates;
    destinationCandidates.reserve(pointsByKey.size());
    for (auto const& [pointKey, point] : pointsByKey)
    {
        AnchorCandidate candidate;
        candidate.pointKey = pointKey;
        candidate.point = point;
        candidate.distanceYards = pointDistance(
            step.x, step.y, step.z,
            point.x, point.y, point.z);
        destinationCandidates.push_back(std::move(candidate));
    }

    std::sort(
        destinationCandidates.begin(),
        destinationCandidates.end(),
        [&](AnchorCandidate const& lhs, AnchorCandidate const& rhs)
        {
            bool const lhsExact = !destinationKey.empty() && lhs.pointKey == destinationKey;
            bool const rhsExact = !destinationKey.empty() && rhs.pointKey == destinationKey;
            if (lhsExact != rhsExact)
                return lhsExact;
            if (lhs.distanceYards != rhs.distanceYards)
                return lhs.distanceYards < rhs.distanceYards;
            return lhs.pointKey < rhs.pointKey;
        });

    if (destinationCandidates.empty())
        return false;

    struct PathSearchState
    {
        float cost = 0.0f;
        std::uint8_t hops = 0;
        std::string pointKey;

        bool operator>(PathSearchState const& other) const
        {
            if (cost != other.cost)
                return cost > other.cost;
            return pointKey > other.pointKey;
        }
    };

    struct CandidatePlan
    {
        service::WorldBotResolvedTravelPlan plan;
        std::string startAnchorKey;
        std::string destAnchorKey;
        float totalScore = std::numeric_limits<float>::max();
    };

    auto const tryBuildPathForStart =
        [&](AnchorCandidate const& startCandidate) -> std::optional<CandidatePlan>
        {
            std::unordered_map<std::string, float> bestCost;
            std::unordered_map<std::string, std::uint8_t> bestHops;
            std::unordered_map<std::string, std::string> previousKey;
            std::priority_queue<
                PathSearchState,
                std::vector<PathSearchState>,
                std::greater<PathSearchState>> open;

            bestCost[startCandidate.pointKey] = 0.0f;
            bestHops[startCandidate.pointKey] = 0;
            open.push(PathSearchState{0.0f, 0u, startCandidate.pointKey});

            while (!open.empty())
            {
                PathSearchState const current = open.top();
                open.pop();

                auto const bestItr = bestCost.find(current.pointKey);
                if (bestItr == bestCost.end() || current.cost > bestItr->second)
                    continue;

                if (current.hops >= 4)
                    continue;

                auto const edgeItr = adjacency.find(current.pointKey);
                if (edgeItr == adjacency.end())
                    continue;

                for (LinkEdge const& edge : edgeItr->second)
                {
                    float const nextCost = current.cost + edge.cost;
                    std::uint8_t const nextHops = static_cast<std::uint8_t>(current.hops + 1u);

                    auto const nextBestItr = bestCost.find(edge.toPointKey);
                    if (nextBestItr != bestCost.end())
                    {
                        std::uint8_t const recordedHops = bestHops[edge.toPointKey];
                        if (nextBestItr->second < nextCost
                            || (std::fabs(nextBestItr->second - nextCost) < 0.001f && recordedHops <= nextHops))
                        {
                            continue;
                        }
                    }

                    bestCost[edge.toPointKey] = nextCost;
                    bestHops[edge.toPointKey] = nextHops;
                    previousKey[edge.toPointKey] = current.pointKey;
                    open.push(PathSearchState{nextCost, nextHops, edge.toPointKey});
                }
            }

            std::optional<CandidatePlan> bestPlan;
            std::size_t const destinationLimit = std::min<std::size_t>(3u, destinationCandidates.size());
            for (std::size_t i = 0; i < destinationLimit; ++i)
            {
                AnchorCandidate const& destCandidate = destinationCandidates[i];
                auto const destCostItr = bestCost.find(destCandidate.pointKey);
                if (destCostItr == bestCost.end())
                    continue;

                std::vector<std::string> pathKeys;
                for (std::string cursor = destCandidate.pointKey; !cursor.empty(); )
                {
                    pathKeys.push_back(cursor);
                    auto const prevItr = previousKey.find(cursor);
                    if (prevItr == previousKey.end())
                        break;
                    cursor = prevItr->second;
                }

                if (pathKeys.empty() || pathKeys.back() != startCandidate.pointKey)
                    continue;

                std::reverse(pathKeys.begin(), pathKeys.end());

                service::WorldBotTravelCapabilityConfig const capabilityConfig =
                    service::LoadWorldBotTravelCapabilityConfig();
                float const speedYardsPerSecond = std::max(
                    0.1f,
                    service::ResolveWorldBotTravelSpeedYardsPerSecond(
                        ResolveTravelCapabilityTier(),
                        capabilityConfig));

                service::WorldBotResolvedTravelPlan plan;
                plan.mapId = step.mapId;
                plan.zoneId = zoneId;
                plan.speedYardsPerSecond = speedYardsPerSecond;

                float previousX = me->GetPositionX();
                float previousY = me->GetPositionY();
                float previousZ = me->GetPositionZ();
                float cumulativeDistanceYards = 0.0f;
                bool firstWaypointAdded = false;
                float attachDistanceYards = 0.0f;

                for (std::size_t pathIndex = 0; pathIndex < pathKeys.size(); ++pathIndex)
                {
                    auto const pointItr = pointsByKey.find(pathKeys[pathIndex]);
                    if (pointItr == pointsByKey.end())
                        continue;

                    model::TaskPointEntry const& point = pointItr->second;
                    float const legDistanceYards = pointDistance(
                        previousX, previousY, previousZ,
                        point.x, point.y, point.z);

                    if (!firstWaypointAdded)
                        attachDistanceYards = legDistanceYards;

                    if (!firstWaypointAdded && legDistanceYards <= connectorArrivalThreshold)
                    {
                        previousX = point.x;
                        previousY = point.y;
                        previousZ = point.z;
                        continue;
                    }

                    cumulativeDistanceYards += legDistanceYards;
                    previousX = point.x;
                    previousY = point.y;
                    previousZ = point.z;

                    service::WorldBotRouteWaypoint waypoint;
                    waypoint.mapId = point.mapId;
                    waypoint.x = point.x;
                    waypoint.y = point.y;
                    waypoint.z = point.z;
                    waypoint.cumulativeDistanceYards = cumulativeDistanceYards;
                    waypoint.routeKey = point.pointKey;
                    waypoint.pathIndex = -1;
                    waypoint.pointIndex = static_cast<std::int32_t>(pathIndex);
                    plan.waypoints.push_back(std::move(waypoint));
                    firstWaypointAdded = true;
                }

                float const routeDistanceYards = cumulativeDistanceYards;
                float const finalLegDistanceYards = pointDistance(
                    previousX, previousY, previousZ,
                    step.x, step.y, step.z);

                if (finalLegDistanceYards > destinationArrivalThreshold)
                {
                    cumulativeDistanceYards += finalLegDistanceYards;
                    service::WorldBotRouteWaypoint finalWaypoint;
                    finalWaypoint.mapId = step.mapId;
                    finalWaypoint.x = step.x;
                    finalWaypoint.y = step.y;
                    finalWaypoint.z = step.z;
                    finalWaypoint.cumulativeDistanceYards = cumulativeDistanceYards;
                    finalWaypoint.routeKey = destinationKey.empty() ? std::string("direct_target") : destinationKey;
                    finalWaypoint.pathIndex = -1;
                    finalWaypoint.pointIndex = static_cast<std::int32_t>(pathKeys.size());
                    plan.waypoints.push_back(std::move(finalWaypoint));
                }

                if (plan.waypoints.empty())
                    continue;

                plan.attachDistanceYards = attachDistanceYards;
                plan.routeDistanceYards = routeDistanceYards;
                plan.detachDistanceYards = finalLegDistanceYards;
                plan.totalDistanceYards = cumulativeDistanceYards;
                plan.etaMs = static_cast<std::uint32_t>(
                    std::clamp((cumulativeDistanceYards / speedYardsPerSecond) * 1000.0f, 1000.0f, 3600000.0f));

                float const totalScore =
                    destCostItr->second
                    + startCandidate.distanceYards
                    + destCandidate.distanceYards;

                if (!bestPlan || totalScore < bestPlan->totalScore)
                {
                    bestPlan = CandidatePlan{
                        std::move(plan),
                        startCandidate.pointKey,
                        destCandidate.pointKey,
                        totalScore};
                }
            }

            return bestPlan;
        };

    constexpr float MaxConnectorAttachDistanceYards = 180.0f;

    while (_localPoiConnectorAttemptCount < 2)
    {
        std::optional<AnchorCandidate> selectedStart;
        for (AnchorCandidate const& candidate : startCandidates)
        {
            if (_localPoiConnectorTriedStartKeys.find(candidate.pointKey) != _localPoiConnectorTriedStartKeys.end())
                continue;
            if (candidate.distanceYards > MaxConnectorAttachDistanceYards)
                continue;
            selectedStart = candidate;
            break;
        }

        if (!selectedStart.has_value())
            return false;

        _localPoiConnectorTriedStartKeys.insert(selectedStart->pointKey);
        ++_localPoiConnectorAttemptCount;

        std::optional<CandidatePlan> candidatePlan = tryBuildPathForStart(*selectedStart);
        if (!candidatePlan.has_value())
            continue;

        _activeTravelNavigationPolicy = TravelNavigationPolicy::LocalWithPoiConnector;
        _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
        _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
        ActivateRouteTravelPlan(candidatePlan->plan);

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "travel_connector",
            BuildTravelNarrative(
                _session,
                step,
                "falling back to local connector chain from '"
                    + candidatePlan->startAnchorKey
                    + "' toward '"
                    + candidatePlan->destAnchorKey
                    + "'"));

        return true;
    }

    return false;
}

void WorldBotCreatureAI::ResetLocalTravelFallbackState()
{
    _localPoiConnectorAttemptCount = 0;
    _localHelperFallbackTried = false;
    _localMacroFallbackTried = false;
    _localPoiConnectorTriedStartKeys.clear();
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

bool WorldBotCreatureAI::TryBuildDebugScoutTravelPlan(
    service::AmbientStep const& step,
    service::WorldBotTravelCapabilityTier tier)
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;

    float const arrivalThreshold = ResolveTravelProofArrivalThreshold();

    float startX = me->GetPositionX();
    float startY = me->GetPositionY();
    float startZ = me->GetPositionZ();
    me->UpdateGroundPositionZ(startX, startY, startZ);

    float destX = step.x;
    float destY = step.y;
    float destZ = step.z;
    me->UpdateGroundPositionZ(destX, destY, destZ);

    PathGenerator scoutPath(me);
    scoutPath.SetSlopeCheck(true);
    scoutPath.SetUseStraightPath(false);
    scoutPath.SetUseRaycast(false);

    bool const calculateResult = scoutPath.CalculatePath(startX, startY, startZ, destX, destY, destZ, true);
    PathType const pathType = scoutPath.GetPathType();
    Movement::PointsArray const& rawPoints = scoutPath.GetPath();
    float const pathLengthYards = scoutPath.getPathLength();
    bool const rejected = !calculateResult
        || rawPoints.size() <= 2u
        || (pathType & PATHFIND_NOPATH)
        || (pathType & PATHFIND_NOT_USING_PATH)
        || (pathType & PATHFIND_SHORTCUT);

    if (rejected)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "travel_scout_rejected",
            "result=" + std::to_string(calculateResult ? 1 : 0)
                + " flags='" + DescribePathTypeFlags(pathType)
                + "' points=" + std::to_string(rawPoints.size())
                + " length_yd=" + std::to_string(pathLengthYards)
                + " start=(" + std::to_string(startX)
                + "," + std::to_string(startY)
                + "," + std::to_string(startZ) + ")"
                + " dest=(" + std::to_string(destX)
                + "," + std::to_string(destY)
                + "," + std::to_string(destZ) + ")");
        return false;
    }

    service::WorldBotTravelCapabilityConfig const capabilityConfig =
        service::LoadWorldBotTravelCapabilityConfig();
    float const speedYardsPerSecond = std::max(
        0.1f,
        service::ResolveWorldBotTravelSpeedYardsPerSecond(tier, capabilityConfig));

    service::WorldBotResolvedTravelPlan plan;
    plan.mapId = static_cast<std::uint16_t>(me->GetMapId());
    plan.zoneId = me->GetZoneId();
    plan.speedYardsPerSecond = speedYardsPerSecond;

    G3D::Vector3 previous(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
    float cumulativeDistanceYards = 0.0f;
    for (G3D::Vector3 const& point : rawPoints)
    {
        if (plan.waypoints.empty()
            && me->GetDistance(point.x, point.y, point.z) <= arrivalThreshold)
        {
            previous = point;
            continue;
        }

        cumulativeDistanceYards += std::sqrt((point.x - previous.x) * (point.x - previous.x)
            + (point.y - previous.y) * (point.y - previous.y)
            + (point.z - previous.z) * (point.z - previous.z));
        previous = point;

        service::WorldBotRouteWaypoint waypoint;
        waypoint.mapId = static_cast<std::uint16_t>(me->GetMapId());
        waypoint.x = point.x;
        waypoint.y = point.y;
        waypoint.z = point.z;
        waypoint.cumulativeDistanceYards = cumulativeDistanceYards;
        waypoint.routeKey = "debug_path_scout";
        waypoint.pathIndex = -1;
        waypoint.pointIndex = static_cast<std::int32_t>(plan.waypoints.size());
        plan.waypoints.push_back(std::move(waypoint));
    }

    if (plan.waypoints.empty())
        return false;

    plan.routeDistanceYards = cumulativeDistanceYards;
    plan.totalDistanceYards = cumulativeDistanceYards;
    plan.etaMs = static_cast<std::uint32_t>(
        std::clamp((cumulativeDistanceYards / speedYardsPerSecond) * 1000.0f, 1000.0f, 3600000.0f));

    ActivateRouteTravelPlan(plan);
    _debugScoutPathActive = true;
    return true;
}

void WorldBotCreatureAI::ClearActiveRouteTravelPlan()
{
    _routeTravelPlanActive = false;
    _debugScoutPathActive = false;
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
    if (!MoveToActiveTravelTarget(step))
        return false;
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

bool WorldBotCreatureAI::IsAmbientGroupLeader() const
{
    if (_identity.ambientGroupId == 0)
        return false;

    return _identity.ambientGroupLeaderIdentityId == 0
        || _identity.ambientGroupLeaderIdentityId == _identity.id;
}

bool WorldBotCreatureAI::IsAmbientTankRole() const
{
    return _identity.ambientGroupRole == "tank"
        || _preparedBuild.resolvedRoleKey == "TANK";
}

bool WorldBotCreatureAI::IsAmbientHealerRole() const
{
    return _identity.ambientGroupRole == "healer"
        || _identity.ambientGroupRole == "support"
        || _preparedBuild.resolvedRoleKey == "HEAL";
}

bool WorldBotCreatureAI::IsAmbientDpsRole() const
{
    return !IsAmbientTankRole() && !IsAmbientHealerRole();
}

Creature* WorldBotCreatureAI::FindAmbientGroupLeaderCreature(float radius) const
{
    if (!me || _identity.ambientGroupId == 0 || IsAmbientGroupLeader())
        return nullptr;

    for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(me, radius, false))
    {
        if (!IsAmbientGroupedWith(ally))
            continue;

        Creature* creature = ally->ToCreature();
        if (!creature || creature->GetEntry() != WorldBotEntry || !creature->AI())
            continue;

        auto const* allyAi = static_cast<WorldBotCreatureAI const*>(creature->AI());
        if (allyAi->_identity.id == _identity.ambientGroupLeaderIdentityId)
            return creature;
    }

    return nullptr;
}

float WorldBotCreatureAI::ResolveAmbientGroupFollowBaseDistance(bool pullStage) const
{
    if (pullStage)
    {
        if (_identity.ambientGroupRole == "tank")
            return 0.0f;

        if (_identity.ambientGroupRole == "melee_dps")
            return AmbientPullStageMeleeFollowDistance;

        if (_identity.ambientGroupRole == "healer" || _identity.ambientGroupRole == "support")
            return AmbientPullStageHealerFollowDistance;

        if (_identity.ambientGroupRole == "ranged_dps")
            return AmbientPullStageRangedFollowDistance;

        return AmbientPullStageFallbackFollowDistance;
    }

    model::BotGlobalConfig const cfg = GetWorldBotGlobalConfigService().Get();

    if (_identity.ambientGroupRole == "tank" || _identity.ambientGroupRole == "melee_dps")
        return cfg.followDistanceMelee;

    if (_identity.ambientGroupRole == "healer" || _identity.ambientGroupRole == "support")
        return cfg.followDistanceHealer;

    if (_identity.ambientGroupRole == "ranged_dps")
        return std::min(2.0f, cfg.followDistanceRanged);

    return cfg.followDistanceFallback;
}

bool WorldBotCreatureAI::IsHealerDistressWithinWrangleWindow(
    service::AmbientGroupCombatSnapshot const& snapshot,
    std::uint64_t nowMs) const
{
    if (!me
        || snapshot.distressedAllyGuid.IsEmpty()
        || snapshot.distressTargetGuid.IsEmpty()
        || snapshot.distressTier < service::AmbientGroupDistressTier::AssistRequested
        || snapshot.distressStartedAtMs == 0
        || nowMs < snapshot.distressStartedAtMs
        || (nowMs - snapshot.distressStartedAtMs) >= TankWrangleWindowMs)
    {
        return false;
    }

    Unit* distressedAlly = ObjectAccessor::GetUnit(*me, snapshot.distressedAllyGuid);
    Creature* distressedCreature = distressedAlly ? distressedAlly->ToCreature() : nullptr;
    if (!distressedCreature || distressedCreature->GetEntry() != WorldBotEntry || !distressedCreature->AI())
        return false;

    auto const* allyAi = static_cast<WorldBotCreatureAI const*>(distressedCreature->AI());
    return allyAi->IsAmbientHealerRole();
}

bool WorldBotCreatureAI::IsAmbientSharedDistressedAllyHealer(
    service::AmbientGroupCombatSnapshot const& snapshot) const
{
    if (!me || snapshot.distressedAllyGuid.IsEmpty())
        return false;

    Unit* distressedAlly = ObjectAccessor::GetUnit(*me, snapshot.distressedAllyGuid);
    Creature* distressedCreature = distressedAlly ? distressedAlly->ToCreature() : nullptr;
    if (!distressedCreature || distressedCreature->GetEntry() != WorldBotEntry || !distressedCreature->AI())
        return false;

    auto const* allyAi = static_cast<WorldBotCreatureAI const*>(distressedCreature->AI());
    return allyAi->IsAmbientHealerRole();
}

bool WorldBotCreatureAI::IsAmbientSharedDistressedAllyDps(
    service::AmbientGroupCombatSnapshot const& snapshot) const
{
    if (!me || snapshot.distressedAllyGuid.IsEmpty())
        return false;

    Unit* distressedAlly = ObjectAccessor::GetUnit(*me, snapshot.distressedAllyGuid);
    Creature* distressedCreature = distressedAlly ? distressedAlly->ToCreature() : nullptr;
    if (!distressedCreature || distressedCreature->GetEntry() != WorldBotEntry || !distressedCreature->AI())
        return false;

    auto const* allyAi = static_cast<WorldBotCreatureAI const*>(distressedCreature->AI());
    return allyAi->IsAmbientDpsRole();
}

bool WorldBotCreatureAI::TryClaimAmbientGroupPeelTarget(
    service::AmbientGroupCombatSnapshot const& snapshot,
    std::uint64_t nowMs) const
{
    if (!me || _identity.ambientGroupId == 0 || !IsAmbientDpsRole())
        return false;

    if (!IsAmbientSharedDistressedAllyHealer(snapshot))
        return false;

    if (snapshot.distressTargetGuid.IsEmpty()
        || snapshot.distressTier < service::AmbientGroupDistressTier::AssistRequested
        || IsHealerDistressWithinWrangleWindow(snapshot, nowMs))
    {
        return false;
    }

    if (!snapshot.peelClaimedByGuid.IsEmpty() && snapshot.peelClaimedByGuid != me->GetGUID())
        return false;

    Unit* peelTarget = ObjectAccessor::GetUnit(*me, snapshot.distressTargetGuid);
    if (!peelTarget || !peelTarget->IsAlive() || me->IsFriendlyTo(peelTarget))
        return false;

    bool const changed = GetAmbientGroupCombatStateService().ClaimPeel(
        _identity.ambientGroupId,
        me->GetGUID(),
        peelTarget->GetGUID(),
        nowMs);

    if (changed)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            std::string("kind='peel_claim' claimer='") + me->GetName()
                + "' claimer_guid=" + std::to_string(me->GetGUID().GetCounter())
                + " target='" + peelTarget->GetName()
                + "' target_guid=" + std::to_string(peelTarget->GetGUID().GetCounter())
                + " distress_tier='" + ToString(snapshot.distressTier) + "'");
    }

    return changed || snapshot.peelClaimedByGuid == me->GetGUID();
}

bool WorldBotCreatureAI::TryClaimAmbientGroupPeelAssistTarget(
    service::AmbientGroupCombatSnapshot const& snapshot,
    std::uint64_t nowMs) const
{
    if (!me || _identity.ambientGroupId == 0 || !IsAmbientDpsRole())
        return false;

    if (snapshot.peelClaimedByGuid.IsEmpty()
        || snapshot.peelClaimedByGuid == me->GetGUID()
        || snapshot.peelTargetGuid.IsEmpty())
    {
        return false;
    }

    if (!IsAmbientSharedDistressedAllyDps(snapshot)
        || snapshot.distressedAllyGuid != snapshot.peelClaimedByGuid
        || snapshot.distressTargetGuid != snapshot.peelTargetGuid
        || snapshot.distressTier < service::AmbientGroupDistressTier::AssistRequested)
    {
        return false;
    }

    if (!snapshot.peelAssistClaimedByGuid.IsEmpty()
        && snapshot.peelAssistClaimedByGuid != me->GetGUID())
    {
        return false;
    }

    Unit* peelTarget = ObjectAccessor::GetUnit(*me, snapshot.peelTargetGuid);
    if (!peelTarget || !peelTarget->IsAlive() || me->IsFriendlyTo(peelTarget))
        return false;

    if (peelTarget->GetHealthPct() <= 50.0f)
        return false;

    bool const changed = GetAmbientGroupCombatStateService().ClaimPeelAssist(
        _identity.ambientGroupId,
        me->GetGUID(),
        peelTarget->GetGUID(),
        nowMs);

    if (changed)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            std::string("kind='peel_assist_claim' claimer='") + me->GetName()
                + "' claimer_guid=" + std::to_string(me->GetGUID().GetCounter())
                + " owner_guid=" + std::to_string(snapshot.peelClaimedByGuid.GetCounter())
                + " target='" + peelTarget->GetName()
                + "' target_guid=" + std::to_string(peelTarget->GetGUID().GetCounter())
                + " distress_tier='" + ToString(snapshot.distressTier) + "'");
    }

    return changed || snapshot.peelAssistClaimedByGuid == me->GetGUID();
}

bool WorldBotCreatureAI::TryAdoptClaimedPeelTarget(char const* reason)
{
    if (!me || _identity.ambientGroupId == 0 || !IsAmbientDpsRole())
        return false;

    std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();
    service::AmbientGroupCombatSnapshot const snapshot =
        GetAmbientGroupCombatStateService().Get(_identity.ambientGroupId, nowMs);

    if (snapshot.peelClaimedByGuid != me->GetGUID() || snapshot.peelTargetGuid.IsEmpty())
        return false;

    Unit* target = ObjectAccessor::GetUnit(*me, snapshot.peelTargetGuid);
    if (!target || !target->IsAlive() || me->IsFriendlyTo(target))
    {
        (void)GetAmbientGroupCombatStateService().ClearPeel(
            _identity.ambientGroupId,
            me->GetGUID(),
            snapshot.peelTargetGuid);
        return false;
    }

    if (me->GetVictim() == target)
        return false;

    me->EngageWithTarget(target);
    target->EngageWithTarget(me);
    AttackStart(target);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    EnsureMutualThreatEngagement(me, target);

    if (target->IsCreature() && target->ToCreature()->IsAIEnabled)
        target->ToCreature()->AI()->AttackStart(me);

    // A successful peel transitions the danger thread from "save the healer"
    // into normal DPS pressure on the claimant, but do not keep resetting the
    // timer if we reaffirm the same peeled target on later ticks.
    if (!_distressTracker.active || _distressTracker.attackerGuid != target->GetGUID())
    {
        _distressTracker.active = true;
        _distressTracker.attackerGuid = target->GetGUID();
        _distressTracker.startedAtMs = nowMs;
        _distressTracker.lastDamageAtMs = nowMs;
        _distressTracker.publishedTier = service::AmbientGroupDistressTier::None;
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "group_state_read",
        std::string("request_source='peel_claim' target_guid=")
            + std::to_string(target->GetGUID().GetCounter())
            + " target='" + target->GetName()
            + "' reason='" + (reason ? reason : "unknown") + "'");

    RecordCombatTrace(
        BuildCombatMovementTraceDetail("peel_claim_adopt", target));
    return true;
}

bool WorldBotCreatureAI::TryAdoptClaimedPeelAssistTarget(char const* reason)
{
    if (!me || _identity.ambientGroupId == 0 || !IsAmbientDpsRole())
        return false;

    std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();
    service::AmbientGroupCombatSnapshot const snapshot =
        GetAmbientGroupCombatStateService().Get(_identity.ambientGroupId, nowMs);

    if (snapshot.peelAssistClaimedByGuid != me->GetGUID() || snapshot.peelTargetGuid.IsEmpty())
        return false;

    Unit* target = ObjectAccessor::GetUnit(*me, snapshot.peelTargetGuid);
    if (!target || !target->IsAlive() || me->IsFriendlyTo(target))
    {
        (void)GetAmbientGroupCombatStateService().ClearPeelAssist(
            _identity.ambientGroupId,
            me->GetGUID());
        return false;
    }

    if (me->GetVictim() == target)
        return false;

    me->EngageWithTarget(target);
    target->EngageWithTarget(me);
    AttackStart(target);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    EnsureMutualThreatEngagement(me, target);

    if (target->IsCreature() && target->ToCreature()->IsAIEnabled)
        target->ToCreature()->AI()->AttackStart(me);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "group_state_read",
        std::string("request_source='peel_assist_claim' target_guid=")
            + std::to_string(target->GetGUID().GetCounter())
            + " target='" + target->GetName() + "' reason='" + (reason ? reason : "unknown") + "'");

    RecordCombatTrace(
        BuildCombatMovementTraceDetail("peel_assist_adopt", target));
    return true;
}

std::vector<std::uint64_t> WorldBotCreatureAI::CollectAmbientGroupFollowerGuids(float radius) const
{
    std::vector<std::uint64_t> followerGuids;
    if (!me || _identity.ambientGroupId == 0)
        return followerGuids;

    followerGuids.push_back(me->GetGUID().GetCounter());

    for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(me, radius, false))
    {
        if (!IsAmbientGroupedWith(ally))
            continue;

        Creature* creature = ally->ToCreature();
        if (!creature || creature->GetEntry() != WorldBotEntry || !creature->AI())
            continue;

        auto const* allyAi = static_cast<WorldBotCreatureAI const*>(creature->AI());
        if (allyAi->IsAmbientGroupLeader())
            continue;

        followerGuids.push_back(creature->GetGUID().GetCounter());
    }

    return followerGuids;
}

std::vector<Unit*> WorldBotCreatureAI::CollectAmbientGroupBuffTargets(float radius) const
{
    std::vector<Unit*> targets;
    if (!me)
        return targets;

    if (_preparedBuild.oocBehavior.buffScope == model::BotBuffScope::Self
        || _identity.ambientGroupId == 0)
    {
        targets.push_back(me);
        return targets;
    }

    for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(me, radius, true))
    {
        if (ally != me && !IsAmbientGroupedWith(ally))
            continue;

        targets.push_back(ally);
    }

    if (targets.empty())
        targets.push_back(me);

    return targets;
}

Creature* WorldBotCreatureAI::FindAmbientGroupTankCreature(float radius) const
{
    if (!me)
        return nullptr;

    Creature* best = nullptr;
    auto const consider =
        [&](Creature* creature)
        {
            if (!creature || !creature->IsAlive() || !creature->AI())
                return;

            auto const* allyAi = static_cast<WorldBotCreatureAI const*>(creature->AI());
            bool const isTank = allyAi->_identity.ambientGroupRole == "tank"
                || allyAi->_preparedBuild.resolvedRoleKey == "TANK"
                || allyAi->IsAmbientGroupLeader();
            if (!isTank)
                return;

            if (!best || creature->GetMaxHealth() > best->GetMaxHealth())
                best = creature;
        };

    if (_identity.ambientGroupId != 0)
    {
        consider(me);
        for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(me, radius, false))
        {
            if (!IsAmbientGroupedWith(ally))
                continue;

            Creature* creature = ally->ToCreature();
            if (!creature || creature->GetEntry() != WorldBotEntry)
                continue;

            consider(creature);
        }
    }

    return best;
}

Creature* WorldBotCreatureAI::FindAmbientGroupHealerCreature(float radius) const
{
    if (!me || _identity.ambientGroupId == 0)
        return nullptr;

    for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(me, radius, false))
    {
        if (!IsAmbientGroupedWith(ally))
            continue;

        Creature* creature = ally->ToCreature();
        if (!creature || creature->GetEntry() != WorldBotEntry || !creature->AI())
            continue;

        auto const* allyAi = static_cast<WorldBotCreatureAI const*>(creature->AI());
        if (allyAi->_identity.ambientGroupRole == "healer"
            || allyAi->_identity.ambientGroupRole == "support"
            || allyAi->_preparedBuild.resolvedRoleKey == "HEAL")
        {
            return creature;
        }
    }

    return nullptr;
}

void WorldBotCreatureAI::NoteAmbientGroupDistressContact(Unit* attacker)
{
    if (!me || _identity.ambientGroupId == 0 || IsAmbientTankRole()
        || !attacker || !attacker->IsAlive() || me->IsFriendlyTo(attacker))
    {
        return;
    }

    std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();
    if (!_distressTracker.active || _distressTracker.attackerGuid != attacker->GetGUID())
    {
        if (_distressTracker.active
            && IsAmbientHealerRole()
            && !_distressTracker.attackerGuid.IsEmpty()
            && nowMs < (_distressTracker.lastDamageAtMs + DistressQuietClearMs))
        {
            _distressTracker.attackerGuid = attacker->GetGUID();
            _distressTracker.lastDamageAtMs = nowMs;
            return;
        }

        if (_distressTracker.active && !_distressTracker.attackerGuid.IsEmpty())
            (void)GetAmbientGroupCombatStateService().ClearDistress(
                _identity.ambientGroupId,
                me->GetGUID(),
                _distressTracker.attackerGuid);

        _distressTracker.active = true;
        _distressTracker.attackerGuid = attacker->GetGUID();
        _distressTracker.startedAtMs = nowMs;
        _distressTracker.lastDamageAtMs = nowMs;
        _distressTracker.publishedTier = service::AmbientGroupDistressTier::None;
        return;
    }

    _distressTracker.lastDamageAtMs = nowMs;
}

void WorldBotCreatureAI::TickAmbientGroupDistressState()
{
    if (!me || !_distressTracker.active || _identity.ambientGroupId == 0 || IsAmbientTankRole())
        return;

    std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();
    Unit* attacker = ObjectAccessor::GetUnit(*me, _distressTracker.attackerGuid);
    if (!attacker || !attacker->IsAlive() || me->IsFriendlyTo(attacker))
    {
        if (GetAmbientGroupCombatStateService().ClearDistress(
                _identity.ambientGroupId,
                me->GetGUID(),
                _distressTracker.attackerGuid))
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "group_state_publish",
                std::string("kind='distress' tier='cleared' ally='") + me->GetName()
                    + "' ally_guid=" + std::to_string(me->GetGUID().GetCounter())
                    + " attacker_guid=" + std::to_string(_distressTracker.attackerGuid.GetCounter()));
        }
        _distressTracker.Reset();
        return;
    }

    if (nowMs >= (_distressTracker.lastDamageAtMs + DistressQuietClearMs))
    {
        if (GetAmbientGroupCombatStateService().ClearDistress(
                _identity.ambientGroupId,
                me->GetGUID(),
                _distressTracker.attackerGuid))
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "group_state_publish",
                std::string("kind='distress' tier='cleared' ally='") + me->GetName()
                    + "' ally_guid=" + std::to_string(me->GetGUID().GetCounter())
                    + " attacker='" + attacker->GetName()
                    + "' attacker_guid=" + std::to_string(attacker->GetGUID().GetCounter()));
        }
        _distressTracker.Reset();
        return;
    }

    std::uint64_t const elapsedMs = nowMs - _distressTracker.startedAtMs;
    service::AmbientGroupDistressTier desiredTier = service::AmbientGroupDistressTier::None;

    if (IsAmbientHealerRole())
    {
        if (elapsedMs >= 4000u)
            desiredTier = service::AmbientGroupDistressTier::Critical;
        else if (elapsedMs >= 2500u)
            desiredTier = service::AmbientGroupDistressTier::Urgent;
        else
            desiredTier = service::AmbientGroupDistressTier::AssistRequested;
    }
    else if (IsAmbientDpsRole())
    {
        if (elapsedMs < TankWrangleWindowMs)
        {
            desiredTier = service::AmbientGroupDistressTier::Alert;
        }
        else if (elapsedMs >= 5000u)
        {
            if (attacker->GetHealthPct() > 50.0f)
                desiredTier = service::AmbientGroupDistressTier::Urgent;
            else
                desiredTier = service::AmbientGroupDistressTier::Alert;
        }
        else
        {
            if (attacker->GetHealthPct() > 50.0f)
                desiredTier = service::AmbientGroupDistressTier::AssistRequested;
            else
                desiredTier = service::AmbientGroupDistressTier::Alert;
        }
    }

    if (desiredTier == service::AmbientGroupDistressTier::None
        || desiredTier == _distressTracker.publishedTier)
    {
        return;
    }

    if (GetAmbientGroupCombatStateService().PublishDistress(
            _identity.ambientGroupId,
            me->GetGUID(),
            attacker->GetGUID(),
            desiredTier,
            nowMs))
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            std::string("kind='distress' tier='") + ToString(desiredTier)
                + "' ally='" + me->GetName()
                + "' ally_guid=" + std::to_string(me->GetGUID().GetCounter())
                + " attacker='" + attacker->GetName()
                + "' attacker_guid=" + std::to_string(attacker->GetGUID().GetCounter())
                + " attacker_hp_pct=" + std::to_string(attacker->GetHealthPct())
                + " elapsed_ms=" + std::to_string(elapsedMs)
                + " wrangle_window_ms=" + std::to_string(TankWrangleWindowMs));
    }

    _distressTracker.publishedTier = desiredTier;
}

void WorldBotCreatureAI::PublishAmbientGroupTankAnchor(Unit* target) const
{
    if (!me || _identity.ambientGroupId == 0 || !IsAmbientTankRole()
        || !target || !target->IsAlive())
    {
        return;
    }

    if (GetAmbientGroupCombatStateService().ClaimTankAnchor(
        _identity.ambientGroupId,
        me->GetGUID(),
        target->GetGUID(),
        GameTime::GetGameTimeMS().count()))
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            std::string("kind='tank_anchor' tank='") + me->GetName()
                + "' tank_guid=" + std::to_string(me->GetGUID().GetCounter())
                + " target='" + target->GetName()
                + "' target_guid=" + std::to_string(target->GetGUID().GetCounter()) + "'");
    }
}

void WorldBotCreatureAI::PublishAmbientGroupPrimaryTarget(Unit* target) const
{
    if (!me || _identity.ambientGroupId == 0 || !target || !target->IsAlive())
        return;

    if (!IsAmbientTankRole() && !IsAmbientGroupLeader())
        return;

    std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();
    service::AmbientGroupCombatSnapshot const sharedSnapshot =
        GetAmbientGroupCombatStateService().Get(_identity.ambientGroupId, nowMs);
    if (target->GetGUID() == sharedSnapshot.distressTargetGuid
        && IsHealerDistressWithinWrangleWindow(sharedSnapshot, nowMs))
    {
        return;
    }

    if (GetAmbientGroupCombatStateService().PublishPartyPrimaryTarget(
        _identity.ambientGroupId,
        target->GetGUID(),
        nowMs))
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            std::string("kind='party_primary' owner='") + me->GetName()
                + "' owner_guid=" + std::to_string(me->GetGUID().GetCounter())
                + " target='" + target->GetName()
                + "' target_guid=" + std::to_string(target->GetGUID().GetCounter()) + "'");
    }
}

void WorldBotCreatureAI::PublishAmbientGroupPullArming() const
{
    if (_identity.ambientGroupId == 0 || !IsAmbientGroupLeader())
        return;

    if (GetAmbientGroupCombatStateService().SetLeaderPullPhase(
        _identity.ambientGroupId,
        service::AmbientGroupPullPhase::Arming,
        GameTime::GetGameTimeMS().count()))
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            "kind='pull_phase' phase='arming'");
    }
}

void WorldBotCreatureAI::PublishAmbientGroupPullCommitted() const
{
    if (_identity.ambientGroupId == 0 || !IsAmbientGroupLeader())
        return;

    if (GetAmbientGroupCombatStateService().SetLeaderPullPhase(
        _identity.ambientGroupId,
        service::AmbientGroupPullPhase::Committed,
        GameTime::GetGameTimeMS().count()))
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "group_state_publish",
            "kind='pull_phase' phase='committed'");
    }
}

bool WorldBotCreatureAI::TryApplyRoguePoisonsOutOfCombat()
{
    if (!me || !_preparedBuildReady || me->getClass() != CLASS_ROGUE)
        return false;

    RoguePoisonLoadout const poisonLoadout =
        ResolvePreferredRoguePoisons(_preparedBuild.canonicalSpecKey, _preparedBuild.knownSpellIds);
    if (poisonLoadout.primarySpellId == 0)
        return false;

    auto const tryApplyPoison =
        [&](std::uint32_t spellBaseId,
            std::uint32_t spellId,
            char const* slotLabel) -> bool
        {
            if (spellBaseId == 0 || spellId == 0)
                return false;

            if (RecentlyCastTimedSpell(
                    _recentOocBuff,
                    static_cast<std::uint32_t>(_worldOnlineMs),
                    spellBaseId,
                    me->GetGUID(),
                    15000u))
            {
                return false;
            }

            me->CastSpell(me, spellId, false);
            RememberTimedSpell(
                _recentOocBuff,
                static_cast<std::uint32_t>(_worldOnlineMs),
                spellBaseId,
                me->GetGUID());

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "ooc_buff",
                std::string("spell='") + (spellInfo ? spellInfo->SpellName[0] : "Poison")
                    + "' target='" + me->GetName()
                    + "' slot='" + slotLabel + "'");
            return true;
        };

    bool const poisonSetupMatches =
        _roguePoisonState.active
        && _roguePoisonState.primaryPoisonSpellId == poisonLoadout.primarySpellId
        && _roguePoisonState.secondaryPoisonSpellId == poisonLoadout.secondarySpellId;

    if (!poisonSetupMatches)
    {
        if (_roguePoisonState.primaryPoisonSpellId != poisonLoadout.primarySpellId
            && tryApplyPoison(poisonLoadout.primaryBaseSpellId, poisonLoadout.primarySpellId, "mainhand"))
        {
            _roguePoisonState.primaryPoisonBaseSpellId = poisonLoadout.primaryBaseSpellId;
            _roguePoisonState.primaryPoisonSpellId = poisonLoadout.primarySpellId;
            return true;
        }

        if (poisonLoadout.secondarySpellId != 0
            && _roguePoisonState.secondaryPoisonSpellId != poisonLoadout.secondarySpellId
            && tryApplyPoison(poisonLoadout.secondaryBaseSpellId, poisonLoadout.secondarySpellId, "offhand"))
        {
            _roguePoisonState.secondaryPoisonBaseSpellId = poisonLoadout.secondaryBaseSpellId;
            _roguePoisonState.secondaryPoisonSpellId = poisonLoadout.secondarySpellId;
            return true;
        }
    }

    _roguePoisonState.active = true;
    _roguePoisonState.primaryPoisonBaseSpellId = poisonLoadout.primaryBaseSpellId;
    _roguePoisonState.primaryPoisonSpellId = poisonLoadout.primarySpellId;
    _roguePoisonState.secondaryPoisonBaseSpellId = poisonLoadout.secondaryBaseSpellId;
    _roguePoisonState.secondaryPoisonSpellId = poisonLoadout.secondarySpellId;
    if (_roguePoisonState.appliedAtMs == 0)
        _roguePoisonState.appliedAtMs = static_cast<std::uint32_t>(_worldOnlineMs);

    return false;
}

std::vector<Creature*> WorldBotCreatureAI::CollectControlledGuardianPets() const
{
    std::vector<Creature*> pets;

    if (!me)
        return pets;

    AppendWorldBotControlledPet(pets, me, me->GetGuardianPet());

    auto const appendMinionsByEntry =
        [&](std::uint32_t entry)
        {
            if (entry == 0)
                return;

            std::list<Creature*> minions;
            me->GetAllMinionsByEntry(minions, entry);
            for (Creature* minion : minions)
                AppendWorldBotControlledPet(pets, me, minion);
        };

    if (me->getClass() == CLASS_SHAMAN)
    {
        appendMinionsByEntry(NPC_FERAL_SPIRIT);
        appendMinionsByEntry(NPC_FIRE_ELEMENTAL);
    }

    std::sort(
        pets.begin(),
        pets.end(),
        [](Creature const* left, Creature const* right)
        {
            if (left == right)
                return false;
            if (!left)
                return true;
            if (!right)
                return false;
            if (left->GetEntry() != right->GetEntry())
                return left->GetEntry() < right->GetEntry();
            return left->GetGUID() < right->GetGUID();
        });

    return pets;
}

Creature* WorldBotCreatureAI::GetControlledGuardianPet() const
{
    std::vector<Creature*> pets = CollectControlledGuardianPets();
    return pets.empty() ? nullptr : pets.front();
}

bool WorldBotCreatureAI::IsActivelyTravelingForSelfState() const
{
    if (_identity.id != 0)
    {
        std::uint32_t const forcedTravelIdentityId =
            sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceTravelStateIdentityId", 0);
        if (forcedTravelIdentityId != 0 && forcedTravelIdentityId == _identity.id)
            return true;

        std::uint32_t const suppressedTravelIdentityId =
            sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugSuppressTravelStateIdentityId", 0);
        if (suppressedTravelIdentityId != 0 && suppressedTravelIdentityId == _identity.id)
            return false;
    }

    return _traveling
        || _routeTravelPlanActive
        || _activeTravelExecutionPhase != ActiveTravelExecutionPhase::None
        || !_activePhysicalTransit.empty();
}

bool WorldBotCreatureAI::IsPreparedSelfStateActive(model::WorldBotPreparedSelfState const& state) const
{
    if (!me)
        return false;

    if (state.shapeshiftForm != 0)
    {
        ShapeshiftForm const currentForm = me->GetShapeshiftForm();
        if (state.key == "Bear")
        {
            if (currentForm == FORM_BEAR || currentForm == FORM_DIREBEAR)
                return true;
        }
        else if (static_cast<std::uint8_t>(currentForm) == state.shapeshiftForm)
        {
            return true;
        }

        if (state.activeAuraSpellId != 0 && me->HasAura(state.activeAuraSpellId))
            return true;

        return state.spellId != 0 && me->HasAura(state.spellId);
    }

    if (state.activeAuraSpellId != 0)
        return me->HasAura(state.activeAuraSpellId);

    return state.spellId != 0 && me->HasAura(state.spellId);
}

model::WorldBotPreparedSelfState const* WorldBotCreatureAI::SelectPreparedSelfStateForCurrentContext() const
{
    if (!_preparedBuildReady || _preparedBuild.selfStates.empty())
        return nullptr;

    bool const inCombat = me && (me->IsInCombat() || me->GetVictim());
    bool const traveling = !inCombat && IsActivelyTravelingForSelfState();

    for (model::WorldBotPreparedSelfState const& state : _preparedBuild.selfStates)
    {
        if (traveling && state.preferredWhileTraveling)
            return &state;
    }

    for (model::WorldBotPreparedSelfState const& state : _preparedBuild.selfStates)
    {
        if (inCombat && state.preferredInCombat)
            return &state;
    }

    for (model::WorldBotPreparedSelfState const& state : _preparedBuild.selfStates)
    {
        if (!inCombat && state.preferredOutOfCombat)
            return &state;
    }

    return nullptr;
}

bool WorldBotCreatureAI::TryApplyPreferredSelfState()
{
    if (!me || !_preparedBuildReady || !me->IsAlive() || !me->IsInWorld())
        return false;

    if (_combatInterrupt.active || me->IsNonMeleeSpellCast(false))
        return false;

    model::WorldBotPreparedSelfState const* desiredState = SelectPreparedSelfStateForCurrentContext();
    if (!desiredState || desiredState->spellId == 0 || IsPreparedSelfStateActive(*desiredState))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(desiredState->spellId);
    if (!spellInfo)
        return false;

    bool applied = false;
    if (Aura* aura = me->AddAura(desiredState->spellId, me))
        applied = aura != nullptr;

    if (!applied)
        me->CastSpell(me, desiredState->spellId, false);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "self_state",
        std::string("category='")
            + [&]()
            {
                switch (desiredState->category)
                {
                    case model::WorldBotSelfStateCategory::Form: return "form";
                    case model::WorldBotSelfStateCategory::Stance: return "stance";
                    case model::WorldBotSelfStateCategory::Presence: return "presence";
                    case model::WorldBotSelfStateCategory::Aspect: return "aspect";
                    case model::WorldBotSelfStateCategory::Armor: return "armor";
                    case model::WorldBotSelfStateCategory::Seal: return "seal";
                    default: return "unknown";
                }
            }()
            + "' key='" + desiredState->key
            + "' spell='" + spellInfo->SpellName[0]
            + "' target='" + me->GetName() + "'");
    return true;
}

void WorldBotCreatureAI::PopulateProjectedCreatureSpellbook()
{
    if (!me || !_preparedBuildReady)
        return;

    std::array<std::uint32_t, MAX_CREATURE_SPELLS> projectedSpells {};
    std::size_t nextSlot = 0;

    auto const appendSpell =
        [&](std::uint32_t spellId)
        {
            if (spellId == 0 || nextSlot >= projectedSpells.size())
                return;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!IsProjectedWorldBotSpellCandidate(spellInfo))
                return;

            for (std::uint32_t knownSpellId : projectedSpells)
            {
                if (knownSpellId == spellId)
                    return;
            }

            projectedSpells[nextSlot++] = spellId;
        };

    // First, pin important non-rotation utility that other world-bot helpers
    // query through Creature::HasSpell.
    if (_preparedBuild.knownSpellIds.count(SummonWaterElementalSpellId))
        appendSpell(SummonWaterElementalSpellId);
    if (_preparedBuild.knownSpellIds.count(RaiseDeadSpellId))
        appendSpell(RaiseDeadSpellId);
    appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonFelguardSpellId));
    appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonFelhunterSpellId));
    appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonVoidwalkerSpellId));
    appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonImpSpellId));
    appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonSuccubusSpellId));
    for (model::WorldBotPreparedSelfState const& selfState : _preparedBuild.selfStates)
        appendSpell(selfState.spellId);

    switch (me->getClass())
    {
        case CLASS_WARRIOR:
            appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 6673));
            break;
        case CLASS_DEATH_KNIGHT:
            if (_preparedBuild.knownSpellIds.count(57330u))
                appendSpell(57330u);
            break;
        case CLASS_PALADIN:
        {
            appendSpell(GetPreferredSeal(_preparedBuild.knownSpellIds));
            std::uint32_t const kingsSpellId = FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 20217);
            std::uint32_t const mightSpellId = FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 19740);
            appendSpell(kingsSpellId != 0 ? kingsSpellId : mightSpellId);
            break;
        }
        case CLASS_PRIEST:
            appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 1243));
            break;
        case CLASS_DRUID:
            appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 1126));
            break;
        case CLASS_MAGE:
            appendSpell(FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 1459));
            break;
        case CLASS_WARLOCK:
        {
            std::uint32_t armorSpellId = FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 28176);
            if (armorSpellId == 0)
                armorSpellId = FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 706);
            if (armorSpellId == 0)
                armorSpellId = FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, 696);
            appendSpell(armorSpellId);
            break;
        }
        case CLASS_ROGUE:
        {
            RoguePoisonLoadout const poisonLoadout =
                ResolvePreferredRoguePoisons(_preparedBuild.canonicalSpecKey, _preparedBuild.knownSpellIds);
            appendSpell(poisonLoadout.primarySpellId);
            appendSpell(poisonLoadout.secondarySpellId);
            break;
        }
        default:
            break;
    }

    // Then project the combat-profile surface so creature spell lookups reflect
    // the actual active buttons this build is expected to use.
    service::BotCombatPreparedProfile const projectedProfile =
        GetProfilePreparationService().PrepareForWorldBot(
            me,
            _preparedBuild.knownSpellIds,
            _preparedBuild.canonicalSpecKey,
            _preparedBuild.resolvedRoleKey,
            _preparedBuild.contextKey,
            _preparedBuild.requestedLoadoutKey);

    auto const appendEntrySpells =
        [&](std::vector<model::BotCombatEntryDefinition> const& entries)
        {
            for (model::BotCombatEntryDefinition const& entry : entries)
            {
                appendSpell(service::BotCombatProfilePreparationService::ResolveKnownSpellForAction(
                    projectedProfile.availableSpells,
                    entry.primaryAction));
                if (entry.secondaryAction)
                {
                    appendSpell(service::BotCombatProfilePreparationService::ResolveKnownSpellForAction(
                        projectedProfile.availableSpells,
                        *entry.secondaryAction));
                }
            }
        };

    appendEntrySpells(projectedProfile.interruptEntries);
    appendEntrySpells(projectedProfile.rotationEntries);

    // Finally, backfill any remaining space with deterministic active known
    // spells so utility helpers still see a sensible creature spell surface.
    if (nextSlot < projectedSpells.size())
    {
        std::vector<std::uint32_t> fallbackSpells(
            _preparedBuild.knownSpellIds.begin(),
            _preparedBuild.knownSpellIds.end());
        std::sort(fallbackSpells.begin(), fallbackSpells.end());
        for (std::uint32_t spellId : fallbackSpells)
        {
            appendSpell(spellId);
            if (nextSlot >= projectedSpells.size())
                break;
        }
    }

    for (std::size_t i = 0; i < projectedSpells.size(); ++i)
        me->m_spells[i] = projectedSpells[i];

    std::ostringstream detail;
    detail << "projected_slots=" << nextSlot;
    for (std::size_t i = 0; i < nextSlot; ++i)
        detail << " spell_" << i << "=" << projectedSpells[i];
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "spellbook_snapshot",
        detail.str());
}

bool WorldBotCreatureAI::TryMaintainBasicCompanionPet()
{
    if (!me || !_preparedBuildReady || !me->IsAlive())
        return false;

    if (!CollectControlledGuardianPets().empty())
        return false;

    constexpr std::uint32_t summonRetryMs = 5000u;
    constexpr std::uint32_t statusLogThrottleMs = 60000u;
    auto const logStatus =
        [&](std::string detail)
        {
            if (_worldOnlineMs < (_lastControlledPetStatusLogWorldMs + statusLogThrottleMs))
                return;

            _lastControlledPetStatusLogWorldMs = _worldOnlineMs;
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "pet_control",
                std::move(detail));
        };

    model::BotCombatMode const forcedBotMode = ResolveDebugForcedBotMode();
    bool const allowCombatSummon =
        forcedBotMode == model::BotCombatMode::Passive
        || forcedBotMode == model::BotCombatMode::Hold;

    if (((me->IsInCombat() || me->GetVictim()) && !allowCombatSummon)
        || me->IsNonMeleeSpellCast(false)
        || _syntheticGlobalCooldownRemainingMs > 0)
        return false;

    if (_worldOnlineMs < (_lastControlledPetSummonAttemptWorldMs + summonRetryMs))
        return false;

    bool const preparedHasWaterElemental =
        _preparedBuild.knownSpellIds.find(SummonWaterElementalSpellId) != _preparedBuild.knownSpellIds.end();
    bool const unitHasWaterElemental = me->HasSpell(SummonWaterElementalSpellId);
    bool const preparedHasRaiseDead =
        _preparedBuild.knownSpellIds.find(RaiseDeadSpellId) != _preparedBuild.knownSpellIds.end();
    bool const unitHasRaiseDead = me->HasSpell(RaiseDeadSpellId);
    std::uint32_t const knownFelguardSpellId =
        FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonFelguardSpellId);
    std::uint32_t const knownFelhunterSpellId =
        FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonFelhunterSpellId);
    std::uint32_t const knownVoidwalkerSpellId =
        FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonVoidwalkerSpellId);
    std::uint32_t const knownImpSpellId =
        FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonImpSpellId);
    std::uint32_t const knownSuccubusSpellId =
        FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, SummonSuccubusSpellId);

    std::uint32_t summonSpellId = 0;
    bool summonPreparedFlag = false;
    bool summonUnitFlag = false;
    if (me->getClass() == CLASS_MAGE
        && _preparedBuild.canonicalSpecKey == "Frost"
        && unitHasWaterElemental)
    {
        summonSpellId = SummonWaterElementalSpellId;
        summonPreparedFlag = preparedHasWaterElemental;
        summonUnitFlag = unitHasWaterElemental;
    }
    else if (me->getClass() == CLASS_DEATH_KNIGHT
        && _preparedBuild.canonicalSpecKey == "Unholy"
        && me->HasAura(MasterOfGhoulsSpellId))
    {
        summonSpellId = RaiseDeadPetSummonSpellId;
        summonPreparedFlag = preparedHasRaiseDead;
        summonUnitFlag = unitHasRaiseDead;
    }
    else if (me->getClass() == CLASS_WARLOCK)
    {
        if (_preparedBuild.canonicalSpecKey == "Demonology" && knownFelguardSpellId != 0)
            summonSpellId = knownFelguardSpellId;
        else if (_preparedBuild.canonicalSpecKey == "Affliction" && knownFelhunterSpellId != 0)
            summonSpellId = knownFelhunterSpellId;
        else if (_preparedBuild.canonicalSpecKey == "Destruction" && knownImpSpellId != 0)
            summonSpellId = knownImpSpellId;
        else if (knownFelhunterSpellId != 0)
            summonSpellId = knownFelhunterSpellId;
        else if (knownVoidwalkerSpellId != 0)
            summonSpellId = knownVoidwalkerSpellId;
        else if (knownImpSpellId != 0)
            summonSpellId = knownImpSpellId;
        else if (knownSuccubusSpellId != 0)
            summonSpellId = knownSuccubusSpellId;

        summonPreparedFlag = summonSpellId != 0;
        summonUnitFlag = summonSpellId != 0 && me->HasSpell(summonSpellId);
    }
    else if (me->getClass() == CLASS_HUNTER)
    {
        return TrySummonDirectHunterPet();
    }

    if (summonSpellId == 0)
    {
        if (me->getClass() == CLASS_MAGE && _preparedBuild.canonicalSpecKey == "Frost")
        {
            logStatus(
                "event='summon_skip' reason='spell_unavailable' prepared_has_spell="
                + std::to_string(preparedHasWaterElemental ? 1 : 0)
                + " unit_has_spell=" + std::to_string(unitHasWaterElemental ? 1 : 0)
                + " spec='" + _preparedBuild.canonicalSpecKey + "'");
        }
        else if (me->getClass() == CLASS_DEATH_KNIGHT && _preparedBuild.canonicalSpecKey == "Unholy")
        {
            logStatus(
                "event='summon_skip' reason='spell_unavailable' prepared_has_spell="
                + std::to_string(preparedHasRaiseDead ? 1 : 0)
                + " unit_has_spell=" + std::to_string(unitHasRaiseDead ? 1 : 0)
                + " master_of_ghouls=" + std::to_string(me->HasAura(MasterOfGhoulsSpellId) ? 1 : 0)
                + " glyph_of_the_ghoul=" + std::to_string(me->HasAura(GlyphOfTheGhoulSpellId) ? 1 : 0)
                + " spec='" + _preparedBuild.canonicalSpecKey + "'");
        }
        else if (me->getClass() == CLASS_WARLOCK)
        {
            logStatus(
                "event='summon_skip' reason='spell_unavailable' spec='" + _preparedBuild.canonicalSpecKey
                + "' felguard=" + std::to_string(knownFelguardSpellId)
                + " felhunter=" + std::to_string(knownFelhunterSpellId)
                + " voidwalker=" + std::to_string(knownVoidwalkerSpellId)
                + " imp=" + std::to_string(knownImpSpellId)
                + " succubus=" + std::to_string(knownSuccubusSpellId));
        }
        return false;
    }

    if (me->HasSpellCooldown(summonSpellId))
    {
        logStatus(
            "event='summon_skip' reason='cooldown' spell_id=" + std::to_string(summonSpellId)
            + " spec='" + _preparedBuild.canonicalSpecKey + "'");
        return false;
    }

    _lastControlledPetSummonAttemptWorldMs = _worldOnlineMs;
    SpellCastResult const result = me->CastSpell(me, summonSpellId, false);
    if (result != SPELL_CAST_OK)
    {
        logStatus(
            "event='summon_fail' spell_id=" + std::to_string(summonSpellId)
            + " cast_result=" + std::to_string(static_cast<std::uint32_t>(result))
            + " prepared_has_spell=" + std::to_string(summonPreparedFlag ? 1 : 0)
            + " unit_has_spell=" + std::to_string(summonUnitFlag ? 1 : 0));
        return false;
    }

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(summonSpellId))
    {
        _syntheticGlobalCooldownRemainingMs = std::max(
            _syntheticGlobalCooldownRemainingMs,
            ComputeSyntheticCreatureGlobalCooldownMs(me, spellInfo));
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "pet_control",
        "event='summon_attempt' spell_id=" + std::to_string(summonSpellId)
            + " prepared_has_spell=" + std::to_string(summonPreparedFlag ? 1 : 0)
            + " unit_has_spell=" + std::to_string(summonUnitFlag ? 1 : 0));
    return true;
}

bool WorldBotCreatureAI::TrySummonDirectHunterPet()
{
    if (!me || !_preparedBuildReady || !me->IsAlive() || me->getClass() != CLASS_HUNTER)
        return false;

    constexpr std::uint32_t summonRetryMs = 5000u;
    if (_worldOnlineMs < (_lastControlledPetSummonAttemptWorldMs + summonRetryMs))
        return false;

    SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(ControlledPetSummonPropertiesId);
    if (!properties)
        return false;

    Position summonPos;
    me->GetClosePoint(
        summonPos.m_positionX,
        summonPos.m_positionY,
        summonPos.m_positionZ,
        me->GetCombatReach(),
        PET_FOLLOW_DIST,
        PET_FOLLOW_ANGLE);
    summonPos.m_orientation = me->GetOrientation();

    _lastControlledPetSummonAttemptWorldMs = _worldOnlineMs;
    TempSummon* summon = me->GetMap()->SummonCreature(
        HunterWorldBotDefaultPetEntry,
        summonPos,
        properties,
        0,
        me,
        0,
        0,
        false);
    if (!summon)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "pet_control",
            "event='summon_fail' direct_entry=" + std::to_string(HunterWorldBotDefaultPetEntry)
                + " reason='direct_guardian_create_failed'");
        return false;
    }

    summon->SetFaction(me->GetFaction());
    summon->setPowerType(POWER_FOCUS);
    summon->SetMaxPower(POWER_FOCUS, 100);
    summon->SetPower(POWER_FOCUS, 100);
    summon->AddAura(SPELL_PET_AVOIDANCE, summon);
    summon->AddAura(SPELL_HUNTER_PET_SCALING_01, summon);
    summon->AddAura(SPELL_HUNTER_PET_SCALING_02, summon);
    summon->AddAura(SPELL_HUNTER_PET_SCALING_03, summon);
    summon->AddAura(SPELL_HUNTER_PET_SCALING_04, summon);
    summon->UpdateAllStats();

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "pet_control",
        "event='summon_attempt' direct_entry=" + std::to_string(HunterWorldBotDefaultPetEntry)
            + " source='direct_hunter_pet'");
    return true;
}

void WorldBotCreatureAI::InitializeControlledGuardianPet(Creature* summon)
{
    if (!IsEligibleWorldBotControlledPet(me, summon))
        return;

    summon->SetFaction(me->GetFaction());
    summon->SetReactState(REACT_DEFENSIVE);

    if (CharmInfo* charmInfo = summon->GetCharmInfo())
    {
        charmInfo->SetCommandState(COMMAND_FOLLOW);
        charmInfo->SetIsCommandAttack(false);
        charmInfo->SetIsCommandFollow(true);
        charmInfo->SetIsAtStay(false);
        charmInfo->SetIsFollowing(false);
        charmInfo->SetIsReturning(false);
    }

    summon->GetMotionMaster()->MoveFollow(me, PET_FOLLOW_DIST, summon->GetFollowAngle());

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "pet_control",
        "event='summoned' pet='" + summon->GetName() + "' guid="
            + std::to_string(summon->GetGUID().GetCounter())
            + " entry=" + std::to_string(summon->GetEntry()));
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "glyph_effect_snapshot",
        DescribeControlledPetEffectSnapshot(me, summon));

    if (Unit* victim = me->GetVictim())
        SyncControlledGuardianPetAssist(victim);
    else if (!me->getAttackers().empty())
        SyncControlledGuardianPetDefend(*me->getAttackers().begin());
}

void WorldBotCreatureAI::SyncControlledGuardianPetFollow()
{
    std::vector<Creature*> pets = CollectControlledGuardianPets();
    if (pets.empty() || me->IsInCombat() || me->GetVictim())
        return;

    for (Creature* pet : pets)
    {
        if (!pet || !pet->IsAlive())
            continue;

        if (pet->GetVictim())
        {
            pet->AttackStop();
            pet->CombatStop();
        }

        if (CharmInfo* charmInfo = pet->GetCharmInfo())
        {
            charmInfo->SetCommandState(COMMAND_FOLLOW);
            charmInfo->SetIsCommandAttack(false);
            charmInfo->SetIsCommandFollow(true);
            charmInfo->SetIsAtStay(false);
            charmInfo->SetIsReturning(false);
        }

        if (pet->GetDistance(me) > (PET_FOLLOW_DIST + 2.0f))
            pet->GetMotionMaster()->MoveFollow(me, PET_FOLLOW_DIST, pet->GetFollowAngle());

        auto const itr = _controlledPetAssistTargets.find(pet->GetGUID());
        if (itr != _controlledPetAssistTargets.end() && !itr->second.IsEmpty())
        {
            itr->second.Clear();
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "pet_control",
                    "event='follow_return' pet='" + pet->GetName() + "' guid="
                        + std::to_string(pet->GetGUID().GetCounter()));
        }
    }
}

void WorldBotCreatureAI::SyncControlledGuardianPetAssist(Unit* target)
{
    std::vector<Creature*> pets = CollectControlledGuardianPets();
    if (pets.empty() || !target)
        return;

    for (Creature* pet : pets)
    {
        if (!pet || !pet->IsAlive() || !pet->CanCreatureAttack(target))
            continue;

        pet->SetReactState(REACT_DEFENSIVE);
        if (CharmInfo* charmInfo = pet->GetCharmInfo())
        {
            charmInfo->SetCommandState(COMMAND_FOLLOW);
            charmInfo->SetIsCommandAttack(true);
            charmInfo->SetIsCommandFollow(false);
            charmInfo->SetIsAtStay(false);
            charmInfo->SetIsReturning(false);
            charmInfo->SetIsFollowing(false);
        }

        if (pet->GetVictim() != target)
        {
            if (pet->AI())
                pet->AI()->AttackStart(target);
            else
                pet->GetMotionMaster()->MoveChase(target);

            ObjectGuid& lastTargetGuid = _controlledPetAssistTargets[pet->GetGUID()];
            if (lastTargetGuid != target->GetGUID())
            {
                lastTargetGuid = target->GetGUID();
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "pet_control",
                    "event='assist' pet='" + pet->GetName() + "' guid="
                        + std::to_string(pet->GetGUID().GetCounter())
                        + " target=" + DescribeTraceUnit(target));
            }
        }
    }
}

void WorldBotCreatureAI::SyncControlledGuardianPetDefend(Unit* attacker)
{
    std::vector<Creature*> pets = CollectControlledGuardianPets();
    if (pets.empty() || !attacker || !me || !me->IsAlive())
        return;

    for (Creature* pet : pets)
    {
        if (!pet || !pet->IsAlive())
            continue;

        if (attacker == pet || attacker == me)
            continue;

        if (!pet->CanCreatureAttack(attacker))
            continue;

        pet->SetReactState(REACT_DEFENSIVE);
        if (CharmInfo* charmInfo = pet->GetCharmInfo())
        {
            charmInfo->SetCommandState(COMMAND_FOLLOW);
            charmInfo->SetIsCommandAttack(true);
            charmInfo->SetIsCommandFollow(false);
            charmInfo->SetIsAtStay(false);
            charmInfo->SetIsReturning(false);
            charmInfo->SetIsFollowing(false);
        }

        if (pet->GetVictim() != attacker)
        {
            if (pet->AI())
                pet->AI()->AttackStart(attacker);
            else
                pet->GetMotionMaster()->MoveChase(attacker);
        }

        ObjectGuid& lastTargetGuid = _controlledPetAssistTargets[pet->GetGUID()];
        if (lastTargetGuid != attacker->GetGUID())
        {
            lastTargetGuid = attacker->GetGUID();
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "pet_control",
                "event='defend_owner' pet='" + pet->GetName() + "' guid="
                    + std::to_string(pet->GetGUID().GetCounter())
                    + " attacker=" + DescribeTraceUnit(attacker));
        }
    }
}

bool WorldBotCreatureAI::IsCurrentTaskCityPotionRefillEligible() const
{
    if (!_sessionReady || _currentStep >= _session.steps.size())
        return false;

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type == service::AmbientStepType::Travel
        || step.type == service::AmbientStepType::Transit)
    {
        return false;
    }

    if (step.taskIndex < 0 || static_cast<std::size_t>(step.taskIndex) >= _session.tasks.size())
        return false;

    service::AmbientSessionTask const& task =
        _session.tasks[static_cast<std::size_t>(step.taskIndex)];
    if (task.taskFamily != "city_errand")
        return false;

    std::string const& pointType = !step.targetPointType.empty()
        ? step.targetPointType
        : task.targetPointType;
    return pointType == "mailbox" || pointType == "auction_house";
}

bool WorldBotCreatureAI::TryRefillGenericPotionsFromCityService()
{
    if (!me || !IsCurrentTaskCityPotionRefillEligible())
        return false;

    if (_genericPotionCharges >= MaxGenericPotionCharges)
        return false;

    std::uint8_t const before = _genericPotionCharges;
    _genericPotionCharges = MaxGenericPotionCharges;
    _identity.genericPotionCharges = _genericPotionCharges;
    GetIdentityRepo().UpdateGenericPotionCharges(_identity.id, _genericPotionCharges);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "inventory_refresh",
        "item='generic_potion' before=" + std::to_string(before)
            + " after=" + std::to_string(_genericPotionCharges));
    return true;
}

bool WorldBotCreatureAI::TryUseAutomaticCombatPotion(Unit* target)
{
    if (!me || !target || _genericPotionCharges == 0 || me->IsNonMeleeSpellCast(false))
        return false;

    std::uint32_t itemId = 0;
    char const* potionKind = nullptr;

    if (me->GetHealthPct() <= AutomaticHealingPotionThresholdPct)
    {
        itemId = service::ResolveGenericHealingPotionItemIdForLevel(_identity.level);
        potionKind = "healing";
    }
    else if (me->GetMaxPower(POWER_MANA) > 0
        && GetUnitManaPct(me) <= AutomaticManaPotionThresholdPct)
    {
        itemId = service::ResolveGenericManaPotionItemIdForLevel(_identity.level);
        potionKind = "mana";
    }

    if (itemId == 0 || !potionKind)
        return false;

    service::BotCombatEvaluatedAction action;
    action.actionType = model::BotCombatActionType::Item;
    action.itemId = itemId;
    action.itemSelector = std::string(potionKind == std::string_view("healing") ? "hp" : "mp");
    action.simulatedItemUse = true;
    action.target = me;
    action.targetKey = "self";
    action.entryLabel = std::string("Auto Generic ") + potionKind + " potion";

    if (!service::CanUseSimulatedCombatItem(
            me,
            me,
            itemId,
            &_usedSimulatedItemsThisCombat,
            &_assignedGearItemIds,
            &_simulatedPotionUsesThisSession,
            MaxSimulatedPotionUsesPerSession,
            _syntheticGlobalCooldownRemainingMs))
    {
        return false;
    }

    service::BotCombatActionDispatchResult const dispatchResult =
        service::DispatchEvaluatedAction(me, action);
    if (!dispatchResult.dispatched)
        return false;

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(dispatchResult.resolvedSpellId))
        _syntheticGlobalCooldownRemainingMs =
            ComputeSyntheticCreatureGlobalCooldownMs(me, spellInfo);

    _usedSimulatedItemsThisCombat.insert(itemId);

    if (service::DoesSimulatedCombatItemCountAsPotionUse(itemId)
        && _simulatedPotionUsesThisSession < MaxSimulatedPotionUsesPerSession)
    {
        ++_simulatedPotionUsesThisSession;
    }

    std::uint8_t const chargesBefore = _genericPotionCharges;
    --_genericPotionCharges;
    _identity.genericPotionCharges = _genericPotionCharges;
    GetIdentityRepo().UpdateGenericPotionCharges(_identity.id, _genericPotionCharges);

    std::ostringstream potionTrace;
    potionTrace << "phase='combat' decision='auto_generic_potion' "
        << "kind='" << potionKind << "' "
        << "item_id=" << itemId << " "
        << "resolved_spell=" << dispatchResult.resolvedSpellId << " "
        << "charges_before=" << static_cast<std::uint32_t>(chargesBefore) << " "
        << "charges_after=" << static_cast<std::uint32_t>(_genericPotionCharges) << " "
        << "session_uses=" << static_cast<std::uint32_t>(_simulatedPotionUsesThisSession) << " "
        << "self_hp_pct=" << me->GetHealthPct() << " "
        << "self_mana_pct=" << GetUnitManaPct(me);
    RecordCombatTrace(potionTrace.str());

    return true;
}

void WorldBotCreatureAI::TryApplyOutOfCombatBuff()
{
    if (!me || !_preparedBuildReady)
        return;

    if (_combatInterrupt.active)
        return;

    if (!me->IsAlive() || !me->IsInWorld() || me->IsInCombat() || me->GetVictim())
        return;

    if (me->IsNonMeleeSpellCast(false))
        return;

    if (TryApplyPreferredSelfState())
        return;

    model::BotOocBehavior const& oocBehavior = _preparedBuild.oocBehavior;
    if (oocBehavior.buffScope == model::BotBuffScope::Off)
        return;

    std::uint16_t const refreshSecs = oocBehavior.buffReapplySecs;
    auto const& knownSpells = _preparedBuild.knownSpellIds;
    std::vector<Unit*> const buffTargets = CollectAmbientGroupBuffTargets(AmbientGroupBuffRadius);
    auto const recentlyBuffed =
        [&](std::uint32_t spellBaseId, Unit const* target, std::uint32_t windowMs = 15000u) -> bool
        {
            return target
                && RecentlyCastTimedSpell(
                    _recentOocBuff,
                    static_cast<std::uint32_t>(_worldOnlineMs),
                    spellBaseId,
                    target->GetGUID(),
                    windowMs);
        };
    auto const rememberBuff =
        [&](std::uint32_t spellBaseId, Unit const* target)
        {
            if (!target)
                return;
            RememberTimedSpell(
                _recentOocBuff,
                static_cast<std::uint32_t>(_worldOnlineMs),
                spellBaseId,
                target->GetGUID());
        };

    switch (me->getClass())
    {
        case CLASS_WARRIOR:
        {
            std::uint32_t const shoutSpellId = FindBestKnownSpellInChain(knownSpells, 6673);
            if (shoutSpellId != 0 && AuraNeedsRefresh(me, 6673, refreshSecs) && !recentlyBuffed(6673, me))
            {
                me->CastSpell(me, shoutSpellId, false);
                rememberBuff(6673, me);
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "ooc_buff",
                    std::string("spell='Battle Shout' target='") + me->GetName() + "'");
            }
            break;
        }

        case CLASS_DEATH_KNIGHT:
        {
            if (knownSpells.count(57330) != 0 && AuraNeedsRefresh(me, 57330, refreshSecs) && !recentlyBuffed(57330, me))
            {
                me->CastSpell(me, 57330u, false);
                rememberBuff(57330, me);
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "ooc_buff",
                    std::string("spell='Horn of Winter' target='") + me->GetName() + "'");
            }
            break;
        }

        case CLASS_PALADIN:
        {
            if (!HasSealActive(me))
            {
                std::uint32_t const sealSpellId = GetPreferredSeal(knownSpells);
                if (sealSpellId != 0 && !recentlyBuffed(sealSpellId, me))
                {
                    me->CastSpell(me, sealSpellId, false);
                    rememberBuff(sealSpellId, me);
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(sealSpellId);
                    integration::BotActivityLog::Record(
                        me,
                        _identity.name,
                        _identity.id,
                        "ooc_buff",
                        std::string("spell='") + (spellInfo ? spellInfo->SpellName[0] : "Seal")
                            + "' target='" + me->GetName() + "'");
                    return;
                }
            }

            std::uint32_t const kingsSpellId = FindBestKnownSpellInChain(knownSpells, 20217);
            std::uint32_t const mightSpellId = FindBestKnownSpellInChain(knownSpells, 19740);
            std::uint32_t const blessingSpellId = kingsSpellId != 0 ? kingsSpellId : mightSpellId;
            std::uint32_t const blessingBaseId = kingsSpellId != 0 ? 20217u : 19740u;
            if (blessingSpellId == 0)
                break;

            for (Unit* target : buffTargets)
            {
                if (AuraNeedsRefresh(target, blessingBaseId, refreshSecs)
                    && !recentlyBuffed(blessingBaseId, target))
                {
                    me->CastSpell(target, blessingSpellId, false);
                    rememberBuff(blessingBaseId, target);
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(blessingSpellId);
                    integration::BotActivityLog::Record(
                        me,
                        _identity.name,
                        _identity.id,
                        "ooc_buff",
                        std::string("spell='") + (spellInfo ? spellInfo->SpellName[0] : "Blessing")
                            + "' target='" + target->GetName() + "'");
                    return;
                }
            }
            break;
        }

        case CLASS_PRIEST:
        {
            std::uint32_t const fortitudeSpellId = FindBestKnownSpellInChain(knownSpells, 1243);
            if (fortitudeSpellId == 0)
                break;

            for (Unit* target : buffTargets)
            {
                if (AuraNeedsRefresh(target, 1243, refreshSecs) && !recentlyBuffed(1243, target))
                {
                    me->CastSpell(target, fortitudeSpellId, false);
                    rememberBuff(1243, target);
                    integration::BotActivityLog::Record(
                        me,
                        _identity.name,
                        _identity.id,
                        "ooc_buff",
                        std::string("spell='Power Word: Fortitude' target='") + target->GetName() + "'");
                    return;
                }
            }
            break;
        }

        case CLASS_DRUID:
        {
            std::uint32_t const markSpellId = FindBestKnownSpellInChain(knownSpells, 1126);
            if (markSpellId == 0)
                break;

            for (Unit* target : buffTargets)
            {
                if (AuraNeedsRefresh(target, 1126, refreshSecs) && !recentlyBuffed(1126, target))
                {
                    me->CastSpell(target, markSpellId, false);
                    rememberBuff(1126, target);
                    integration::BotActivityLog::Record(
                        me,
                        _identity.name,
                        _identity.id,
                        "ooc_buff",
                        std::string("spell='Mark of the Wild' target='") + target->GetName() + "'");
                    return;
                }
            }
            break;
        }

        case CLASS_MAGE:
        {
            std::uint32_t const intellectSpellId = FindBestKnownSpellInChain(knownSpells, 1459);
            if (intellectSpellId == 0)
                break;

            for (Unit* target : buffTargets)
            {
                if (AuraNeedsRefresh(target, 1459, refreshSecs) && !recentlyBuffed(1459, target))
                {
                    me->CastSpell(target, intellectSpellId, false);
                    rememberBuff(1459, target);
                    integration::BotActivityLog::Record(
                        me,
                        _identity.name,
                        _identity.id,
                        "ooc_buff",
                        std::string("spell='Arcane Intellect' target='") + target->GetName() + "'");
                    return;
                }
            }
            break;
        }

        case CLASS_WARLOCK:
        {
            std::uint32_t armorSpellId = FindBestKnownSpellInChain(knownSpells, 28176);
            if (armorSpellId == 0)
                armorSpellId = FindBestKnownSpellInChain(knownSpells, 706);
            if (armorSpellId == 0)
                armorSpellId = FindBestKnownSpellInChain(knownSpells, 696);

            if (armorSpellId != 0 && AuraNeedsRefresh(me, armorSpellId, refreshSecs)
                && !recentlyBuffed(armorSpellId, me))
            {
                me->CastSpell(me, armorSpellId, false);
                rememberBuff(armorSpellId, me);
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(armorSpellId);
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "ooc_buff",
                    std::string("spell='") + (spellInfo ? spellInfo->SpellName[0] : "Armor")
                        + "' target='" + me->GetName() + "'");
            }
            break;
        }

        default:
            break;
    }
}

bool WorldBotCreatureAI::TryApplyPrePullSupport()
{
    if (!me || !_preparedBuildReady || me->IsInCombat() || me->GetVictim() || me->IsNonMeleeSpellCast(false))
        return false;

    bool const isHealer =
        _identity.ambientGroupRole == "healer"
        || _identity.ambientGroupRole == "support"
        || _preparedBuild.resolvedRoleKey == "HEAL";
    if (!isHealer)
        return false;

    Creature* tank = FindAmbientGroupTankCreature(AmbientPullReadinessRadius);
    if (!tank)
        return false;

    auto tryTankPrep = [&](std::uint32_t spellBaseId,
                           char const* spellLabel,
                           std::uint16_t refreshSecs,
                           char const* prepRole = "tank_anchor") -> bool
    {
        if (RecentlyCastTimedSpell(
                _recentPullPrep,
                static_cast<std::uint32_t>(_worldOnlineMs),
                spellBaseId,
                tank->GetGUID(),
                15000u))
        {
            return false;
        }

        std::uint32_t const spellId = FindBestKnownSpellInChain(_preparedBuild.knownSpellIds, spellBaseId);
        if (spellId == 0 || !AuraNeedsRefresh(tank, spellBaseId, refreshSecs))
            return false;

        me->CastSpell(tank, spellId, false);
        RememberTimedSpell(
            _recentPullPrep,
            static_cast<std::uint32_t>(_worldOnlineMs),
            spellBaseId,
            tank->GetGUID());
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "pull_prep",
            std::string("spell='") + spellLabel + "' target='" + tank->GetName() + "' role='" + prepRole + "'");
        return true;
    };

    switch (me->getClass())
    {
        case CLASS_PALADIN:
            if (tryTankPrep(53563, "Beacon of Light", 8, "tank_beacon"))
                return true;
            if (tryTankPrep(53601, "Sacred Shield", 5))
                return true;
            break;

        case CLASS_PRIEST:
            if (tryTankPrep(17, "Power Word: Shield", 4, "tank_shield"))
                return true;
            if (tryTankPrep(139, "Renew", 4, "tank_hot"))
                return true;
            break;

        case CLASS_DRUID:
            if (tryTankPrep(774, "Rejuvenation", 4, "tank_hot"))
                return true;
            if (tryTankPrep(8936, "Regrowth", 4, "tank_hot"))
                return true;
            break;

        case CLASS_SHAMAN:
            if (tryTankPrep(974, "Earth Shield", 8, "tank_shield"))
                return true;
            if (tryTankPrep(61295, "Riptide", 4, "tank_hot"))
                return true;
            break;

        default:
            break;
    }

    return false;
}

bool WorldBotCreatureAI::TryStartPendingPullArm(Unit* target, char const* reason)
{
    if (!me || !target || _pendingPullArm.active)
        return false;

    if (_identity.ambientGroupId == 0 || !IsAmbientGroupLeader())
        return false;

    if (Creature* healer = FindAmbientGroupHealerCreature(AmbientPullReadinessRadius))
    {
        float const healerManaPct = GetUnitManaPct(healer);
        float const requiredManaPct = (me->GetMap() && me->GetMap()->IsDungeon()) ? 70.0f : 50.0f;
        if (healerManaPct < requiredManaPct)
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "pull_wait",
                std::string("reason='healer_mana' healer='") + healer->GetName()
                    + "' healer_mana_pct=" + std::to_string(healerManaPct)
                    + " threshold=" + std::to_string(requiredManaPct)
                    + " mode='" + (me->GetMap() && me->GetMap()->IsDungeon() ? "dungeon" : "world") + "'");
            return true;
        }
    }

    _pendingPullArm.active = true;
    _pendingPullArm.targetGuid = target->GetGUID();
    _pendingPullArm.remainingMs = PullArmLeadTimeMs;
    PullStandoffBand const band = ResolvePullStandoffBand(me, target);
    _pendingPullArm.preferredRange = band.preferredRange;
    _pendingPullArm.lastStandoffMoveWorldMs = 0;
    _pendingPullArm.reason = reason ? reason : "unknown";
    PublishAmbientGroupPullArming();

    float const distance = me->GetDistance(target);
    if (NeedsPullStandoff(distance, band))
    {
        _pendingPullArm.waitingForStandoff = true;
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "pull_standoff",
            std::string("reason='") + _pendingPullArm.reason
                + "' target='" + target->GetName()
                + "' target_guid=" + std::to_string(target->GetGUID().GetCounter())
                + " distance=" + std::to_string(distance)
                + " min_range=" + std::to_string(band.minRange)
                + " max_range=" + std::to_string(band.maxRange)
                + " preferred_range=" + std::to_string(_pendingPullArm.preferredRange));
        return true;
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "pull_arming",
        std::string("reason='") + _pendingPullArm.reason
            + "' target='" + target->GetName()
            + "' target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " arming_ms=" + std::to_string(PullArmLeadTimeMs));
    return true;
}

bool WorldBotCreatureAI::TickPendingPullArm(std::uint32_t diff)
{
    if (!me)
        return false;

    if (_pendingPullArm.active)
    {
        bool const preparedSupport = TryApplyPrePullSupport();
        if (!preparedSupport)
        {
            TryApplyOutOfCombatBuff();
        }

        Unit* target = ObjectAccessor::GetUnit(*me, _pendingPullArm.targetGuid);
        if (!target || !target->IsAlive())
        {
            _pendingPullArm.Reset();
            return false;
        }

        if (_pendingPullArm.waitingForStandoff)
        {
            PullStandoffBand const band = ResolvePullStandoffBand(me, target);
            float const distance = me->GetDistance(target);
            if (!NeedsPullStandoff(distance, band))
            {
                _pendingPullArm.waitingForStandoff = false;
                _pendingPullArm.remainingMs = PullArmLeadTimeMs;
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "pull_arming",
                    std::string("reason='") + _pendingPullArm.reason
                        + "' target='" + target->GetName()
                        + "' target_guid=" + std::to_string(target->GetGUID().GetCounter())
                        + " arming_ms=" + std::to_string(PullArmLeadTimeMs)
                        + " standoff_ready=1"
                        + " distance=" + std::to_string(distance));
                return true;
            }

            std::uint64_t const nowWorldMs = _worldOnlineMs;
            if (nowWorldMs >= (_pendingPullArm.lastStandoffMoveWorldMs + AmbientPullStandoffReissueMs))
            {
                Position dest;
                if (TryComputePullStandoffPoint(me, target, _pendingPullArm.preferredRange, dest))
                {
                    me->GetMotionMaster()->MovePoint(0, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
                    _pendingPullArm.lastStandoffMoveWorldMs = nowWorldMs;
                    if (IsDebugForcedCombatIdentity())
                    {
                        std::ostringstream trace;
                        trace << BuildCombatMovementTraceDetail("pull_standoff_move", target)
                              << " distance=" << distance
                              << " preferred_range=" << _pendingPullArm.preferredRange
                              << " destination=(" << dest.GetPositionX() << "," << dest.GetPositionY() << "," << dest.GetPositionZ() << ")";
                        RecordCombatTrace(trace.str());
                    }
                }
            }
            return true;
        }

        if (_pendingPullArm.remainingMs > diff)
        {
            _pendingPullArm.remainingMs -= diff;
            return true;
        }

        ObjectGuid const targetGuid = _pendingPullArm.targetGuid;
        _pendingPullArm.Reset();

        target = ObjectAccessor::GetUnit(*me, targetGuid);
        if (!target || !target->IsAlive())
            return false;

        if (IsDebugForcedCombatIdentity())
        {
            SuspendCurrentStepForCombat(target);
            me->EngageWithTarget(target);
            target->EngageWithTarget(me);
            me->AddThreat(target, 1.0f);
            target->AddThreat(me, 1.0f);
            EnsureMutualThreatEngagement(me, target);
            AttackStart(target);
            if (target->ToCreature() && target->ToCreature()->IsAIEnabled)
                target->ToCreature()->AI()->AttackStart(me);
            TickCombat(0);
            return true;
        }

        return false;
    }

    if (_identity.ambientGroupId == 0 || IsAmbientGroupLeader())
        return false;

    Creature* leader = FindAmbientGroupLeaderCreature(AmbientPullReadinessRadius);
    if (!leader || !leader->AI())
        return false;

    auto const* leaderAi = static_cast<WorldBotCreatureAI const*>(leader->AI());
    if (!leaderAi->_pendingPullArm.active)
        return false;

    if (!TryApplyPrePullSupport())
        TryApplyOutOfCombatBuff();
    return true;
}

bool WorldBotCreatureAI::TryIssueAmbientGroupTravelFollow(service::AmbientStep const& step)
{
    if (!me || step.type != service::AmbientStepType::Travel)
        return false;

    if (_identity.ambientGroupId == 0 || IsAmbientGroupLeader())
        return false;

    if (me->IsInCombat() || me->GetVictim())
        return false;

    Creature* leader = FindAmbientGroupLeaderCreature(AmbientGroupTravelFollowRadius);
    if (!leader || !leader->IsAlive() || leader->GetMapId() != me->GetMapId())
        return false;

    auto const* leaderAi = static_cast<WorldBotCreatureAI const*>(leader->AI());
    if (!leaderAi)
        return false;

    float const leaderDistance = me->GetDistance(leader);
    if (leaderDistance > AmbientGroupTravelCatchupDistance)
        return false;

    bool const pullStage = leaderAi->_pendingPullArm.active;

    model::BotGlobalConfig const cfg = GetWorldBotGlobalConfigService().Get();
    CompanionFollowFormationResult const formation = ResolveCompanionFollowFormation(
        { cfg.followFormation,
          ResolveAmbientGroupFollowBaseDistance(pullStage),
          cfg.followSlotCount,
          me->GetGUID().GetCounter(),
          CollectAmbientGroupFollowerGuids(AmbientGroupTravelFollowRadius) });

    bool const reissuingFollow =
        me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE;

    me->GetMotionMaster()->MoveFollow(leader, formation.distance, formation.angle);

    if (reissuingFollow)
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "travel_formation",
            std::string("leader='") + leader->GetName()
                + "' leader_identity=" + std::to_string(_identity.ambientGroupLeaderIdentityId)
                + " role='" + _identity.ambientGroupRole
                + "' slot=" + std::to_string(formation.slot)
                + " roster_index=" + std::to_string(formation.rosterIndex)
                + " distance=" + std::to_string(formation.distance)
                + " angle=" + std::to_string(formation.angle)
                + " pull_stage=" + std::to_string(pullStage ? 1 : 0));
    }

    return true;
}

bool WorldBotCreatureAI::MoveToActiveTravelTarget(service::AmbientStep const& step)
{
    if (TryIssueAmbientGroupTravelFollow(step))
        return true;

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

    StrictTravelPathCheckResult const pathCheck =
        EvaluateStrictGroundTravelPath(me, targetX, targetY, targetZ);
    if (!IsStrictGroundTravelPathAccepted(pathCheck))
    {
        bool const localPolicy =
            _activeTravelNavigationPolicy == TravelNavigationPolicy::LocalOnly
            || _activeTravelNavigationPolicy == TravelNavigationPolicy::LocalWithPoiConnector
            || _activeTravelNavigationPolicy == TravelNavigationPolicy::LocalWithAssist;

        if (localPolicy && TryActivateLocalPoiConnectorFallback(step))
            return MoveToActiveTravelTarget(step);

        if (localPolicy && !_localHelperFallbackTried)
        {
            _localHelperFallbackTried = true;

            service::WorldBotResolvedTravelPlan assistPlan;
            if (TryBuildLocalAssistTravelPlan(step, assistPlan))
            {
                _activeTravelNavigationPolicy = TravelNavigationPolicy::LocalWithAssist;
                _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
                ActivateRouteTravelPlan(assistPlan);

                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "travel_assist",
                    BuildTravelNarrative(
                        _session,
                        step,
                        "falling back to assist route '"
                            + (_routeTravelPlan.waypoints.empty()
                                ? std::string("unknown")
                                : _routeTravelPlan.waypoints.front().routeKey)
                            + "'"));

                return MoveToActiveTravelTarget(step);
            }
        }

        if (localPolicy && !_localMacroFallbackTried && _session.sourceKind != "debug_path_scout")
        {
            _localMacroFallbackTried = true;
            _activeTravelNavigationPolicy = TravelNavigationPolicy::MacroTravel;

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
                            "local fallbacks exhausted -> " + DescribeTravelOptionChoice(travelOption)));

                    return MoveToActiveTravelTarget(step);
                }

                if (travelOption.groundPlan.has_value() && !travelOption.groundPlan->empty())
                {
                    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                    ActivateRouteTravelPlan(*travelOption.groundPlan);

                    integration::BotActivityLog::Record(
                        me, _identity.name, _identity.id,
                        "travel_option",
                        BuildTravelNarrative(
                            _session,
                            step,
                            "local fallbacks exhausted -> " + DescribeTravelOptionChoice(travelOption)));

                    return MoveToActiveTravelTarget(step);
                }
            }

            service::WorldBotResolvedTravelPlan routePlan;
            if (TryBuildRouteTravelPlan(step, routePlan))
            {
                _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
                ActivateRouteTravelPlan(routePlan);

                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "travel_option",
                    BuildTravelNarrative(
                        _session,
                        step,
                        "local fallbacks exhausted -> nearest node fallback"));

                return MoveToActiveTravelTarget(step);
            }
        }

        char const* abortReason =
            localPolicy
                ? "no discoverable path found after local fallbacks"
                : (_routeTravelPlanActive ? "route waypoint move rejected" : "direct move rejected");

        AbortCurrentTravelForNoPath(
            step,
            targetX,
            targetY,
            targetZ,
            pathCheck.calculateResult,
            pathCheck.pointCount,
            pathCheck.pathLengthYards,
            static_cast<std::uint32_t>(pathCheck.pathType),
            abortReason);
        return false;
    }

    me->GetMotionMaster()->MovePoint(
        static_cast<uint32>(_currentStep),
        targetX,
        targetY,
        targetZ);
    return true;
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
    if (_debugScoutPathActive)
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
    if (!MoveToActiveTravelTarget(step))
        return false;

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

void WorldBotCreatureAI::AbortCurrentTravelForNoPath(
    service::AmbientStep const& step,
    float targetX,
    float targetY,
    float targetZ,
    bool calculateResult,
    std::size_t pointCount,
    float pathLengthYards,
    std::uint32_t pathTypeBits,
    char const* reason)
{
    if (!me || _sessionDone)
        return;

    me->StopMoving();
    me->GetMotionMaster()->Clear();
    me->Yell("SOS! No path found. Aborting!", LANG_UNIVERSAL);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "travel_no_path",
        BuildTravelNarrative(
            _session,
            step,
            std::string(reason ? reason : "travel rejected")
                + " nav_policy=" + DescribeTravelNavigationPolicy(_activeTravelNavigationPolicy)
                + " start=(" + std::to_string(me->GetPositionX())
                + "," + std::to_string(me->GetPositionY())
                + "," + std::to_string(me->GetPositionZ()) + ")"
                + " dest=(" + std::to_string(targetX)
                + "," + std::to_string(targetY)
                + "," + std::to_string(targetZ) + ")"
                + " result=" + std::to_string(calculateResult ? 1 : 0)
                + " flags='" + DescribePathTypeFlags(static_cast<PathType>(pathTypeBits)) + "'"
                + " points=" + std::to_string(pointCount)
                + " length_yd=" + std::to_string(pathLengthYards)));

    if (_session.sourceKind == "debug_route_harness")
    {
        _traveling = false;
        _activeTravelStepStartKnown = false;
        ResetLocalTravelFallbackState();
        ClearVisibleTravelMode();
        ClearActiveRouteTravelPlan();
        ClearActiveTaxiTravel();
        ResetTravelWatchdog(_travelWatchdog);

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "travel_skip",
            "debug harness skipping failed travel leg");

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "status_change",
            "Skipping failed debug travel leg -> beginning "
                + DescribeNextTask(_session, _currentStep + 1));

        AdvanceStep();
        return;
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "session_abort",
        "travel_no_path -> returning to ledger");

    _traveling = false;
    _activeTravelStepStartKnown = false;
    _sessionDone = true;
    ResetLocalTravelFallbackState();
    ClearVisibleTravelMode();
    ClearActiveRouteTravelPlan();
    ClearActiveTaxiTravel();
    ResetTravelWatchdog(_travelWatchdog);

    SessionCompletionMetadata const completionMetadata =
        BuildSessionCompletionMetadata(_session, _currentStep);
    RuntimeLedgerBreadcrumbs const breadcrumbs =
        BuildRuntimeLedgerBreadcrumbs(_session, _currentStep, _activityTimer);
    GetIdentityRepo().CompleteWorldSession(
        _identity.id,
        me->GetZoneId(),
        _worldOnlineMs,
        completionMetadata.sourceKind,
        completionMetadata.sourceKey,
        completionMetadata.taskFamily,
        completionMetadata.targetZoneId,
        breadcrumbs.taskActivityKey,
        breadcrumbs.questHubKey,
        breadcrumbs.questHubElapsedMs);
    me->DespawnOrUnsummon(Milliseconds(1000));
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

    if (_visibleTravelModeSpellId != 0)
        me->RemoveAurasDueToSpell(_visibleTravelModeSpellId);

    me->SetSpeed(MOVE_RUN, 1.0f, true);

    _visibleTravelModeActive = false;
    _visibleTravelModeSpellId = 0;
    _visibleTravelCapabilityTier = service::WorldBotTravelCapabilityTier::Foot;
    _visibleTravelSpeedRate = 1.0f;
}

void WorldBotCreatureAI::JustEngagedWith(Unit* who)
{
    SuspendCurrentStepForCombat(who);
    PublishAmbientGroupPullCommitted();
    PublishAmbientGroupPrimaryTarget(who);
    PublishAmbientGroupTankAnchor(who);
    SyncControlledGuardianPetAssist(who);
}

void WorldBotCreatureAI::JustReachedHome()
{
    // For materialized bots, reaching home is only a signal that combat may
    // have settled. The bot should request its next assignment on its own tick
    // once the all-clear window and readiness checks pass.
    if (_combatInterrupt.active && !me->IsInCombat() && !me->GetVictim())
        return;
}

void WorldBotCreatureAI::ApplyNamedDebugRunShell()
{
    if (!me)
        return;

    bool const isDebugShellSession =
        _session.sourceKind == "debug_named_run"
        || _session.sourceKind == "debug_route_harness"
        || _session.sourceKind == "debug_path_scout";

    if (!isDebugShellSession)
        return;

    static constexpr std::uint32_t kTaskmasterDisplayId = 17246u; // Caregiver Breel (female draenei)

    me->SetName("Taskmaster");
    me->SetDisplayId(kTaskmasterDisplayId);
    me->SetNativeDisplayId(kTaskmasterDisplayId);
    me->LoadEquipment(0, true);
    for (uint32 slot = 0; slot < MAX_EQUIPMENT_ITEMS; ++slot)
        me->SetVirtualItem(slot, 0);

    me->SetFaction(35u);
    me->SetReactState(REACT_PASSIVE);
    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);
}

void WorldBotCreatureAI::JustRespawned()
{
    ApplyIdentityToCreature();
    _controlledPetAssistTargets.clear();
    _lastControlledPetSummonAttemptWorldMs = 0;
    _lastControlledPetStatusLogWorldMs = 0;
    _ambientFleeState.Reset();
    _ambientPursuitState.Reset();

    if (_identity.ambientGroupId != 0)
        (void)GetAmbientGroupCombatStateService().ClearPeel(
            _identity.ambientGroupId,
            me->GetGUID(),
            ObjectGuid());

    if (_identity.ambientGroupId != 0)
        (void)GetAmbientGroupCombatStateService().ClearPeelAssist(
            _identity.ambientGroupId,
            me->GetGUID());

    if (_distressTracker.active && _identity.ambientGroupId != 0)
        (void)GetAmbientGroupCombatStateService().ClearDistress(
            _identity.ambientGroupId,
            me->GetGUID(),
            _distressTracker.attackerGuid);
    _distressTracker.Reset();

    if (!_pendingCorpseRecovery)
        return;

    _pendingCorpseRecovery = false;
    _combatInterrupt = {};
    _syntheticGlobalCooldownRemainingMs = 0;
    _combatDisengageGraceMs = 0;
    _judgementOfWisdomSnapshot.Reset();
    _lastIncomingDamageSnapshot = {};
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

void WorldBotCreatureAI::JustSummoned(Creature* summon)
{
    InitializeControlledGuardianPet(summon);
}

void WorldBotCreatureAI::SummonedCreatureDespawn(Creature* summon)
{
    if (!summon || !me)
        return;

    if (summon->GetOwnerGUID() != me->GetGUID())
        return;

    _controlledPetAssistTargets.erase(summon->GetGUID());
}

void WorldBotCreatureAI::DamageTaken(
    Unit* attacker,
    uint32& damage,
    DamageEffectType damageType,
    SpellSchoolMask damageSchoolMask)
{
    if (damage == 0)
        return;

    CaptureIncomingDamageSnapshot(attacker, damage, damageType, damageSchoolMask);
    NoteAmbientGroupDistressContact(attacker);

    if (attacker)
        SyncControlledGuardianPetDefend(attacker);
}

void WorldBotCreatureAI::DamageDealt(
    Unit* victim,
    uint32& damage,
    DamageEffectType damageType,
    SpellSchoolMask damageSchoolMask)
{
    CreatureAI::DamageDealt(victim, damage, damageType, damageSchoolMask);

    if (damage == 0)
        return;

    RecordCombatDamageDone(damage);
    TryTriggerWorldBotReactiveGearProcs(victim, damage, damageType, damageSchoolMask);
}

void WorldBotCreatureAI::SpellHit(Unit* caster, SpellInfo const* spellInfo)
{
    if (!caster || !spellInfo || !me)
        return;

    if (me->IsFriendlyTo(caster) || spellInfo->IsPositive())
        return;

    char const* spellName = spellInfo->SpellName[DEFAULT_LOCALE];
    if (!spellName || !*spellName)
        spellName = spellInfo->SpellName[0];

    CaptureIncomingDamageSnapshot(
        caster,
        0,
        SPELL_DIRECT_DAMAGE,
        spellInfo->GetSchoolMask(),
        spellName);

    NoteAmbientGroupDistressContact(caster);
    SyncControlledGuardianPetDefend(caster);
}

void WorldBotCreatureAI::TryTriggerWorldBotReactiveGearProcs(
    Unit* victim,
    std::uint32_t damage,
    DamageEffectType damageType,
    SpellSchoolMask damageSchoolMask)
{
    if (!me || !_preparedBuildReady || !victim || !victim->IsAlive() || victim == me || damage == 0)
        return;

    // Reactive weapon/trinket procs should only be evaluated off landed physical hits.
    if (damageSchoolMask != SPELL_SCHOOL_MASK_NORMAL)
        return;

    if (damageType != DIRECT_DAMAGE && damageType != SPELL_DIRECT_DAMAGE)
        return;

    WeaponAttackType const attackType =
        ResolveWorldBotReactiveProcAttackType(me, victim, _preparedBuild, damageType);
    bool const isWhiteHit = damageType == DIRECT_DAMAGE;
    uint32 const procVictim = PROC_FLAG_TAKEN_DAMAGE;
    uint32 const procEx = PROC_EX_NORMAL_HIT;

    auto const tryCastReactiveProc =
        [&](SpellInfo const* spellInfo,
            float chance,
            std::uint8_t equipmentSlot,
            std::uint32_t itemId,
            std::uint32_t sourceId,
            char const* sourceKind)
        {
            if (!spellInfo || chance <= 0.0f)
                return;

            if (!roll_chance_f(chance))
                return;

            Unit* castTarget = spellInfo->IsPositive() ? me : victim;
            if (!castTarget)
                return;

            if (spellInfo->IsPositive() && castTarget->HasAura(spellInfo->Id, me->GetGUID()))
                return;

            me->CastSpell(
                castTarget,
                spellInfo->Id,
                TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD));

            std::ostringstream oss;
            oss << "source=" << sourceKind
                << " attack=" << DescribeWorldBotReactiveAttackType(attackType)
                << " slot=" << static_cast<std::uint32_t>(equipmentSlot)
                << " item=" << itemId
                << " source_id=" << sourceId
                << " spell=" << spellInfo->Id
                << " chance=" << std::fixed << std::setprecision(2) << chance
                << " target=" << DescribeTraceUnit(castTarget);
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "reactive_gear_proc",
                oss.str());
        };

    for (model::WorldBotAssignedGearEntry const& entry : _preparedBuild.assignedGear)
    {
        if (entry.itemId == 0)
            continue;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
        if (!itemTemplate)
            continue;

        if (!IsWorldBotReactiveProcEligibleForAttackType(entry, itemTemplate, attackType))
            continue;

        if (procVictim & PROC_FLAG_TAKEN_DAMAGE)
        {
            for (std::uint8_t i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                _Spell const& spellData = itemTemplate->Spells[i];
                if (spellData.SpellId <= 0 || spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
                    continue;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(static_cast<std::uint32_t>(spellData.SpellId));
                if (!spellInfo)
                    continue;

                float chance = static_cast<float>(spellInfo->ProcChance);
                if (spellData.SpellPPMRate > 0.0f)
                    chance = me->GetPPMProcChance(me->GetAttackTime(attackType), spellData.SpellPPMRate, spellInfo);
                else if (chance > 100.0f)
                    chance = me->GetWeaponProcChance();

                tryCastReactiveProc(
                    spellInfo,
                    chance,
                    entry.slot,
                    entry.itemId,
                    static_cast<std::uint32_t>(spellData.SpellId),
                    "item_chance_on_hit");
            }
        }

        if (entry.enchantments.empty())
            continue;

        std::vector<std::uint32_t> const values = ParseWorldBotEnchantmentValues(entry.enchantments);
        if (values.empty())
            continue;

        for (std::size_t base = 0; base < values.size(); base += MAX_ENCHANTMENT_OFFSET)
        {
            std::uint32_t const enchantId = values[base];
            if (enchantId == 0)
                continue;

            SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!enchantEntry)
                continue;

            SpellEnchantProcEntry const* procEntry = sSpellMgr->GetSpellEnchantProcEvent(enchantId);
            if (procEntry && procEntry->procEx && (procEntry->procEx & procEx) == 0)
                continue;

            for (std::size_t effectIndex = 0;
                 effectIndex < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS;
                 ++effectIndex)
            {
                if (enchantEntry->type[effectIndex] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                    continue;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(enchantEntry->spellid[effectIndex]);
                if (!spellInfo)
                    continue;

                if (procEntry
                    && (procEntry->attributeMask & ENCHANT_PROC_ATTR_WHITE_HIT)
                    && !isWhiteHit)
                {
                    continue;
                }

                if (procEntry
                    && (procEntry->attributeMask & ENCHANT_PROC_ATTR_EXCLUSIVE) != 0)
                {
                    Unit* checkTarget = spellInfo->IsPositive() ? me : victim;
                    if (checkTarget && checkTarget->HasAura(spellInfo->Id, me->GetGUID()))
                        continue;
                }

                float chance = enchantEntry->amount[effectIndex] != 0
                    ? static_cast<float>(enchantEntry->amount[effectIndex])
                    : me->GetWeaponProcChance();

                if (procEntry)
                {
                    if (procEntry->PPMChance > 0.0f)
                        chance = me->GetPPMProcChance(me->GetAttackTime(attackType), procEntry->PPMChance, spellInfo);
                    else if (procEntry->customChance != 0)
                        chance = static_cast<float>(procEntry->customChance);
                }

                tryCastReactiveProc(
                    spellInfo,
                    chance,
                    entry.slot,
                    entry.itemId,
                    enchantId,
                    "enchant_combat_spell");
            }
        }
    }
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
    _gatherRouteState.Reset();
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
        _preparedBuild.contextKey,
        _preparedBuild.requestedLoadoutKey);
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

model::BotCombatMode WorldBotCreatureAI::ResolveDebugForcedBotMode() const
{
    if (_identity.id == 0)
        return model::BotCombatMode::Assist;

    std::uint32_t const modeIdentityId =
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceBotModeIdentityId", 0);
    if (modeIdentityId == 0 || modeIdentityId != _identity.id)
        return model::BotCombatMode::Assist;

    std::string mode = sConfigMgr->GetOption<std::string>("LivingWorld.DebugForceBotMode", "");
    std::transform(
        mode.begin(),
        mode.end(),
        mode.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (mode == "passive")
        return model::BotCombatMode::Passive;
    if (mode == "hold" || mode == "stay")
        return model::BotCombatMode::Hold;
    if (mode == "guard")
        return model::BotCombatMode::Guard;
    return model::BotCombatMode::Assist;
}

namespace
{

std::string DescribeTraceUnit(Unit const* unit);
std::string DescribeThreatStateForTrace(Unit* actor, Unit* currentVictim);
std::string DescribeAttackValidityForTrace(Creature* actor, Unit* target);

std::string DescribeThreatStateForTrace(Unit* actor, Unit* currentVictim)
{
    std::ostringstream oss;
    Unit* threatVictim = actor && actor->CanHaveThreatList() ? actor->GetThreatMgr().GetCurrentVictim() : nullptr;
    oss << "actor_in_combat=" << ((actor && actor->IsInCombat()) ? 1 : 0) << " "
        << "actor_is_engaged=" << ((actor && actor->IsEngaged()) ? 1 : 0) << " "
        << "actor_alive=" << ((actor && actor->IsAlive()) ? 1 : 0) << " "
        << "actor_evade=" << ((actor && actor->ToCreature() && actor->ToCreature()->IsInEvadeMode()) ? 1 : 0) << " "
        << "actor_hp_pct=" << (actor ? actor->GetHealthPct() : 0.0f) << " "
        << "actor_victim=" << DescribeTraceUnit(currentVictim) << " "
        << "actor_threat_victim=" << DescribeTraceUnit(threatVictim) << " "
        << "actor_threat_list_size=" << ((actor && actor->CanHaveThreatList()) ? actor->GetThreatMgr().GetThreatListSize() : 0);
    return oss.str();
}

std::string DescribeAttackValidityForTrace(Creature* actor, Unit* target)
{
    std::ostringstream oss;
    if (!actor || !target)
    {
        oss << "tgt=0 fail='no_target'";
        return oss.str();
    }

    float leashRadius = sWorld->getFloatConfig(CONFIG_CREATURE_LEASH_RADIUS);
    Position const& home = actor->GetHomePosition();
    float homeDist2d = actor->GetDistance2d(home.GetPositionX(), home.GetPositionY());
    float targetDist2d = actor->GetExactDist2d(target);
    float visibility = std::max<float>(actor->GetVisibilityRange(), target->GetVisibilityRange());
    bool targetable = target->isTargetableForAttack(false, actor);
    bool acceptable = actor->_IsTargetAcceptable(target);
    bool accessible = target->isInAccessiblePlaceFor(actor);
    bool aiAttack = !actor->IsAIEnabled || actor->AI()->CanAIAttack(target);
    bool actorEvade = actor->IsInEvadeMode();
    bool targetEvade = target->IsCreature() && target->ToCreature()->IsInEvadeMode();
    bool canSee = actor->CanSeeOrDetect(target);
    bool hostile = actor->IsHostileTo(target);
    bool engagedBy = actor->IsEngagedBy(target);
    bool recentLeashExtension =
        !actor->isWorldBoss()
        && (actor->GetLastLeashExtensionTime() + actor->GetLeashTimer() > GameTime::GetGameTime().count()
            || actor->HasTauntAura());
    bool withinVisibility = actor->GetMap()->IsDungeon() || actor->GetCharmerOrOwnerGUID().IsPlayer() || actor->IsWithinDist(target, visibility);
    bool leashSatisfied = true;
    if (!actor->GetMap()->IsDungeon() && !actor->GetCharmerOrOwnerGUID().IsPlayer() && leashRadius > 0.0f && !actor->isWorldBoss() && !recentLeashExtension)
        leashSatisfied = actor->IsInDist2d(&home, leashRadius);

    bool targetValid = actor->IsValidAttackTarget(target);
    bool canAttack = actor->CanCreatureAttack(target);
    char const* fail = "ok";
    if (!targetValid)
        fail = "invalid";
    else if (!targetable)
        fail = "untargetable";
    else if (!acceptable)
        fail = "unacceptable";
    else if (!accessible)
        fail = "inaccessible";
    else if (!aiAttack)
        fail = "ai";
    else if (actorEvade)
        fail = "self_evade";
    else if (targetEvade)
        fail = "target_evade";
    else if (!canSee || !withinVisibility)
        fail = "visibility";
    else if (!leashSatisfied)
        fail = "leash";
    else if (!canAttack)
        fail = "other";

    oss << "tgt=" << (targetValid ? 1 : 0) << " "
        << "acc=" << (acceptable ? 1 : 0) << " "
        << "atk=" << (canAttack ? 1 : 0) << " "
        << "fail='" << fail << "' "
        << "dist=" << targetDist2d << " "
        << "home=" << homeDist2d << " "
        << "leash=" << leashRadius << " "
        << "recent=" << (recentLeashExtension ? 1 : 0) << " "
        << "see=" << (canSee ? 1 : 0) << " "
        << "host=" << (hostile ? 1 : 0) << " "
        << "engaged=" << (engagedBy ? 1 : 0) << " "
        << "access=" << (accessible ? 1 : 0) << " "
        << "ai=" << (aiAttack ? 1 : 0) << " "
        << "vis=" << visibility;
    return oss.str();
}

void EnsureMutualThreatEngagement(Creature* attacker, Unit* victim)
{
    if (!attacker || !victim)
        return;

    if (attacker->IsAlive() && victim->IsAlive())
    {
        attacker->SetInCombatWith(victim);
        victim->SetInCombatWith(attacker);
    }

    if (attacker->IsAIEnabled)
        attacker->AI()->JustStartedThreateningMe(victim);

    if (Creature* victimCreature = victim->ToCreature())
    {
        if (victimCreature->IsAIEnabled)
            victimCreature->AI()->JustStartedThreateningMe(attacker);
    }
}

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

        me->EngageWithTarget(creature);
        creature->EngageWithTarget(me);
        me->AddThreat(creature, 1.0f);
        creature->AddThreat(me, 1.0f);
        EnsureMutualThreatEngagement(me, creature);
        if (me->IsAIEnabled)
            me->AI()->AttackStart(creature);

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

    std::uint32_t const forceDelayMs =
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceCombatDelayMs", 0);
    if (_worldOnlineMs < forceDelayMs)
        return;

    if (!CanInterruptCurrentStepForCombat())
    {
        RecordCombatTrace("phase='debug' decision='force_target_scan' result='travel_not_interruptible'");
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

    auto const maybeRepositionForSaferPull =
        [&]() -> bool
        {
            Creature* const targetCreature = target->ToCreature();
            if (!me || !targetCreature || !me->GetMap())
                return false;

            if (!CreatureHasKnownSpell(targetCreature, MechanoKickSpellId))
                return false;

            float const currentAngle = target->GetAngle(me);
            TerrainFootingSample const current = EvaluateCombatFootingAt(
                me,
                target,
                me->GetPositionX(),
                me->GetPositionY(),
                currentAngle);
            if (!current.valid)
                return false;

            float const preferredRange = std::max(18.0f, me->GetDistance(target));
            TerrainSurveyCandidate best;
            TerrainSurveyDiagnostics diagnostics;
            bool reusedCache = false;
            if (CanReuseTerrainSurveyCache(
                    _terrainSurveyCache,
                    me,
                    target,
                    preferredRange,
                    true,
                    TerrainSurveyRadiusYards))
            {
                best = MakeTerrainSurveyCandidateFromCache(_terrainSurveyCache);
                reusedCache = true;
            }
            else
            {
                best = FindBestCombatFootingSurveyAroundTarget(
                    me,
                    target,
                    preferredRange,
                    true,
                    TerrainSurveyRadiusYards,
                    { 0.0f, 4.0f, 8.0f, 12.0f },
                    { Pi, Pi + QuarterPi, Pi - QuarterPi, Pi + HalfPi, Pi - HalfPi, 0.0f },
                    &diagnostics);
                if (!best.valid)
                {
                    best = FindBestCombatFootingSurveyNearFight(
                        me,
                        target,
                        true,
                        TerrainSurveyRadiusYards,
                        &diagnostics);
                }
                StoreTerrainSurveyCache(
                    _terrainSurveyCache,
                    me,
                    target,
                    preferredRange,
                    true,
                    TerrainSurveyRadiusYards,
                    best);
            }

            float const currentScore = ScoreTerrainSurveyCandidate(current, true, 0.0f, 0.0f);
            float const scoreGain = currentScore - best.score;
            if (!best.valid || scoreGain < 0.35f)
            {
                if (IsDebugForcedCombatIdentity())
                {
                    std::ostringstream skip;
                    skip << BuildCombatMovementTraceDetail("terrain_survey_skip", target)
                         << " stage='pre_pull' reason='" << (!best.valid ? "no_candidate" : "insufficient_gain") << "'"
                         << " reused_cache=" << (reusedCache ? 1 : 0)
                         << DescribeTerrainDiagnostics(diagnostics)
                         << " current_back_drop=" << current.backDrop
                         << " current_side_drop=" << current.sideDrop
                         << " current_rear_support=" << current.rearSupportDistance
                         << " current_score=" << currentScore;
                    if (best.valid)
                    {
                        skip << " best_back_drop=" << best.backDrop
                             << " best_side_drop=" << best.sideDrop
                             << " best_rear_support=" << best.rearSupportDistance
                             << " best_score=" << best.score
                             << " score_gain=" << scoreGain;
                    }
                    RecordCombatTrace(skip.str());
                }
                return false;
            }

            me->GetMotionMaster()->MovePoint(0, best.x, best.y, best.z);

            std::ostringstream oss;
            oss << BuildCombatMovementTraceDetail("pre_pull_reposition", target)
                << " current_back_drop=" << current.backDrop
                << " current_side_drop=" << current.sideDrop
                << " current_rear_support=" << current.rearSupportDistance
                << " best_back_drop=" << best.backDrop
                << " best_side_drop=" << best.sideDrop
                << " best_rear_support=" << best.rearSupportDistance
                << " orbit_radius=" << best.orbitRadius
                << " angle_offset=" << best.orbitOffset
                << " reused_cache=" << (reusedCache ? 1 : 0)
                << " current_score=" << currentScore
                << " best_score=" << best.score
                << " destination=(" << best.x << "," << best.y << "," << best.z << ")";
            RecordCombatTrace(oss.str());
            return true;
        };

    if (maybeRepositionForSaferPull())
        return;

    if (TryStartPendingPullArm(target, "debug_forced_combat"))
        return;

    if (_identity.ambientGroupId != 0 && !IsAmbientGroupLeader())
    {
        if (Creature* leader = FindAmbientGroupLeaderCreature(AmbientPullReadinessRadius))
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "pull_delegate",
                std::string("reason='debug_forced_combat' leader='") + leader->GetName()
                    + "' leader_guid=" + std::to_string(leader->GetGUID().GetCounter())
                    + " target='" + target->GetName()
                    + "' target_guid=" + std::to_string(target->GetGUID().GetCounter()));
            return;
        }
    }

    SuspendCurrentStepForCombat(target);
    me->EngageWithTarget(target);
    target->EngageWithTarget(me);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    EnsureMutualThreatEngagement(me, target);
    AttackStart(target);

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

    if (IsDebugForcedCombatIdentity())
    {
        std::ostringstream stateTrace;
        stateTrace << "phase='debug' decision='force_target_state' "
                   << "bot_" << DescribeThreatStateForTrace(me, me->GetVictim()) << " "
                   << "target_" << DescribeThreatStateForTrace(target, target->GetVictim());
        RecordCombatTrace(stateTrace.str());
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

    if (!CanInterruptCurrentStepForCombat())
        return;

    _combatInterrupt.active = true;
    _combatInterrupt.reason =
        (_currentStep < _session.steps.size() && IsCombatAreaStep(_session.steps[_currentStep]))
            ? CombatInterruptionReason::AuthoredGrind
            : CombatInterruptionReason::ReactiveDefense;
    _combatInterrupt.suspendedStepIndex = _currentStep;
    _combatInterrupt.allClearElapsedMs = 0;
    _combatInterrupt.allClearRequiredMs = ResolveCombatResumeDelayMs();
    _combatInterrupt.resumePending = false;
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

    if (_identity.ambientGroupId != 0)
    {
        std::vector<Unit*> friendlyAllies = CollectNearbyFriendlyAmbientWorldBots(me, AmbientCombatAssistRadius, false);
        for (Unit* ally : friendlyAllies)
        {
            if (!IsAmbientGroupedWith(ally))
                continue;

            if (!(ally->IsInCombat() || ally->GetVictim() || !ally->getAttackers().empty()))
                continue;

            if (TrySustainAmbientCombat("group_leader_still_engaged"))
                return;

            break;
        }
    }

    RecordCombatSummary("combat_exit");
    _combatInterrupt = {};
    _combatDisengageGraceMs = 0;
    _traveling = false;
    ClearVisibleTravelMode();
    ClearActiveRouteTravelPlan();
    ClearActiveTaxiTravel();
    _judgementOfWisdomSnapshot.Reset();
    _groupCombatHandoffSnapshot.Reset();
    _debugJudgementAuraObservation.Reset();
    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    _lastDebugCombatManaDrainWorldMs = 0;
    _debugCombatManaGemObserved = false;
    _lastIncomingDamageSnapshot = {};
    _usedSimulatedItemsThisCombat.clear();
    ResetTravelWatchdog(_travelWatchdog);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_exit",
        "resume_step='" + DescribeCurrentStep() + "'");
}

bool WorldBotCreatureAI::TryRequestSuspendedStepResume()
{
    if (!_combatInterrupt.active || !_combatInterrupt.resumePending || !me)
        return false;

    if (me->IsInCombat() || me->GetVictim() || me->IsEngaged() || !me->getAttackers().empty())
        return false;

    if (_pendingPullArm.active)
        return false;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "resume_request",
        "step='" + DescribeCurrentStep()
            + "' reason='bot_ready_for_next_assignment'");

    ResumeSuspendedStepAfterCombat();
    return true;
}

void WorldBotCreatureAI::ResetCombatMetricsSegment()
{
    _combatMetricsCurrent = {};
}

void WorldBotCreatureAI::CaptureIncomingDamageSnapshot(
    Unit* attacker,
    std::uint32_t damage,
    DamageEffectType damageType,
    SpellSchoolMask schoolMask,
    char const* moveName)
{
    if (!attacker)
        return;

    std::uint32_t const nowMs = GameTime::GetGameTimeMS().count();
    bool const hasExplicitMoveName = moveName && *moveName;
    bool const sameSourceRecentSpell =
        _lastIncomingDamageSnapshot.sourceGuid == attacker->GetGUID()
        && !_lastIncomingDamageSnapshot.moveName.empty()
        && _lastIncomingDamageSnapshot.amount == 0
        && (nowMs - _lastIncomingDamageSnapshot.capturedAtMs) <= 1500;

    _lastIncomingDamageSnapshot.sourceGuid = attacker->GetGUID();
    _lastIncomingDamageSnapshot.sourceName = attacker->GetName();
    _lastIncomingDamageSnapshot.amount = damage;
    _lastIncomingDamageSnapshot.schoolMask = schoolMask;
    _lastIncomingDamageSnapshot.damageType = damageType;
    _lastIncomingDamageSnapshot.capturedAtMs = nowMs;

    if (hasExplicitMoveName)
        _lastIncomingDamageSnapshot.moveName = moveName;
    else if (!sameSourceRecentSpell)
        _lastIncomingDamageSnapshot.moveName.clear();
}

std::string WorldBotCreatureAI::BuildCombatSummaryReason(char const* fallbackReason, Unit* killer) const
{
    if (_lastIncomingDamageSnapshot.valid())
    {
        std::ostringstream oss;
        oss << "move='"
            << (!_lastIncomingDamageSnapshot.moveName.empty()
                    ? _lastIncomingDamageSnapshot.moveName
                    : DescribeDamageTypeForTrace(_lastIncomingDamageSnapshot.damageType))
            << "' amount=" << _lastIncomingDamageSnapshot.amount
            << " source='" << _lastIncomingDamageSnapshot.sourceName << "'"
            << " source_guid=" << _lastIncomingDamageSnapshot.sourceGuid.GetCounter();
        return oss.str();
    }

    if (killer)
    {
        std::ostringstream oss;
        oss << "source='" << killer->GetName() << "'"
            << " source_guid=" << killer->GetGUID().GetCounter();
        if (fallbackReason && *fallbackReason)
            oss << " reason='" << fallbackReason << "'";
        return oss.str();
    }

    return std::string(fallbackReason && *fallbackReason ? fallbackReason : "unknown");
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

Unit* WorldBotCreatureAI::FindNearbyCreatureCombatTarget(float radius) const
{
    if (!me)
        return nullptr;

    auto const isViableCreatureTarget =
        [&](Unit* candidate) -> bool
        {
            if (!candidate || !candidate->IsAlive() || candidate == me)
                return false;
            if (me->IsFriendlyTo(candidate) || !candidate->isTargetableForAttack(false, me))
                return false;

            Creature* creature = candidate->ToCreature();
            if (!creature)
                return false;

            return creature->GetEntry() != WorldBotEntry;
        };

    std::vector<Unit*> groupedAllies;
    groupedAllies.push_back(me);
    for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(me, radius, false))
    {
        if (IsAmbientGroupedWith(ally))
            groupedAllies.push_back(ally);
    }

    std::vector<Unit*> candidates;
    for (Unit* ally : groupedAllies)
    {
        if (!ally)
            continue;

        for (Unit* attacker : ally->getAttackers())
        {
            if (isViableCreatureTarget(attacker))
                candidates.push_back(attacker);
        }

        if (Unit* victim = ally->GetVictim())
        {
            if (isViableCreatureTarget(victim))
                candidates.push_back(victim);
        }
    }

    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(me, me, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, candidates, check);
    Cell::VisitObjects(me, searcher, radius);

    candidates.erase(
        std::remove_if(
            candidates.begin(),
            candidates.end(),
            [&](Unit* candidate)
            {
                if (!isViableCreatureTarget(candidate))
                    return true;
                return !candidate->IsInCombat()
                    && !candidate->GetVictim()
                    && candidate->getAttackers().empty();
            }),
        candidates.end());

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    Unit* best = nullptr;
    float bestScore = std::numeric_limits<float>::lowest();
    for (Unit* candidate : candidates)
    {
        float score = 0.0f;
        float const distance = me->GetDistance(candidate);
        score -= distance * 4.0f;
        score += std::clamp(100.0f - candidate->GetHealthPct(), 0.0f, 100.0f);

        if (candidate == me->GetVictim())
            score += 120.0f;

        Unit* victim = candidate->GetVictim();
        if (victim)
        {
            if (victim == me)
                score += 250.0f;
            else if (std::find(groupedAllies.begin(), groupedAllies.end(), victim) != groupedAllies.end())
                score += 180.0f;
        }

        if (!best || score > bestScore)
        {
            best = candidate;
            bestScore = score;
        }
    }

    return best;
}

WorldBotCreatureAI::GroupCombatTargetReply WorldBotCreatureAI::RequestGroupedCombatTarget(float radius) const
{
    if (!me || _identity.ambientGroupId == 0)
        return {};

    std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();

    auto const isViableTarget =
        [&](Unit* candidate) -> bool
        {
            if (!candidate || candidate == me || !candidate->IsAlive() || !candidate->IsInWorld())
                return false;
            if (me->IsFriendlyTo(candidate) || !candidate->isTargetableForAttack(false, me))
                return false;
            return true;
        };

    std::vector<Unit*> groupedAllies = CollectNearbyFriendlyAmbientWorldBots(me, radius, false);
    groupedAllies.erase(
        std::remove_if(
            groupedAllies.begin(),
            groupedAllies.end(),
            [&](Unit* ally)
            {
                return !IsAmbientGroupedWith(ally);
            }),
        groupedAllies.end());
    groupedAllies.push_back(me);

    service::AmbientGroupCombatSnapshot const sharedSnapshot =
        GetAmbientGroupCombatStateService().Get(_identity.ambientGroupId, nowMs);
    bool const healerDistressWithinWrangle =
        IsHealerDistressWithinWrangleWindow(sharedSnapshot, nowMs);
    bool const peelClaimedByMe = TryClaimAmbientGroupPeelTarget(sharedSnapshot, nowMs);
    bool const peelAssistClaimedByMe = TryClaimAmbientGroupPeelAssistTarget(sharedSnapshot, nowMs);

    Unit* best = nullptr;
    float bestScore = std::numeric_limits<float>::lowest();
    char const* bestSource = "none";
    auto const consider =
        [&](Unit* candidate, Unit* ownerAlly, float baseScore, char const* source)
        {
            if (!isViableTarget(candidate))
                return;

            float score = baseScore;
            score -= me->GetDistance(candidate) * 3.0f;

            if (ownerAlly)
            {
                score += 40.0f;
                if (ownerAlly == me)
                    score += 20.0f;

                if (Creature* allyCreature = ownerAlly->ToCreature())
                {
                    if (allyCreature->GetEntry() == WorldBotEntry && allyCreature->AI())
                    {
                        auto const* allyAi = static_cast<WorldBotCreatureAI const*>(allyCreature->AI());
                        if (_identity.ambientGroupLeaderIdentityId != 0
                            && allyAi->_identity.id == _identity.ambientGroupLeaderIdentityId)
                        {
                            score += 200.0f;
                        }
                    }
                }

                if (candidate->GetVictim() == ownerAlly)
                    score += 120.0f;
            }

            if (candidate == me->GetVictim())
                score += 150.0f;

            if (!best || score > bestScore)
            {
                best = candidate;
                bestScore = score;
                bestSource = source ? source : "none";
            }
        };

    auto const considerSharedGuid =
        [&](ObjectGuid const& guid, float baseScore, char const* source)
        {
            if (guid.IsEmpty())
                return;

            Unit* candidate = ObjectAccessor::GetUnit(*me, guid);
            consider(candidate, nullptr, baseScore, source);
        };

    if (IsAmbientTankRole()
        && !sharedSnapshot.distressTargetGuid.IsEmpty()
        && sharedSnapshot.distressTier >= service::AmbientGroupDistressTier::AssistRequested)
    {
        bool const peelOwnedByOther =
            !sharedSnapshot.peelClaimedByGuid.IsEmpty()
            && sharedSnapshot.peelClaimedByGuid != me->GetGUID()
            && sharedSnapshot.peelTargetGuid == sharedSnapshot.distressTargetGuid;
        bool tankShouldRecoverPeel = false;
        if (peelOwnedByOther
            && sharedSnapshot.peelTargetGuid == sharedSnapshot.distressTargetGuid
            && sharedSnapshot.distressedAllyGuid == sharedSnapshot.peelClaimedByGuid
            && sharedSnapshot.distressTier >= service::AmbientGroupDistressTier::Urgent
            && !sharedSnapshot.peelAssistClaimedByGuid.IsEmpty())
        {
            Unit* distressTarget = ObjectAccessor::GetUnit(*me, sharedSnapshot.distressTargetGuid);
            Creature* distressCreature = distressTarget ? distressTarget->ToCreature() : nullptr;
            tankShouldRecoverPeel =
                distressTarget
                && distressTarget->IsAlive()
                && distressTarget->GetHealthPct() > 50.0f
                && (!distressCreature || !distressCreature->isWorldBoss());
        }

        if (!peelOwnedByOther || healerDistressWithinWrangle)
            considerSharedGuid(sharedSnapshot.distressTargetGuid, 520.0f, "distress_target");
        else if (tankShouldRecoverPeel)
            considerSharedGuid(sharedSnapshot.distressTargetGuid, 530.0f, "peel_tank_fallback");
    }

    if (peelClaimedByMe && !sharedSnapshot.distressTargetGuid.IsEmpty())
        considerSharedGuid(sharedSnapshot.distressTargetGuid, 540.0f, "peel_claim");

    if (peelAssistClaimedByMe && !sharedSnapshot.peelTargetGuid.IsEmpty())
        considerSharedGuid(sharedSnapshot.peelTargetGuid, 535.0f, "peel_assist_claim");

    if (!sharedSnapshot.tankAnchorTargetGuid.IsEmpty()
        && !(healerDistressWithinWrangle && !IsAmbientTankRole()))
    {
        float const anchorScore = IsAmbientTankRole() ? 500.0f : 470.0f;
        considerSharedGuid(sharedSnapshot.tankAnchorTargetGuid, anchorScore, "tank_anchor");
    }

    if (!sharedSnapshot.partyPrimaryTargetGuid.IsEmpty())
        considerSharedGuid(sharedSnapshot.partyPrimaryTargetGuid, 430.0f, "party_primary");

    for (Unit* ally : groupedAllies)
    {
        if (!ally)
            continue;

        if (Unit* victim = ally->GetVictim())
        {
            char const* source = "ally_victim";
            if (ally == me)
                source = "self_victim";
            else if (Creature* allyCreature = ally->ToCreature())
            {
                if (allyCreature->GetEntry() == WorldBotEntry && allyCreature->AI())
                {
                    auto const* allyAi = static_cast<WorldBotCreatureAI const*>(allyCreature->AI());
                    if (_identity.ambientGroupLeaderIdentityId != 0
                        && allyAi->_identity.id == _identity.ambientGroupLeaderIdentityId)
                    {
                        source = "leader_victim";
                    }
                }
            }

            consider(victim, ally, 300.0f, source);
        }

        for (Unit* attacker : ally->getAttackers())
        {
            char const* source = "ally_attacker";
            if (ally == me)
                source = "self_attacker";
            else if (Creature* allyCreature = ally->ToCreature())
            {
                if (allyCreature->GetEntry() == WorldBotEntry && allyCreature->AI())
                {
                    auto const* allyAi = static_cast<WorldBotCreatureAI const*>(allyCreature->AI());
                    if (_identity.ambientGroupLeaderIdentityId != 0
                        && allyAi->_identity.id == _identity.ambientGroupLeaderIdentityId)
                    {
                        source = "leader_attacker";
                    }
                }
            }

            consider(attacker, ally, 260.0f, source);
        }
    }

    return { best, bestSource };
}

bool WorldBotCreatureAI::IsAmbientGroupedWith(Unit const* ally) const
{
    if (!me || !ally || ally == me)
        return false;

    Creature const* creature = ally->ToCreature();
    if (!creature || creature->GetEntry() != WorldBotEntry || !creature->AI())
        return false;

    auto const* allyAi = static_cast<WorldBotCreatureAI const*>(creature->AI());
    if (_identity.ambientGroupId == 0 || allyAi->_identity.ambientGroupId == 0)
        return false;

    return _identity.ambientGroupId == allyAi->_identity.ambientGroupId;
}

bool WorldBotCreatureAI::TryAdoptGroupedCombatTarget(char const* reason)
{
    if (!me || !_combatInterrupt.active || _identity.ambientGroupId == 0)
        return false;

    GroupCombatTargetReply const reply = RequestGroupedCombatTarget(AmbientCombatAssistRadius);
    Unit* target = reply.target;
    if (!target)
        return false;
    if (!CanInitiateAmbientCombatAgainstTarget(target, reason))
        return false;

    std::uint32_t const nowMs = GameTime::GetGameTimeMS().count();
    bool const sameTargetRecent =
        !_groupCombatHandoffSnapshot.targetGuid.IsEmpty()
        && _groupCombatHandoffSnapshot.targetGuid == target->GetGUID()
        && (nowMs - _groupCombatHandoffSnapshot.adoptedAtMs) < GroupCombatHandoffRefreshMs;

    _combatDisengageGraceMs = 0;
    _combatInterrupt.allClearElapsedMs = 0;

    if (!sameTargetRecent)
    {
        if (std::string_view(reply.source) == "distress_target"
            || std::string_view(reply.source) == "tank_anchor"
            || std::string_view(reply.source) == "party_primary"
            || std::string_view(reply.source) == "peel_tank_fallback"
            || std::string_view(reply.source) == "peel_assist_claim")
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "group_state_read",
                std::string("request_source='") + reply.source
                    + "' target_guid=" + std::to_string(target->GetGUID().GetCounter())
                    + " target='" + target->GetName() + "'");
        }

        me->EngageWithTarget(target);
        target->EngageWithTarget(me);
        AttackStart(target);
        me->AddThreat(target, 1.0f);
        target->AddThreat(me, 1.0f);
        EnsureMutualThreatEngagement(me, target);
        PublishAmbientGroupPrimaryTarget(target);
        PublishAmbientGroupTankAnchor(target);

        if (target->IsCreature() && target->ToCreature()->IsAIEnabled)
            target->ToCreature()->AI()->AttackStart(me);

        _groupCombatHandoffSnapshot.targetGuid = target->GetGUID();
        _groupCombatHandoffSnapshot.adoptedAtMs = nowMs;

        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "combat_handoff",
            std::string("reason='") + (reason ? reason : "unknown")
                + "' request_source='" + (reply.source ? reply.source : "none")
                + "' target_guid=" + std::to_string(target->GetGUID().GetCounter())
                + " target='" + target->GetName() + "'");

        if (IsDebugForcedCombatIdentity())
        {
            std::ostringstream handoffTrace;
            handoffTrace << "phase='combat' decision='party_target_adopt' "
                         << "reason='" << (reason ? reason : "unknown") << "' "
                         << "request_source='" << (reply.source ? reply.source : "none") << "' "
                         << "target=" << DescribeTraceUnit(target);
            RecordCombatTrace(handoffTrace.str());
            RecordCombatTrace(BuildCombatMovementTraceDetail("party_target_adopt", target));
        }
    }
    else if (!me->IsInCombat() || !me->GetVictim())
    {
        // Keep the grouped target "sticky" for a short grace window so we do
        // not re-announce the same handoff every tick while the rest of the
        // party is still clearly on that target.
        me->EngageWithTarget(target);
        target->EngageWithTarget(me);
        EnsureMutualThreatEngagement(me, target);
    }

    return true;
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

std::uint32_t WorldBotCreatureAI::GetCustomSpellWaitMs(std::uint32_t spellId) const
{
    if (IsJudgementOfWisdomSpell(spellId)
        && _judgementOfWisdomSnapshot.castWorldMs > 0
        && me
        && me->GetVictim()
        && _judgementOfWisdomSnapshot.targetGuid == me->GetVictim()->GetGUID())
    {
        if (_worldOnlineMs < (_judgementOfWisdomSnapshot.castWorldMs + JudgementOfWisdomCooldownMs))
        {
            std::uint64_t const remainingMs64 =
                (_judgementOfWisdomSnapshot.castWorldMs + JudgementOfWisdomCooldownMs) - _worldOnlineMs;
            return static_cast<std::uint32_t>(
                std::min<std::uint64_t>(remainingMs64, std::numeric_limits<std::uint32_t>::max()));
        }
    }

    if (!IsConsecrationSpell(spellId) || !_consecrationSnapshot.active)
        return 0;

    if (_consecrationSnapshot.mapId != me->GetMapId())
        return 0;

    float const movedDistance = me->GetDistance(
        _consecrationSnapshot.x,
        _consecrationSnapshot.y,
        _consecrationSnapshot.z);
    if (movedDistance >= ConsecrationRecenterDistanceYards)
        return 0;

    if (_worldOnlineMs >= (_consecrationSnapshot.castWorldMs + ConsecrationLifetimeMs))
        return 0;

    std::uint64_t const remainingMs64 =
        (_consecrationSnapshot.castWorldMs + ConsecrationLifetimeMs) - _worldOnlineMs;
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(remainingMs64, std::numeric_limits<std::uint32_t>::max()));
}

void WorldBotCreatureAI::NoteSuccessfulSpellCast(std::uint32_t spellId, Unit* target)
{
    if (IsJudgementOfWisdomSpell(spellId))
    {
        _judgementOfWisdomSnapshot.castWorldMs = _worldOnlineMs;
        _judgementOfWisdomSnapshot.targetGuid = target ? target->GetGUID() : ObjectGuid();
    }

    if (!IsConsecrationSpell(spellId))
        return;

    _consecrationSnapshot.active = true;
    _consecrationSnapshot.spellId = spellId;
    _consecrationSnapshot.castWorldMs = _worldOnlineMs;
    _consecrationSnapshot.mapId = me->GetMapId();
    _consecrationSnapshot.x = me->GetPositionX();
    _consecrationSnapshot.y = me->GetPositionY();
    _consecrationSnapshot.z = me->GetPositionZ();
}

bool WorldBotCreatureAI::CanInterruptCurrentStepForCombat() const
{
    if (!_sessionReady || _sessionDone || _currentStep >= _session.steps.size())
        return true;

    service::AmbientStep const& step = _session.steps[_currentStep];
    if (step.type == service::AmbientStepType::Transit)
        return false;

    if (step.type != service::AmbientStepType::Travel)
        return true;

    switch (_activeTravelExecutionPhase)
    {
        case ActiveTravelExecutionPhase::TaxiApproach:
        case ActiveTravelExecutionPhase::TaxiTransit:
        case ActiveTravelExecutionPhase::TaxiFinalLeg:
            return false;
        case ActiveTravelExecutionPhase::GroundOnly:
        case ActiveTravelExecutionPhase::None:
        default:
            break;
    }

    return _activeTransitExecutionPhase == ActiveTransitExecutionPhase::None;
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

    if (TryStartPendingPullArm(target, "grind_pull"))
        return true;

    if (_identity.ambientGroupId != 0 && !IsAmbientGroupLeader())
    {
        if (Creature* leader = FindAmbientGroupLeaderCreature(AmbientPullReadinessRadius))
        {
            integration::BotActivityLog::Record(
                me,
                _identity.name,
                _identity.id,
                "pull_delegate",
                std::string("reason='grind_pull' leader='") + leader->GetName()
                    + "' leader_guid=" + std::to_string(leader->GetGUID().GetCounter())
                    + " target='" + target->GetName()
                    + "' target_guid=" + std::to_string(target->GetGUID().GetCounter()));
            return true;
        }
    }

    SuspendCurrentStepForCombat(target);
    me->EngageWithTarget(target);
    target->EngageWithTarget(me);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    EnsureMutualThreatEngagement(me, target);
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

bool WorldBotCreatureAI::TryJoinNearbyAmbientCombat(char const* reason)
{
    if (!me || me->IsInCombat() || me->GetVictim() || !CanInterruptCurrentStepForCombat())
        return false;

    std::vector<Unit*> friendlyAllies = CollectNearbyFriendlyAmbientWorldBots(me, AmbientCombatAssistRadius, false);
    Unit* preferredTarget = nullptr;
    std::size_t groupedCombatAllyCount = 0;
    for (Unit* ally : friendlyAllies)
    {
        if (!IsAmbientGroupedWith(ally))
            continue;

        if (ally->IsInCombat() || ally->GetVictim() || !ally->getAttackers().empty())
            ++groupedCombatAllyCount;

        Creature* allyCreature = ally->ToCreature();
        if (!allyCreature || allyCreature->GetEntry() != WorldBotEntry || !allyCreature->AI())
            continue;

        auto const* allyAi = static_cast<WorldBotCreatureAI const*>(allyCreature->AI());
        if (_identity.ambientGroupLeaderIdentityId != 0
            && allyAi->_identity.id == _identity.ambientGroupLeaderIdentityId
            && ally->GetVictim()
            && ally->GetVictim()->IsAlive()
            && ally->GetVictim()->IsInWorld()
            && !me->IsFriendlyTo(ally->GetVictim())
            && ally->GetVictim()->isTargetableForAttack(false, me))
        {
            preferredTarget = ally->GetVictim();
        }
    }

    if (groupedCombatAllyCount == 0)
        return false;

    Unit* target = preferredTarget ? preferredTarget : FindNearbyAmbientCombatTarget(AmbientCombatAssistRadius);
    if (!target)
        return false;
    if (!CanInitiateAmbientCombatAgainstTarget(target, reason))
        return false;

    SuspendCurrentStepForCombat(target);
    _combatDisengageGraceMs = 0;
    _combatInterrupt.allClearElapsedMs = 0;
    me->EngageWithTarget(target);
    target->EngageWithTarget(me);
    AttackStart(target);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    EnsureMutualThreatEngagement(me, target);

    PublishAmbientGroupPrimaryTarget(target);
    PublishAmbientGroupTankAnchor(target);

    if (target->IsCreature() && target->ToCreature()->IsAIEnabled)
        target->ToCreature()->AI()->AttackStart(me);

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "combat_assist",
        std::string("reason='") + (reason ? reason : "unknown")
            + "' grouped_allies=" + std::to_string(groupedCombatAllyCount)
            + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " target='" + target->GetName() + "'");

    if (IsDebugForcedCombatIdentity())
    {
        std::ostringstream assistTrace;
        assistTrace << "phase='combat' decision='group_assist' "
                    << "reason='" << (reason ? reason : "unknown") << "' "
                    << "grouped_allies=" << groupedCombatAllyCount << " "
                    << "target=" << DescribeTraceUnit(target);
        RecordCombatTrace(assistTrace.str());
        RecordCombatTrace(BuildCombatMovementTraceDetail("group_assist", target));
    }

    return true;
}

bool WorldBotCreatureAI::TrySustainAmbientCombat(char const* reason)
{
    if (!me || !_combatInterrupt.active)
        return false;

    Unit* target = nullptr;
    if (_identity.ambientGroupLeaderIdentityId != 0)
    {
        std::vector<Unit*> friendlyAllies = CollectNearbyFriendlyAmbientWorldBots(me, AmbientCombatAssistRadius, false);
        for (Unit* ally : friendlyAllies)
        {
            if (!IsAmbientGroupedWith(ally))
                continue;

            Creature* allyCreature = ally->ToCreature();
            if (!allyCreature || allyCreature->GetEntry() != WorldBotEntry || !allyCreature->AI())
                continue;

            auto const* allyAi = static_cast<WorldBotCreatureAI const*>(allyCreature->AI());
            if (allyAi->_identity.id == _identity.ambientGroupLeaderIdentityId
                && ally->GetVictim()
                && ally->GetVictim()->IsAlive()
                && ally->GetVictim()->IsInWorld()
                && !me->IsFriendlyTo(ally->GetVictim())
                && ally->GetVictim()->isTargetableForAttack(false, me))
            {
                target = ally->GetVictim();
                break;
            }
        }
    }

    if (!target)
    {
        for (Unit* attacker : me->getAttackers())
        {
            if (!attacker || !attacker->IsAlive() || !attacker->IsInWorld())
                continue;
            if (me->IsFriendlyTo(attacker) || !attacker->isTargetableForAttack(false, me))
                continue;

            target = attacker;
            break;
        }
    }

    if (!target)
        target = FindNearbyCreatureCombatTarget(AmbientCombatAssistRadius);
    if (!target)
        target = FindNearbyAmbientCombatTarget(AmbientCombatAssistRadius);
    if (!target)
        return false;
    if (std::find(me->getAttackers().begin(), me->getAttackers().end(), target) == me->getAttackers().end()
        && !CanInitiateAmbientCombatAgainstTarget(target, reason))
    {
        return false;
    }

    _combatDisengageGraceMs = 0;
    _combatInterrupt.allClearElapsedMs = 0;
    me->EngageWithTarget(target);
    target->EngageWithTarget(me);
    AttackStart(target);
    me->AddThreat(target, 1.0f);
    target->AddThreat(me, 1.0f);
    EnsureMutualThreatEngagement(me, target);
    PublishAmbientGroupPrimaryTarget(target);
    PublishAmbientGroupTankAnchor(target);

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
    {
        std::ostringstream stateTrace;
        stateTrace << "phase='combat' decision='reassist_state' "
                   << "reason='" << (reason ? reason : "unknown") << "' "
                   << "bot_" << DescribeThreatStateForTrace(me, me->GetVictim()) << " "
                   << "target_" << DescribeThreatStateForTrace(target, target->GetVictim());
        RecordCombatTrace(stateTrace.str());
        RecordCombatTrace(BuildCombatMovementTraceDetail(reason ? reason : "combat_reassist", target));
    }

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

bool WorldBotCreatureAI::TryActivateGatherRoute(service::AmbientStep const& step)
{
    if (!me || step.subjectKey.empty())
        return false;

    if (_gatherRouteState.active && _gatherRouteState.routeKey == step.subjectKey && !_gatherRouteState.points.empty())
        return true;

    std::uint32_t zoneId = me->GetZoneId();
    if (step.taskIndex >= 0 && static_cast<std::size_t>(step.taskIndex) < _session.tasks.size())
        zoneId = _session.tasks[static_cast<std::size_t>(step.taskIndex)].targetZoneId;

    std::vector<service::WorldBotRoutePlanner::RoutePath> const paths = GetWorldBotRoutePlanner().LoadZonePaths(
        step.mapId,
        zoneId,
        service::WorldBotRoutePathKind::SubRoute);

    auto const itr = std::find_if(
        paths.begin(),
        paths.end(),
        [&](service::WorldBotRoutePlanner::RoutePath const& path)
        {
            return path.routeKey == step.subjectKey && path.SupportsResourceKind(step.subjectKind) && !path.points.empty();
        });
    if (itr == paths.end())
        return false;

    _gatherRouteState.Reset();
    _gatherRouteState.active = true;
    _gatherRouteState.closedLoop = itr->closedLoop;
    _gatherRouteState.routeKey = itr->routeKey;
    _gatherRouteState.points = itr->points;

    float const seedX = step.x;
    float const seedY = step.y;
    float const seedZ = step.z;
    std::size_t bestIndex = 0;
    float bestDistance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < _gatherRouteState.points.size(); ++index)
    {
        service::WorldBotRoutePlanner::RoutePoint const& point = _gatherRouteState.points[index];
        float const dx = point.x - seedX;
        float const dy = point.y - seedY;
        float const dz = point.z - seedZ;
        float const distanceSq = (dx * dx) + (dy * dy) + (dz * dz);
        if (distanceSq < bestDistance)
        {
            bestDistance = distanceSq;
            bestIndex = index;
        }
    }
    _gatherRouteState.waypointIndex = bestIndex;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "gather_route_activate",
        step.label + " route_key='" + _gatherRouteState.routeKey
            + "' point_count=" + std::to_string(_gatherRouteState.points.size())
            + " start_index=" + std::to_string(_gatherRouteState.waypointIndex));
    return true;
}

void WorldBotCreatureAI::MaybeAdvanceGatherRouteWaypoint()
{
    if (!me || !_gatherRouteState.active || _gatherRouteState.points.empty())
        return;

    service::WorldBotRoutePlanner::RoutePoint const& current =
        _gatherRouteState.points[_gatherRouteState.waypointIndex];
    if (me->GetDistance(current.x, current.y, current.z) > ArrivalThreshold)
        return;

    _gatherMovingToNode = false;
    if ((_gatherRouteState.waypointIndex + 1u) < _gatherRouteState.points.size())
    {
        ++_gatherRouteState.waypointIndex;
    }
    else
    {
        _gatherRouteState.waypointIndex = 0;
    }

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "gather_route_waypoint",
        "route_key='" + _gatherRouteState.routeKey
            + "' next_index=" + std::to_string(_gatherRouteState.waypointIndex));
}

bool WorldBotCreatureAI::TryMoveAlongGatherRoute()
{
    if (!me || !_gatherRouteState.active || _gatherRouteState.points.empty())
        return false;

    MaybeAdvanceGatherRouteWaypoint();
    service::WorldBotRoutePlanner::RoutePoint const& point =
        _gatherRouteState.points[_gatherRouteState.waypointIndex];

    float const distance = me->GetDistance(point.x, point.y, point.z);
    if (distance <= ArrivalThreshold)
        return true;

    if (!_gatherMovingToNode)
    {
        me->GetMotionMaster()->MovePoint(
            static_cast<uint32>(_currentStep),
            point.x,
            point.y,
            point.z);
        _gatherMovingToNode = true;
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "gather_route_move",
            "route_key='" + _gatherRouteState.routeKey
                + "' waypoint_index=" + std::to_string(_gatherRouteState.waypointIndex));
    }

    return true;
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
        (void)TryActivateGatherRoute(step);
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

        if (TryMoveAlongGatherRoute())
            return;

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
    me->HandleEmoteCommand(EMOTE_ONESHOT_LOOT);
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

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "gather_success",
        step.label + " target_guid=" + std::to_string(target->GetGUID().GetCounter())
            + " despawned=1");
}

void WorldBotCreatureAI::TickGrindStep(service::AmbientStep const& step)
{
    if (_activityTimer == 0)
    {
        _nextGrindMeanderWorldMs = 0;
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
    {
        if (!TryStartGrindCombat(step))
        {
            float const meanderRadius = std::max(8.0f, std::min(step.combatRadius, 25.0f));
            if (_worldOnlineMs >= _nextGrindMeanderWorldMs
                && me->GetDistance(step.x, step.y, step.z) <= std::max(8.0f, step.combatRadius * 1.5f))
            {
                Position center(step.x, step.y, step.z, me->GetOrientation());
                Position dest = me->GetRandomPoint(center, meanderRadius);
                me->GetMotionMaster()->MovePoint(0, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
                _nextGrindMeanderWorldMs = _worldOnlineMs + urand(5000u, 11000u);
                integration::BotActivityLog::Record(
                    me,
                    _identity.name,
                    _identity.id,
                    "activity_meander",
                    step.label + " center_x=" + std::to_string(step.x)
                        + " center_y=" + std::to_string(step.y)
                        + " radius=" + std::to_string(meanderRadius));
            }
        }
    }

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

    Unit* const preTickVictim = me->GetVictim();
    bool const preTickInCombat = me->IsInCombat();
    bool const preTickEngaged = me->IsEngaged();

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
            Unit* threatVictim = me->CanHaveThreatList() ? me->GetThreatMgr().GetCurrentVictim() : nullptr;
            std::ostringstream oss;
            oss << "phase='combat' decision='flow' "
                << "reason='" << reason << "' "
                << "bot_in_combat=" << (me->IsInCombat() ? 1 : 0) << " "
                << "bot_is_engaged=" << (me->IsEngaged() ? 1 : 0) << " "
                << "bot_casting=" << (me->IsNonMeleeSpellCast(false) ? 1 : 0) << " "
                << "bot_attackers=" << me->getAttackers().size() << " "
                << "combat_interrupt=" << (_combatInterrupt.active ? 1 : 0) << " "
                << "pre_tick_in_combat=" << (preTickInCombat ? 1 : 0) << " "
                << "pre_tick_engaged=" << (preTickEngaged ? 1 : 0) << " "
                << "pre_tick_victim=" << DescribeTraceUnit(preTickVictim) << " "
                << "pre_tick_victim_alive=" << ((preTickVictim && preTickVictim->IsAlive()) ? 1 : 0) << " "
                << "pre_tick_victim_evade=" << ((preTickVictim && preTickVictim->ToCreature() && preTickVictim->ToCreature()->IsInEvadeMode()) ? 1 : 0) << " "
                << "pre_tick_distance=" << ((preTickVictim && me) ? me->GetDistance(preTickVictim) : 0.0f) << " "
                << "victim=" << DescribeTraceUnit(targetForTrace) << " "
                << "threat_victim=" << DescribeTraceUnit(threatVictim) << " "
                << "threat_list_size=" << (me->CanHaveThreatList() ? me->GetThreatMgr().GetThreatListSize() : 0);
            RecordCombatTrace(oss.str());
        };

    SuspendCurrentStepForCombat(me->GetVictim());

    if (!UpdateVictim())
    {
        if (IsDebugForcedCombatIdentity() && (preTickInCombat || preTickVictim))
        {
            std::ostringstream edge;
            edge << "phase='combat' decision='combat_edge' "
                 << "reason='update_victim_false' "
                 << "pre_tick_victim=" << DescribeTraceUnit(preTickVictim) << " "
                 << "pre_tick_victim_alive=" << ((preTickVictim && preTickVictim->IsAlive()) ? 1 : 0) << " "
                 << "pre_tick_victim_evade=" << ((preTickVictim && preTickVictim->ToCreature() && preTickVictim->ToCreature()->IsInEvadeMode()) ? 1 : 0) << " "
                 << "pre_tick_distance=" << ((preTickVictim && me) ? me->GetDistance(preTickVictim) : 0.0f) << " "
                 << "bot_in_combat=" << (me->IsInCombat() ? 1 : 0) << " "
                 << "bot_is_engaged=" << (me->IsEngaged() ? 1 : 0) << " "
                 << "bot_attackers=" << me->getAttackers().size() << " "
                 << "threat_list_size=" << (me->CanHaveThreatList() ? me->GetThreatMgr().GetThreatListSize() : 0) << " "
                 << DescribeAttackValidityForTrace(me, preTickVictim);
            RecordCombatTrace(edge.str());
        }

        if (TryAdoptGroupedCombatTarget("group_target_request_update_victim_false"))
            return;

        if (TrySustainAmbientCombat("update_victim_false_reassist"))
            return;

        _combatDisengageGraceMs = std::min(CombatDisengageGraceMs, _combatDisengageGraceMs + diff);
        recordDebugCombatFlowTrace("update_victim_false");
        return;
    }

    Unit* target = me->GetVictim();
    if (!target)
    {
        if (IsDebugForcedCombatIdentity() && (preTickInCombat || preTickVictim))
        {
            std::ostringstream edge;
            edge << "phase='combat' decision='combat_edge' "
                 << "reason='missing_victim_after_update' "
                 << "pre_tick_victim=" << DescribeTraceUnit(preTickVictim) << " "
                 << "pre_tick_victim_alive=" << ((preTickVictim && preTickVictim->IsAlive()) ? 1 : 0) << " "
                 << "pre_tick_victim_evade=" << ((preTickVictim && preTickVictim->ToCreature() && preTickVictim->ToCreature()->IsInEvadeMode()) ? 1 : 0) << " "
                 << "pre_tick_distance=" << ((preTickVictim && me) ? me->GetDistance(preTickVictim) : 0.0f) << " "
                 << "bot_in_combat=" << (me->IsInCombat() ? 1 : 0) << " "
                 << "bot_is_engaged=" << (me->IsEngaged() ? 1 : 0) << " "
                 << "bot_attackers=" << me->getAttackers().size() << " "
                 << "threat_list_size=" << (me->CanHaveThreatList() ? me->GetThreatMgr().GetThreatListSize() : 0) << " "
                 << DescribeAttackValidityForTrace(me, preTickVictim);
            RecordCombatTrace(edge.str());
        }

        if (TryAdoptGroupedCombatTarget("group_target_request_missing_victim"))
            return;

        if (TrySustainAmbientCombat("missing_victim_reassist"))
            return;

        _combatDisengageGraceMs = std::min(CombatDisengageGraceMs, _combatDisengageGraceMs + diff);
        recordDebugCombatFlowTrace("missing_victim_after_update");
        return;
    }

    SyncControlledGuardianPetAssist(target);

    _combatDisengageGraceMs = 0;
    PublishAmbientGroupPrimaryTarget(target);
    PublishAmbientGroupTankAnchor(target);

    if (_identity.ambientGroupId != 0 && IsAmbientDpsRole())
    {
        std::uint64_t const nowMs = GameTime::GetGameTimeMS().count();
        service::AmbientGroupCombatSnapshot const snapshot =
            GetAmbientGroupCombatStateService().Get(_identity.ambientGroupId, nowMs);
        (void)TryClaimAmbientGroupPeelTarget(snapshot, nowMs);
        (void)TryClaimAmbientGroupPeelAssistTarget(snapshot, nowMs);
    }

    if (TryAdoptClaimedPeelTarget("combat_tick"))
    {
        target = me->GetVictim();
        if (!target)
            return;
    }

    if (TryAdoptClaimedPeelAssistTarget("combat_tick"))
    {
        target = me->GetVictim();
        if (!target)
            return;
    }

    if (IsDebugForcedCombatIdentity())
    {
        bool const hasAura = target->HasAura(JudgementOfWisdomBaseSpellId);
        bool const targetChanged = _debugJudgementAuraObservation.targetGuid != target->GetGUID();
        bool const stateChanged =
            !_debugJudgementAuraObservation.initialized
            || targetChanged
            || _debugJudgementAuraObservation.hasAura != hasAura;
        if (stateChanged)
        {
            std::ostringstream auraTrace;
            auraTrace << "phase='debug' decision='target_aura_edge' "
                      << "aura='Judgement of Wisdom(20186)' "
                      << "target=" << DescribeTraceUnit(target) << " "
                      << "state='" << (hasAura ? "gained_or_present" : "missing_or_lost") << "' "
                      << "target_changed=" << (targetChanged ? 1 : 0);
            RecordCombatTrace(auraTrace.str());

            _debugJudgementAuraObservation.targetGuid = target->GetGUID();
            _debugJudgementAuraObservation.hasAura = hasAura;
            _debugJudgementAuraObservation.initialized = true;
        }
    }

    model::WorldBotHazardSnapshot const hazard = BuildWorldBotHazardSnapshot(me, _hazardEvaluationState);
    std::vector<service::WorldBotNearbyHostileSnapshot> const nearbyHostiles =
        CollectNearbyHostileSnapshots(me, target, 30.0f);
    bool const inEffectiveMeleeRange = me->IsWithinMeleeRange(target);
    model::WorldBotCombatSituation const situation = service::BuildWorldBotCombatSituation(
        _preparedBuild,
        me,
        true,
        me->GetDistance(target),
        inEffectiveMeleeRange,
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
                << " effective_melee=" << (situation.inEffectiveMeleeRange ? 1 : 0)
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
            constexpr float RiskyVerticalChaseDelta = 8.0f;
            constexpr float RiskyVerticalChase2dWindow = 12.0f;
            constexpr float SafeChaseProbeStepYards = 3.0f;
            constexpr float StickyMeleeCloseProbeStartYards = 4.0f;
            constexpr float StickyMeleeCloseProbeMaxYards = 12.0f;

            auto const tryIssueLedgeSafeMeleeAnchor =
                [&]() -> bool
                {
                    if (!me || !target || !me->GetMap())
                        return false;

                    if (situation.movementStyle != model::WorldBotMovementStyle::FrontlineTank
                        && situation.movementStyle != model::WorldBotMovementStyle::StickyMelee)
                        return false;

                    if (movementDecision.posture != model::WorldBotCombatPosture::Hold
                        && movementDecision.posture != model::WorldBotCombatPosture::Close)
                        return false;

                    float const targetDistance2d = me->GetExactDist2d(target);
                    if (targetDistance2d > 8.0f)
                        return false;

                    float const desiredRange = service::ResolveWorldBotDesiredRange(
                        situation.movementStyle,
                        movementDecision.posture);
                    if (desiredRange <= 0.0f)
                        return false;

                    bool puntAwareTarget = false;
                    if (Creature* targetCreature = target->ToCreature())
                        puntAwareTarget = CreatureHasKnownSpell(targetCreature, MechanoKickSpellId);

                    struct LedgeAnchorCandidate
                    {
                        bool valid = false;
                        float x = 0.0f;
                        float y = 0.0f;
                        float z = 0.0f;
                        float backDrop = 1000.0f;
                        float sideDrop = 0.0f;
                        float rearSupportDistance = TerrainRearSupportProbeYards;
                        float orbitOffset = 0.0f;
                        float orbitRadius = 0.0f;
                        float travelDelta = 0.0f;
                        float score = std::numeric_limits<float>::max();
                    };

                    auto const evaluatePosition =
                        [&](float candidateX, float candidateY, float facingAngle, float orbitOffset = 0.0f, float orbitRadius = 0.0f) -> LedgeAnchorCandidate
                        {
                            LedgeAnchorCandidate candidate;
                            TerrainFootingSample const sample =
                                EvaluateCombatFootingAt(me, target, candidateX, candidateY, facingAngle);
                            if (!sample.valid)
                                return candidate;

                            candidate.valid = true;
                            candidate.x = sample.x;
                            candidate.y = sample.y;
                            candidate.z = sample.z;
                            candidate.backDrop = sample.backDrop;
                            candidate.sideDrop = sample.sideDrop;
                            candidate.rearSupportDistance = sample.rearSupportDistance;
                            candidate.orbitOffset = orbitOffset;
                            candidate.orbitRadius = orbitRadius;
                            candidate.travelDelta = sample.travelDelta;
                            candidate.score = ScoreTerrainSurveyCandidate(
                                sample,
                                puntAwareTarget,
                                orbitRadius,
                                orbitOffset);
                            return candidate;
                        };

                    float const currentAngle = target->GetAngle(me);
                    LedgeAnchorCandidate const current = evaluatePosition(
                        me->GetPositionX(),
                        me->GetPositionY(),
                        currentAngle);

                    float const dangerThreshold = puntAwareTarget
                        ? TerrainPuntAwareBackDropYards
                        : TerrainDangerBackDropYards;
                    bool const currentRisky =
                        current.valid
                        && (current.backDrop >= dangerThreshold
                            || current.sideDrop >= TerrainDangerBackDropYards
                            || (puntAwareTarget && current.rearSupportDistance > TerrainRearSupportProbeYards - 0.5f));

                    TerrainSurveyCandidate surveyedBest;
                    TerrainSurveyDiagnostics diagnostics;
                    bool reusedCache = false;
                    if (CanReuseTerrainSurveyCache(
                            _terrainSurveyCache,
                            me,
                            target,
                            desiredRange,
                            puntAwareTarget,
                            12.0f))
                    {
                        surveyedBest = MakeTerrainSurveyCandidateFromCache(_terrainSurveyCache);
                        reusedCache = true;
                    }
                    else
                    {
                        surveyedBest = FindBestCombatFootingSurveyAroundTarget(
                            me,
                            target,
                            desiredRange,
                            puntAwareTarget,
                            12.0f,
                            { -0.5f, 0.0f, 0.75f, 1.5f },
                            { 0.0f, QuarterPi, -QuarterPi, HalfPi, -HalfPi, HalfPi + QuarterPi, -(HalfPi + QuarterPi), Pi },
                            &diagnostics);
                        if (!surveyedBest.valid)
                        {
                            surveyedBest = FindBestCombatFootingSurveyNearFight(
                                me,
                                target,
                                puntAwareTarget,
                                12.0f,
                                &diagnostics);
                        }
                        StoreTerrainSurveyCache(
                            _terrainSurveyCache,
                            me,
                            target,
                            desiredRange,
                            puntAwareTarget,
                            12.0f,
                            surveyedBest);
                    }

                    float const requiredScoreGain = puntAwareTarget ? 0.25f : 1.0f;
                    if (!surveyedBest.valid)
                    {
                        if (IsDebugForcedCombatIdentity())
                        {
                            std::ostringstream skip;
                            skip << BuildCombatMovementTraceDetail("terrain_survey_skip", target)
                                 << " stage='combat_anchor' reason='no_candidate'"
                                 << " punt_aware=" << (puntAwareTarget ? 1 : 0)
                                 << " reused_cache=" << (reusedCache ? 1 : 0)
                                 << DescribeTerrainDiagnostics(diagnostics)
                                 << " current_back_drop=" << current.backDrop
                                 << " current_side_drop=" << current.sideDrop
                                 << " current_rear_support=" << current.rearSupportDistance
                                 << " current_score=" << current.score;
                            RecordCombatTrace(skip.str());
                        }
                        return false;
                    }

                    LedgeAnchorCandidate best = evaluatePosition(
                        surveyedBest.x,
                        surveyedBest.y,
                        surveyedBest.facingAngle,
                        surveyedBest.orbitOffset,
                        surveyedBest.orbitRadius);
                    if (!best.valid)
                        return false;

                    bool const shouldUseSurveyedAnchor =
                        puntAwareTarget
                            ? (best.score + requiredScoreGain < current.score)
                            : (currentRisky && best.score + requiredScoreGain < current.score);
                    if (!shouldUseSurveyedAnchor)
                    {
                        if (IsDebugForcedCombatIdentity())
                        {
                            std::ostringstream skip;
                            skip << BuildCombatMovementTraceDetail("terrain_survey_skip", target)
                                 << " stage='combat_anchor' reason='insufficient_gain'"
                                 << " punt_aware=" << (puntAwareTarget ? 1 : 0)
                                 << " reused_cache=" << (reusedCache ? 1 : 0)
                                 << " current_risky=" << (currentRisky ? 1 : 0)
                                 << " current_back_drop=" << current.backDrop
                                 << " current_side_drop=" << current.sideDrop
                                 << " current_rear_support=" << current.rearSupportDistance
                                 << " best_back_drop=" << best.backDrop
                                 << " best_side_drop=" << best.sideDrop
                                 << " best_rear_support=" << best.rearSupportDistance
                                 << " current_score=" << current.score
                                 << " best_score=" << best.score;
                            RecordCombatTrace(skip.str());
                        }
                        return false;
                    }

                    me->GetMotionMaster()->MovePoint(0, best.x, best.y, best.z);

                    std::ostringstream ledgeTrace;
                    ledgeTrace << BuildCombatMovementTraceDetail("ledge_anchor", target)
                               << " current_back_drop=" << current.backDrop
                               << " current_side_drop=" << current.sideDrop
                               << " current_rear_support=" << current.rearSupportDistance
                               << " best_back_drop=" << best.backDrop
                               << " best_side_drop=" << best.sideDrop
                               << " best_rear_support=" << best.rearSupportDistance
                               << " punt_aware=" << (puntAwareTarget ? 1 : 0)
                               << " orbit_offset=" << best.orbitOffset
                               << " orbit_radius=" << best.orbitRadius
                               << " reused_cache=" << (reusedCache ? 1 : 0)
                               << " current_score=" << current.score
                               << " best_score=" << best.score
                               << " destination=(" << best.x << "," << best.y << "," << best.z << ")";
                    RecordCombatTrace(ledgeTrace.str());
                    return true;
                };

            switch (movementPlan.kind)
            {
                case service::WorldBotMovementPlanKind::MovePoint:
                    me->GetMotionMaster()->MovePoint(0, movementPlan.pointX, movementPlan.pointY, movementPlan.pointZ);
                    break;
                case service::WorldBotMovementPlanKind::Chase:
                {
                    if (tryIssueLedgeSafeMeleeAnchor())
                        break;

                    float const targetZDelta = std::fabs(me->GetPositionZ() - target->GetPositionZ());
                    float const targetDistance2d = me->GetExactDist2d(target);

                    bool const stickyMeleeStyle =
                        situation.movementStyle == model::WorldBotMovementStyle::FrontlineTank
                        || situation.movementStyle == model::WorldBotMovementStyle::StickyMelee;

                    if (stickyMeleeStyle
                        && movementDecision.posture == model::WorldBotCombatPosture::Close
                        && targetDistance2d >= StickyMeleeCloseProbeStartYards
                        && targetDistance2d <= StickyMeleeCloseProbeMaxYards)
                    {
                        Position dest = me->GetPosition();
                        float const probeStep = std::clamp(
                            targetDistance2d - 2.0f,
                            1.5f,
                            4.0f);

                        me->MovePositionToFirstCollision(dest, probeStep, me->GetAngle(target));

                        float destX = dest.GetPositionX();
                        float destY = dest.GetPositionY();
                        float destZ = dest.GetPositionZ();
                        bool usedValidatedDestination = false;
                        if (me->GetMap()
                            && me->GetMap()->CanReachPositionAndGetValidCoords(me, destX, destY, destZ, true, true))
                        {
                            usedValidatedDestination = true;
                        }

                        me->GetMotionMaster()->MovePoint(0, destX, destY, destZ);

                        std::ostringstream closeTrace;
                        closeTrace << BuildCombatMovementTraceDetail("close_probe", target)
                                   << " probe_step=" << probeStep
                                   << " validated=" << (usedValidatedDestination ? 1 : 0)
                                   << " destination=(" << destX << "," << destY << "," << destZ << ")";
                        RecordCombatTrace(closeTrace.str());
                        break;
                    }

                    // If the target suddenly diverges vertically at close range,
                    // treat it as a risky ledge break and advance with a short,
                    // collision-safe probe instead of blindly full-chasing.
                    if (targetZDelta >= RiskyVerticalChaseDelta
                        && targetDistance2d <= RiskyVerticalChase2dWindow)
                    {
                        Position dest = me->GetPosition();
                        float const probeStep = std::clamp(
                            std::max(movementPlan.chaseDistance + 0.5f, SafeChaseProbeStepYards),
                            1.5f,
                            std::max(1.5f, std::min(targetDistance2d, 4.0f)));

                        me->MovePositionToFirstCollision(dest, probeStep, me->GetAngle(target));

                        float destX = dest.GetPositionX();
                        float destY = dest.GetPositionY();
                        float destZ = dest.GetPositionZ();
                        if (me->GetMap()
                            && me->GetMap()->CanReachPositionAndGetValidCoords(me, destX, destY, destZ, true, true))
                        {
                            me->GetMotionMaster()->MovePoint(0, destX, destY, destZ);
                        }
                        else
                        {
                            me->GetMotionMaster()->MovePoint(0, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
                        }
                        break;
                    }

                    me->GetMotionMaster()->MoveChase(target, movementPlan.chaseDistance);
                    break;
                }
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

    bool acted = TryUseAutomaticCombatPotion(target);
    service::BotCombatEvaluationResult interruptResult;
    service::BotCombatEvaluationResult rotationResult;
    if (!acted
        && (!_combatPreparedProfile.interruptEntries.empty() || !_combatPreparedProfile.rotationEntries.empty()))
    {
        service::BotCombatRuntimeContext context;
        context.bot = me;
        context.owner = nullptr;
        context.primaryTarget = target;
        context.allowHardCasts = movementDecision.allowHardCasts;
        context.syntheticGlobalCooldownRemainingMs = _syntheticGlobalCooldownRemainingMs;
        context.usedSimulatedItemsThisCombat = &_usedSimulatedItemsThisCombat;
        context.equippedWorldBotItemIds = &_assignedGearItemIds;
        context.simulatedPotionUsesThisSession = &_simulatedPotionUsesThisSession;
        context.genericPotionCharges = &_genericPotionCharges;
        context.simulatedPotionUseLimit = MaxSimulatedPotionUsesPerSession;
        for (model::WorldBotAssignedGearEntry const& entry : _preparedBuild.assignedGear)
        {
            if (entry.slot == EQUIPMENT_SLOT_TRINKET1)
                context.equippedTrinket1ItemId = entry.itemId;
            else if (entry.slot == EQUIPMENT_SLOT_TRINKET2)
                context.equippedTrinket2ItemId = entry.itemId;
        }
        context.rotationWaitMs = _combatPreparedProfile.resolution.profile.settings.rotationWaitMs;
        context.defaultAoEMode = _combatPreparedProfile.resolution.profile.settings.defaultAoEMode;
        context.defaultAoEMinTargets = _combatPreparedProfile.resolution.profile.settings.defaultAoEMinTargets;
        context.defaultAoEScanRadius = _combatPreparedProfile.resolution.profile.settings.defaultAoEScanRadius;
        context.situation = situation;
        context.conservationMode = effectiveConservation.mode;
        context.conserving = _combatConserving;
        context.offenseSuppressed = offenseSuppressed;
        context.enableDownRank = _combatPreparedProfile.resolution.profile.settings.enableDownRank;
        context.downRankFloor = _combatPreparedProfile.resolution.profile.settings.downRankFloor;
        context.customSpellWaitResolver = [this](std::uint32_t spellId)
        {
            return GetCustomSpellWaitMs(spellId);
        };
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
                    NoteSuccessfulSpellCast(action.spellId, action.target);

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
                    {
                        if (me->ToCreature())
                        {
                            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(dispatchResult.resolvedSpellId))
                                _syntheticGlobalCooldownRemainingMs =
                                    ComputeSyntheticCreatureGlobalCooldownMs(me, spellInfo);
                        }

                        _usedSimulatedItemsThisCombat.insert(action.itemId);

                        if (service::DoesSimulatedCombatItemCountAsPotionUse(action.itemId)
                            && _simulatedPotionUsesThisSession < MaxSimulatedPotionUsesPerSession)
                        {
                            ++_simulatedPotionUsesThisSession;
                            if ((service::IsGenericHealingPotionSelector(action.itemSelector)
                                    || service::IsGenericManaPotionSelector(action.itemSelector))
                                && _genericPotionCharges > 0)
                            {
                                --_genericPotionCharges;
                                _identity.genericPotionCharges = _genericPotionCharges;
                                GetIdentityRepo().UpdateGenericPotionCharges(_identity.id, _genericPotionCharges);
                            }
                            std::ostringstream potionTrace;
                            potionTrace << "phase='combat' decision='simulated_potion_use' "
                                << "item_id=" << action.itemId << " "
                                << "item_selector='" << action.itemSelector << "' "
                                << "used=" << static_cast<std::uint32_t>(_simulatedPotionUsesThisSession) << " "
                                << "limit=" << static_cast<std::uint32_t>(MaxSimulatedPotionUsesPerSession) << " "
                                << "charges_after=" << static_cast<std::uint32_t>(_genericPotionCharges) << " "
                                << "target=" << DescribeTraceUnit(target);
                            RecordCombatTrace(potionTrace.str());
                        }
                    }

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
            {
                bool const stickyMeleeStyle =
                    situation.movementStyle == model::WorldBotMovementStyle::FrontlineTank
                    || situation.movementStyle == model::WorldBotMovementStyle::StickyMelee;
                float const targetDistance2d = me->GetExactDist2d(target);

                if (stickyMeleeStyle
                    && movementDecision.posture == model::WorldBotCombatPosture::Hold
                    && targetDistance2d >= 2.75f
                    && targetDistance2d <= 6.0f
                    && me->isAttackReady(BASE_ATTACK))
                {
                    Position dest = me->GetPosition();
                    float const probeStep = std::clamp(targetDistance2d - 2.0f, 0.5f, 1.5f);
                    me->MovePositionToFirstCollision(dest, probeStep, me->GetAngle(target));

                    float destX = dest.GetPositionX();
                    float destY = dest.GetPositionY();
                    float destZ = dest.GetPositionZ();
                    bool usedValidatedDestination = false;
                    if (me->GetMap()
                        && me->GetMap()->CanReachPositionAndGetValidCoords(me, destX, destY, destZ, true, true))
                    {
                        usedValidatedDestination = true;
                    }

                    me->GetMotionMaster()->MovePoint(0, destX, destY, destZ);

                    std::ostringstream probeTrace;
                    probeTrace << BuildCombatMovementTraceDetail("contact_probe", target)
                               << " probe_step=" << probeStep
                               << " validated=" << (usedValidatedDestination ? 1 : 0)
                               << " destination=(" << destX << "," << destY << "," << destZ << ")";
                    RecordCombatTrace(probeTrace.str());
                    break;
                }

                recordMovementDoctrineTrace();
                break;
            }
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
            ResetLocalTravelFallbackState();
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

            _activeTravelNavigationPolicy = ResolveTravelNavigationPolicy(step);
            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_policy",
                BuildTravelNarrative(
                    _session,
                    step,
                    std::string("nav_policy=") + DescribeTravelNavigationPolicy(_activeTravelNavigationPolicy)
                        + " direct_yd=" + std::to_string(me->GetDistance(step.x, step.y, step.z))));

            service::WorldBotResolvedTravelOption travelOption;
            if (_activeTravelNavigationPolicy == TravelNavigationPolicy::MacroTravel
                && _session.sourceKind != "debug_path_scout"
                && TryBuildBestTravelOption(step, travelOption))
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
            else if (_activeTravelNavigationPolicy == TravelNavigationPolicy::MacroTravel
                && _session.sourceKind != "debug_path_scout")
            {
                service::WorldBotResolvedTravelPlan routePlan;
                if (TryBuildRouteTravelPlan(step, routePlan))
                {
                    _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                    _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
                    ActivateRouteTravelPlan(routePlan);
                }
            }
            else
            {
                _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
            }

            if (!_routeTravelPlanActive && _session.sourceKind == "debug_path_scout")
            {
                _activeTravelExecutionPhase = ActiveTravelExecutionPhase::GroundOnly;
                _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
                if (!TryBuildDebugScoutTravelPlan(step, travelTier))
                {
                    integration::BotActivityLog::Record(
                        me,
                        _identity.name,
                        _identity.id,
                        "session_abort",
                        "travel_scout_rejected -> ending debug path scout session");
                    _traveling = false;
                    ClearVisibleTravelMode();
                    ClearActiveRouteTravelPlan();
                    ClearActiveTaxiTravel();
                    ResetTravelWatchdog(_travelWatchdog);
                    _sessionDone = true;
                    return;
                }
            }

            if (_routeTravelPlanActive)
            {
                std::string travelPlanDetail;
                if (_debugScoutPathActive)
                {
                    travelPlanDetail = BuildTravelNarrative(
                        _session,
                        step,
                        "scout nav path"
                            " total_yd=" + std::to_string(_routeTravelPlan.totalDistanceYards)
                            + " eta=" + FormatDurationMs(_routeTravelPlan.etaMs)
                            + " waypoints=" + std::to_string(_routeTravelPlan.waypoints.size()));
                }
                else if (_activeTravelNavigationPolicy == TravelNavigationPolicy::LocalWithPoiConnector)
                {
                    travelPlanDetail = BuildTravelNarrative(
                        _session,
                        step,
                        "connector chain via '" + (_routeTravelPlan.waypoints.empty()
                            ? std::string("unknown")
                            : _routeTravelPlan.waypoints.front().routeKey)
                            + "' attach_yd=" + std::to_string(_routeTravelPlan.attachDistanceYards)
                            + " route_yd=" + std::to_string(_routeTravelPlan.routeDistanceYards)
                            + " final_leg_yd=" + std::to_string(_routeTravelPlan.detachDistanceYards)
                            + " total_yd=" + std::to_string(_routeTravelPlan.totalDistanceYards)
                            + " eta=" + FormatDurationMs(_routeTravelPlan.etaMs)
                            + " waypoints=" + std::to_string(_routeTravelPlan.waypoints.size()));
                }
                else if (_activeTravelNavigationPolicy == TravelNavigationPolicy::LocalWithAssist)
                {
                    travelPlanDetail = BuildTravelNarrative(
                        _session,
                        step,
                        "assist route '" + (_routeTravelPlan.waypoints.empty()
                            ? std::string("unknown")
                            : _routeTravelPlan.waypoints.front().routeKey)
                            + "' attach_yd=" + std::to_string(_routeTravelPlan.attachDistanceYards)
                            + " route_yd=" + std::to_string(_routeTravelPlan.routeDistanceYards)
                            + " final_leg_yd=" + std::to_string(_routeTravelPlan.detachDistanceYards)
                            + " total_yd=" + std::to_string(_routeTravelPlan.totalDistanceYards)
                            + " eta=" + FormatDurationMs(_routeTravelPlan.etaMs)
                            + " waypoints=" + std::to_string(_routeTravelPlan.waypoints.size()));
                }
                else
                {
                    travelPlanDetail = BuildTravelNarrative(
                        _session,
                        step,
                        "nearest node found"
                            " attach_yd=" + std::to_string(_routeTravelPlan.attachDistanceYards)
                            + " route_yd=" + std::to_string(_routeTravelPlan.routeDistanceYards)
                            + " final_leg_yd=" + std::to_string(_routeTravelPlan.detachDistanceYards)
                            + " total_yd=" + std::to_string(_routeTravelPlan.totalDistanceYards)
                            + " eta=" + FormatDurationMs(_routeTravelPlan.etaMs)
                            + " waypoints=" + std::to_string(_routeTravelPlan.waypoints.size()));
                }

                integration::BotActivityLog::Record(
                    me, _identity.name, _identity.id,
                    "travel_plan",
                    travelPlanDetail);
            }

            _travelWatchdogConfig = BuildActiveTravelWatchdogConfig(step, travelTier);
            ApplyVisibleTravelMode(travelTier);
            if (!MoveToActiveTravelTarget(step))
                return;
            _traveling = true;

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "travel_start",
                BuildTravelNarrative(_session, step, DescribeActiveTravelTarget(step)));

            integration::BotActivityLog::Record(
                me, _identity.name, _identity.id,
                "status_change",
                std::string("Starting travel | mode=") + DescribeTravelCapabilityTier(travelTier)
                    + " nav_policy=" + DescribeTravelNavigationPolicy(_activeTravelNavigationPolicy)
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

            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
                TryIssueAmbientGroupTravelFollow(step);

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
            if (dist <= ResolveActiveTravelArrivalThreshold(step))
            {
                if (_routeTravelPlanActive && AdvanceAlongActiveRouteTravelPlan())
                {
                    if (!MoveToActiveTravelTarget(step))
                        return;
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
                AbortCurrentTravelForNoPath(
                    step,
                    targetX,
                    targetY,
                    targetZ,
                    false,
                    0u,
                    0.0f,
                    0u,
                    signal == TravelWatchdogSignal::Timeout
                        ? "travel timeout abort"
                        : "travel stuck abort");
                return;
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
        (void)TryRefillGenericPotionsFromCityService();
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
    ResetLocalTravelFallbackState();
    ClearVisibleTravelMode();
    ClearActiveRouteTravelPlan();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();
    ResetGatherState();
    ResetTravelWatchdog(_travelWatchdog);
    ResetIdleWatchdog();

    if (_currentStep >= _session.steps.size())
        CompletSession();
    else
        PersistRuntimeLedgerState();
}

bool WorldBotCreatureAI::TryChainFollowupSession()
{
    if (!me || !_sessionReady || _sessionDone)
        return false;
    if (!SessionSourceAllowsFollowup(_session.sourceKind, _session.sourceKey))
        return false;

    std::uint64_t const sessionBudgetMs = _identity.activeWorldSessionBudgetMs;
    if (sessionBudgetMs != 0 && _worldOnlineMs >= sessionBudgetMs)
        return false;

    SessionCompletionMetadata const previousMetadata = BuildSessionCompletionMetadata(_session, _currentStep);
    RuntimeLedgerBreadcrumbs const breadcrumbs = BuildRuntimeLedgerBreadcrumbs(_session, _currentStep, _activityTimer);
    service::AmbientSessionResumeHint const resumeHint{
        _session.sourceKind,
        _session.sourceKey.empty() ? _session.activityKey : _session.sourceKey,
        previousMetadata.taskFamily,
        previousMetadata.targetZoneId,
        previousMetadata.subjectKind,
        previousMetadata.subjectKey,
        breadcrumbs.questHubElapsedMs
    };

    std::uint32_t const currentZoneId = me->GetZoneId() != 0 ? me->GetZoneId() : _identity.lastSeenZoneId;
    std::uint32_t const reserveCityZoneId = _identity.populationRole == "city_reserve"
        ? _identity.reserveCityZoneId
        : 0u;
    service::AmbientSessionComposeBias const composeBias{
        reserveCityZoneId != 0 ? std::string("city_errand") : std::string{},
        reserveCityZoneId
    };

    service::BotActivitySessionComposer composer;
    auto nextSession = composer.Compose(
        _identity.faction,
        _identity.level,
        _identity.hasHerbalism,
        _identity.hasMining,
        _identity.hasFishing,
        reserveCityZoneId != 0 ? reserveCityZoneId : currentZoneId,
        reserveCityZoneId != 0 ? reserveCityZoneId : _identity.homeZoneId,
        reserveCityZoneId != 0 ? std::string{} : _identity.homeAnchorPointKey,
        reserveCityZoneId != 0 ? std::string{} : _identity.homeBindPointKey,
        &_knownExploredZoneIds,
        &resumeHint,
        reserveCityZoneId != 0 ? &composeBias : nullptr,
        _identity.personalityKey);

    if (!nextSession)
        return false;

    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "session_chain_continue",
        "world_online_ms=" + std::to_string(_worldOnlineMs)
            + " budget_ms=" + std::to_string(sessionBudgetMs)
            + " next_source_kind='" + (nextSession->sourceKind.empty() ? std::string("unknown") : nextSession->sourceKind)
            + "' next_source_key='" + (nextSession->sourceKey.empty() ? nextSession->activityKey : nextSession->sourceKey) + "'");

    SetIdentityAndSession(
        _identity,
        *nextSession,
        0,
        0,
        _worldOnlineMs,
        true,
        false,
        _completedSessionsThisActivation);
    return true;
}

bool WorldBotCreatureAI::TryStartNextActivation(
    std::uint32_t lastSeenZoneId,
    std::uint32_t completedActivations,
    std::string const& lastSessionSourceKind,
    std::string const& lastSessionSourceKey,
    std::string const& lastTaskFamily,
    std::uint32_t lastTaskTargetZoneId,
    std::string const& lastTaskActivityKey,
    std::string const& lastQuestHubKey,
    std::uint64_t lastQuestHubElapsedMs)
{
    if (!me)
        return false;

    if (sConfigMgr->GetOption<bool>("LivingWorld.DebugDisableActivationExtension", false))
    {
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "activation_extension_skipped",
            "reason='debug_disabled' completed_activations=" + std::to_string(completedActivations));
        return false;
    }

    std::uint32_t chance = 5u;
    if (completedActivations <= 1u)
        chance = 25u;
    else if (completedActivations == 2u)
        chance = 15u;

    std::uint32_t const roll = urand(1u, 100u);
    integration::BotActivityLog::Record(
        me,
        _identity.name,
        _identity.id,
        "activation_extension_roll",
        "completed_activations=" + std::to_string(completedActivations)
            + " chance=" + std::to_string(chance)
            + " roll=" + std::to_string(roll));

    if (roll > chance)
        return false;

    GetIdentityRepo().CompleteWorldSession(
        _identity.id,
        lastSeenZoneId,
        _worldOnlineMs,
        lastSessionSourceKind,
        lastSessionSourceKey,
        lastTaskFamily,
        lastTaskTargetZoneId,
        lastTaskActivityKey,
        lastQuestHubKey,
        lastQuestHubElapsedMs);

    auto refreshedIdentity = GetIdentityRepo().FindById(_identity.id);
    if (!refreshedIdentity || refreshedIdentity->isRetired)
    {
        me->DespawnOrUnsummon(Milliseconds(1000));
        return true;
    }

    GetIdentityRepo().MarkActive(_identity.id);
    refreshedIdentity = GetIdentityRepo().FindById(_identity.id);
    if (!refreshedIdentity)
    {
        me->DespawnOrUnsummon(Milliseconds(1000));
        return true;
    }

    service::AmbientSessionResumeHint const resumeHint{
        refreshedIdentity->lastSessionSourceKind,
        refreshedIdentity->lastSessionSourceKey,
        refreshedIdentity->lastTaskFamily,
        refreshedIdentity->lastTaskTargetZoneId,
        refreshedIdentity->lastQuestHubKey.empty() ? std::string{} : std::string("quest"),
        refreshedIdentity->lastQuestHubKey.empty() ? std::string{} : std::string("quest_hub:") + refreshedIdentity->lastQuestHubKey,
        refreshedIdentity->lastQuestHubElapsedMs
    };

    std::uint32_t const currentZoneId = me->GetZoneId() != 0 ? me->GetZoneId() : refreshedIdentity->lastSeenZoneId;
    std::uint32_t const reserveCityZoneId = refreshedIdentity->populationRole == "city_reserve"
        ? refreshedIdentity->reserveCityZoneId
        : 0u;
    service::AmbientSessionComposeBias const composeBias{
        reserveCityZoneId != 0 ? std::string("city_errand") : std::string{},
        reserveCityZoneId
    };

    service::BotActivitySessionComposer composer;
    auto nextSession = composer.Compose(
        refreshedIdentity->faction,
        refreshedIdentity->level,
        refreshedIdentity->hasHerbalism,
        refreshedIdentity->hasMining,
        refreshedIdentity->hasFishing,
        reserveCityZoneId != 0 ? reserveCityZoneId : currentZoneId,
        reserveCityZoneId != 0 ? reserveCityZoneId : refreshedIdentity->homeZoneId,
        reserveCityZoneId != 0 ? std::string{} : refreshedIdentity->homeAnchorPointKey,
        reserveCityZoneId != 0 ? std::string{} : refreshedIdentity->homeBindPointKey,
        &_knownExploredZoneIds,
        &resumeHint,
        reserveCityZoneId != 0 ? &composeBias : nullptr,
        refreshedIdentity->personalityKey);

    if (!nextSession)
    {
        GetIdentityRepo().MarkAvailable(_identity.id, lastSeenZoneId);
        me->DespawnOrUnsummon(Milliseconds(1000));
        return true;
    }

    integration::BotActivityLog::Record(
        me,
        refreshedIdentity->name,
        refreshedIdentity->id,
        "activation_extension_continue",
        "completed_activations=" + std::to_string(completedActivations)
            + " next_source_kind='" + (nextSession->sourceKind.empty() ? std::string("unknown") : nextSession->sourceKind)
            + "' next_source_key='" + (nextSession->sourceKey.empty() ? nextSession->activityKey : nextSession->sourceKey) + "'");

    SetIdentityAndSession(
        *refreshedIdentity,
        *nextSession,
        0,
        0,
        0,
        true,
        false,
        completedActivations);
    return true;
}

void WorldBotCreatureAI::CompletSession()
{
    if (_sessionDone)
        return;

    bool const budgetElapsed =
        _identity.activeWorldSessionBudgetMs != 0
        && _worldOnlineMs >= _identity.activeWorldSessionBudgetMs;

    if (!budgetElapsed && TryChainFollowupSession())
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
    RuntimeLedgerBreadcrumbs const breadcrumbs =
        BuildRuntimeLedgerBreadcrumbs(_session, _currentStep, _activityTimer);

    bool const canRequestFreshShift =
        SessionSourceAllowsFollowup(_session.sourceKind, _session.sourceKey);

    if (!budgetElapsed)
    {
        std::uint64_t const remainingMs =
            _identity.activeWorldSessionBudgetMs > _worldOnlineMs
                ? (_identity.activeWorldSessionBudgetMs - _worldOnlineMs)
                : 0ull;
        integration::BotActivityLog::Record(
            me,
            _identity.name,
            _identity.id,
            "session_clockout_early",
            "world_online_ms=" + std::to_string(_worldOnlineMs)
                + " budget_ms=" + std::to_string(_identity.activeWorldSessionBudgetMs)
                + " remaining_ms=" + std::to_string(remainingMs)
                + " last_task='" + breadcrumbs.taskActivityKey + "'"
                + " last_hub='" + breadcrumbs.questHubKey + "'");
    }

    if (canRequestFreshShift
        && TryStartNextActivation(
            zoneId,
            _completedSessionsThisActivation + 1u,
            completionMetadata.sourceKind,
            completionMetadata.sourceKey,
            completionMetadata.taskFamily,
            completionMetadata.targetZoneId,
            breadcrumbs.taskActivityKey,
            breadcrumbs.questHubKey,
            breadcrumbs.questHubElapsedMs))
    {
        return;
    }

    GetIdentityRepo().CompleteWorldSession(
        _identity.id,
        zoneId,
        _worldOnlineMs,
        completionMetadata.sourceKind,
        completionMetadata.sourceKey,
        completionMetadata.taskFamily,
        completionMetadata.targetZoneId,
        breadcrumbs.taskActivityKey,
        breadcrumbs.questHubKey,
        breadcrumbs.questHubElapsedMs);

    me->DespawnOrUnsummon(Milliseconds(1000));
}

void WorldBotCreatureAI::JustDied(Unit* killer)
{
    if (_combatInterrupt.active)
    {
        std::string const reason = BuildCombatSummaryReason("death", killer);
        RecordCombatSummary(reason.c_str());
    }

    if (_identity.ambientGroupId != 0)
        (void)GetAmbientGroupCombatStateService().ClearPeel(
            _identity.ambientGroupId,
            me->GetGUID(),
            ObjectGuid());

    if (_distressTracker.active && _identity.ambientGroupId != 0)
        (void)GetAmbientGroupCombatStateService().ClearDistress(
            _identity.ambientGroupId,
            me->GetGUID(),
            _distressTracker.attackerGuid);
    _distressTracker.Reset();

    service::ResetSharedHazardEvaluationState(_hazardEvaluationState);
    ClearVisibleTravelMode();
    ClearActiveTaxiTravel();
    ClearActivePhysicalTransit();
    _syntheticGlobalCooldownRemainingMs = 0;
    _judgementOfWisdomSnapshot.Reset();
    _consecrationSnapshot = {};
    _combatDisengageGraceMs = 0;
    _ambientFleeState.Reset();
    _ambientPursuitState.Reset();
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
        RuntimeLedgerBreadcrumbs const breadcrumbs =
            BuildRuntimeLedgerBreadcrumbs(_session, _currentStep, _activityTimer);
        GetIdentityRepo().CompleteWorldSession(
            _identity.id,
            me ? me->GetZoneId() : 0,
            _worldOnlineMs,
            completionMetadata.sourceKind,
            completionMetadata.sourceKey,
            completionMetadata.taskFamily,
            completionMetadata.targetZoneId,
            breadcrumbs.taskActivityKey,
            breadcrumbs.questHubKey,
            breadcrumbs.questHubElapsedMs);
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
