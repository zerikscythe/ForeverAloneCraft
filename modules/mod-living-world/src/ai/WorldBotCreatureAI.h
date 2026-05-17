#pragma once

#include "ai/AbstractWorldBotProgressor.h"
#include "ai/TravelWatchdog.h"
#include "integration/BotActivityLog.h"
#include "integration/SqlBotIdentityRepository.h"
#include "model/WorldBotPreparedBuild.h"
#include "service/BotActivitySessionComposer.h"
#include "service/BotCombatProfilePreparationService.h"
#include "service/BotCombatRuntimeEvaluator.h"
#include "service/SharedHazardEvaluation.h"
#include "service/WorldBotRoutePlanning.h"
#include "service/WorldBotTaxiPlanning.h"

#include "CreatureAI.h"

#include <unordered_set>

class GameObject;

namespace living_world
{
namespace ai
{

// WorldBotCreatureAI drives creature-based world bots (ambient, hostile, raid).
// The creature is spawned from living_world_bot_identity via the population tick.
// It needs no WoW account or Player session.
//
// Lifecycle:
//   1. SummonCreature(WORLD_BOT_ENTRY, pos) spawns the creature.
//   2. Caller casts AI and calls SetIdentityAndSession(record, session).
//   3. UpdateAI ticks through session steps (Travel then Activity).
//   4. On session complete the creature despawns and the identity is marked
//      available again so it can reappear in a future tick.
class WorldBotCreatureAI : public CreatureAI
{
public:
    enum class ActiveTravelExecutionPhase : std::uint8_t
    {
        None = 0,
        GroundOnly = 1,
        TaxiApproach = 2,
        TaxiTransit = 3,
        TaxiFinalLeg = 4,
    };

    enum class ActiveTransitExecutionPhase : std::uint8_t
    {
        None = 0,
        WaitingForTransport = 1,
        Boarding = 2,
        Riding = 3,
    };

    struct ActivePhysicalTransitState
    {
        std::string routeKey;
        std::string transitType;
        std::string sourceLabel;
        std::string destLabel;
        std::uint32_t transportEntry = 0;
        std::uint16_t sourceMapId = 0;
        std::uint16_t destMapId = 0;
        float sourceX = 0.0f;
        float sourceY = 0.0f;
        float sourceZ = 0.0f;
        float destX = 0.0f;
        float destY = 0.0f;
        float destZ = 0.0f;
        float boardLocalX = 0.0f;
        float boardLocalY = 0.0f;
        float boardLocalZ = 0.0f;
        float boardLocalO = 0.0f;
        float boardDetectRadius = 90.0f;
        float boardArriveRadius = 10.0f;
        float disembarkRadius = 70.0f;
        ObjectGuid activeTransportGuid;

        [[nodiscard]] bool empty() const
        {
            return routeKey.empty() || transportEntry == 0;
        }
    };

    struct RuntimeSnapshot
    {
        integration::BotIdentityRecord     identity;
        service::AmbientSession            session;
        AbstractWorldBotProgressState      progress;
        std::uint64_t                      worldOnlineMs = 0;
        bool                               inTaxiTransit = false;
        bool                               inPhysicalTransit = false;
    };

    explicit WorldBotCreatureAI(Creature* creature);

    // Called by the population tick immediately after SummonCreature returns.
    void SetIdentityAndSession(
        integration::BotIdentityRecord const& identity,
        service::AmbientSession          const& session,
        std::size_t currentStep = 0,
        std::uint32_t stepElapsedMs = 0,
        std::uint64_t worldOnlineMsSoFar = 0,
        bool alreadyMarkedActive = false,
        bool resumedFromAbstract = false);

    // CreatureAI overrides
    void InitializeAI() override;
    void UpdateAI(uint32 diff) override;
    void JustDied(Unit* /*killer*/) override;
    void JustEngagedWith(Unit* who) override;
    void JustReachedHome() override;

