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
    struct RuntimeSnapshot
    {
        integration::BotIdentityRecord     identity;
        service::AmbientSession            session;
        AbstractWorldBotProgressState      progress;
        std::uint64_t                      worldOnlineMs = 0;
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
    std::string DescribeActiveTravelTarget(service::AmbientStep const& step) const;

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
    model::WorldBotPreparedBuild _preparedBuild;
    service::BotCombatPreparedProfile _combatPreparedProfile;
    ObjectGuid     _gatherTargetGuid;
    std::string    _lastCombatTraceDetail;
    std::uint64_t  _lastCombatTraceWorldMs = 0;
    std::uint64_t  _lastDebugCombatManaDrainWorldMs = 0;
    bool           _debugCombatManaGemObserved = false;
    bool           _hasShieldBaseline = false;
    bool           _routeTravelPlanActive = false;
    std::size_t    _routeTravelWaypointIndex = 0;
    std::unordered_set<std::uint32_t> _usedSimulatedItemsThisCombat;
    service::SharedHazardEvaluationState _hazardEvaluationState;
    service::WorldBotResolvedTravelPlan _routeTravelPlan;

    // Accumulates UpdateAI diff for the 500ms tick gate.
    std::uint32_t _tickAccum     = 0;

    static constexpr std::uint32_t TickIntervalMs  = 500;
    static constexpr float         ArrivalThreshold = 15.f;
    static constexpr std::uint32_t PositionSnapshotIntervalMs = 15000;
};

} // namespace ai
} // namespace living_world