    bool BuildRuntimeSnapshot(RuntimeSnapshot& out) const;
    [[nodiscard]] bool HasShieldBaseline() const { return _hasShieldBaseline; }
    [[nodiscard]] model::WorldBotAssignedGearSummary const& GetAssignedGearSummary() const { return _preparedBuild.assignedGearSummary; }

private:
    GameObject* ResolveGatherTarget() const;
    void TickStep(uint32 diff);
    void TickCombat(uint32 diff);
    void TickGatherStep(service::AmbientStep const& step);
    void AdvanceStep();
    void CompletSession();
    void PersistRuntimeLedgerState(std::string const& detailOverride = "") const;
    void RecordPositionSnapshot(char const* eventType, std::string const& detail) const;
    void RecordCombatTrace(std::string const& detail);
    void SuspendCurrentStepForCombat(Unit* target);
    void ResumeSuspendedStepAfterCombat();
    void EnsureCombatProfile();
    void InvalidateCombatProfile();
    bool IsDebugCombatManaDrainIdentity() const;
    bool ApplyDebugCombatManaTarget(Unit* target, char const* traceDecision, bool logAttempt = false);
    void MaybeApplyDebugCombatManaDrain(Unit* target);
    void ResetGatherState();
    bool IsGatherNodeForStep(GameObject const* go, service::AmbientStep const& step) const;
    GameObject* FindNearestGatherNode(service::AmbientStep const& step) const;
    std::string DescribeCurrentStep() const;
    std::string BuildCombatTraceDetail(
        char const* phase,
        service::BotCombatEvaluationResult const& result,
        Unit* target) const;
    std::string BuildCombatMovementTraceDetail(
        char const* decision,
        Unit* target) const;
    bool TryBuildRouteTravelPlan(
        service::AmbientStep const& step,
        service::WorldBotResolvedTravelPlan& outPlan) const;
    void ClearActiveRouteTravelPlan();
    void MoveToActiveTravelTarget(service::AmbientStep const& step);
    float GetActiveTravelTargetDistance(service::AmbientStep const& step) const;
    bool AdvanceAlongActiveRouteTravelPlan();
    bool TryReanchorActiveRouteTravelPlan(service::AmbientStep const& step, char const* reason);
    bool TryBuildBestTravelOption(
        service::AmbientStep const& step,
        service::WorldBotResolvedTravelOption& outOption) const;
    void ActivateRouteTravelPlan(service::WorldBotResolvedTravelPlan const& plan);
    void ClearActiveTaxiTravel();
    bool BeginActiveTaxiTransit(service::AmbientStep const& step);
    bool CompleteActiveTaxiTransit(service::AmbientStep const& step);
    void ClearActivePhysicalTransit();
    bool TryBeginPhysicalTransit(service::AmbientStep const& step);
    bool TickPhysicalTransit(service::AmbientStep const& step);
    ai::TravelWatchdogConfig BuildActiveTravelWatchdogConfig(
        service::AmbientStep const& step,
        service::WorldBotTravelCapabilityTier tier) const;
    std::string DescribeRuntimeStateKey() const;
    std::string DescribeRuntimeStateDetail() const;
    std::string DescribeActiveTravelTarget(service::AmbientStep const& step) const;
    service::WorldBotTravelCapabilityTier ResolveTravelCapabilityTier() const;
    void ApplyVisibleTravelMode(service::WorldBotTravelCapabilityTier tier);
    void ClearVisibleTravelMode();
    void ObserveCurrentZoneExploration();

    // Apply identity fields (level, display_id) to the creature.
    void ApplyIdentityToCreature();

    integration::BotIdentityRecord  _identity;
    service::AmbientSession         _session;

    std::size_t  _currentStep    = 0;
    std::uint32_t _activityTimer = 0;   // ms elapsed on current activity step
    bool          _traveling     = false;
    bool          _sessionReady  = false;
    bool          _sessionDone   = false;
    bool          _combatSuspendedStep = false;
    bool          _preparedBuildReady = false;
    bool          _combatProfilePrepared = false;
    bool          _gatherMovingToNode = false;
    std::uint8_t  _gatherCompletedCycles = 0;
    std::uint64_t _worldOnlineMs = 0;
    TravelWatchdogState _travelWatchdog;
    TravelWatchdogConfig _travelWatchdogConfig;
    model::WorldBotPreparedBuild _preparedBuild;
    service::BotCombatPreparedProfile _combatPreparedProfile;
    ObjectGuid     _gatherTargetGuid;
    std::string    _lastCombatTraceDetail;
    std::uint64_t  _lastCombatTraceWorldMs = 0;
    std::uint64_t  _lastDebugCombatManaDrainWorldMs = 0;
    bool           _debugCombatManaGemObserved = false;
    bool           _hasShieldBaseline = false;
    ActiveTravelExecutionPhase _activeTravelExecutionPhase = ActiveTravelExecutionPhase::None;
    service::WorldBotTravelOptionMode _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
    std::uint32_t  _activeTaxiTransitElapsedMs = 0;
    bool           _activeTravelStepStartKnown = false;
    std::uint16_t  _activeTravelStepStartMapId = 0;
    float          _activeTravelStepStartX = 0.0f;
    float          _activeTravelStepStartY = 0.0f;
    float          _activeTravelStepStartZ = 0.0f;
    bool           _routeTravelPlanActive = false;
    std::size_t    _routeTravelWaypointIndex = 0;
    std::uint32_t  _routeTravelLastZoneId = 0;
    std::uint64_t  _routeTravelLastReanchorWorldMs = 0;
    bool           _visibleTravelModeActive = false;
    std::uint32_t  _visibleTravelModeSpellId = 0;
    service::WorldBotTravelCapabilityTier _visibleTravelCapabilityTier = service::WorldBotTravelCapabilityTier::Foot;
    float          _visibleTravelSpeedRate = 1.0f;
    ActiveTransitExecutionPhase _activeTransitExecutionPhase = ActiveTransitExecutionPhase::None;
    std::unordered_set<std::uint32_t> _usedSimulatedItemsThisCombat;
    std::unordered_set<std::uint32_t> _knownExploredZoneIds;
    service::SharedHazardEvaluationState _hazardEvaluationState;
    service::WorldBotResolvedTravelPlan _routeTravelPlan;
    service::WorldBotResolvedTaxiJourney _activeTaxiJourney;
    ActivePhysicalTransitState _activePhysicalTransit;

    // Accumulates UpdateAI diff for the 500ms tick gate.
    std::uint32_t _tickAccum     = 0;

    static constexpr std::uint32_t TickIntervalMs  = 500;
    static constexpr float         ArrivalThreshold = 15.f;
    static constexpr std::uint32_t PositionSnapshotIntervalMs = 15000;
};

} // namespace ai
} // namespace living_world
