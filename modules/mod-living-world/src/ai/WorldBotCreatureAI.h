#pragma once

#include "ai/AbstractWorldBotProgressor.h"
#include "ai/TravelWatchdog.h"
#include "integration/BotActivityLog.h"
#include "integration/SqlBotIdentityRepository.h"
#include "model/BotCombatMode.h"
#include "model/WorldBotPreparedBuild.h"
#include "service/BotActivitySessionComposer.h"
#include "service/AmbientGroupCombatStateService.h"
#include "service/BotCombatProfilePreparationService.h"
#include "service/BotCombatRuntimeEvaluator.h"
#include "service/SharedHazardEvaluation.h"
#include "service/WorldBotRoutePlanning.h"
#include "service/WorldBotTaxiPlanning.h"

#include "CreatureAI.h"

#include <map>
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
    struct CombatMetrics
    {
        std::uint64_t outgoingDamage = 0;
        std::uint64_t incomingDamage = 0;
        std::uint64_t outgoingHealing = 0;
        std::uint64_t incomingHealing = 0;
    };

    enum class CombatInterruptionReason : std::uint8_t
    {
        ReactiveDefense = 0,
        AuthoredGrind = 1,
    };

    struct CombatInterruptionContext
    {
        bool active = false;
        CombatInterruptionReason reason = CombatInterruptionReason::ReactiveDefense;
        std::size_t suspendedStepIndex = 0;
        std::uint32_t allClearElapsedMs = 0;
        std::uint32_t allClearRequiredMs = 0;
        bool resumePending = false;
    };

    enum class ActiveTravelExecutionPhase : std::uint8_t
    {
        None = 0,
        GroundOnly = 1,
        TaxiApproach = 2,
        TaxiTransit = 3,
        TaxiFinalLeg = 4,
    };

    enum class TravelNavigationPolicy : std::uint8_t
    {
        LocalOnly = 0,
        LocalWithPoiConnector = 1,
        LocalWithAssist = 2,
        MacroTravel = 3,
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
        std::uint32_t                      completedSessionsThisActivation = 0;
        bool                               inCombat = false;
        bool                               isEngaged = false;
        bool                               hasVictim = false;
        bool                               hasAttackers = false;
        bool                               combatInterruptActive = false;
        bool                               inTaxiTransit = false;
        bool                               inPhysicalTransit = false;
        bool                               physicalTransitReadyForAbstract = false;
        std::uint32_t                      physicalTransitTransportEntry = 0;
        float                              physicalTransitLocalX = 0.0f;
        float                              physicalTransitLocalY = 0.0f;
        float                              physicalTransitLocalZ = 0.0f;
        float                              physicalTransitLocalO = 0.0f;
    };

    struct GroundEffectSnapshot
    {
        bool active = false;
        std::uint32_t spellId = 0;
        std::uint64_t castWorldMs = 0;
        std::uint32_t mapId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct LastIncomingDamageSnapshot
    {
        ObjectGuid sourceGuid;
        std::string sourceName;
        std::string moveName;
        std::uint32_t amount = 0;
        SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL;
        DamageEffectType damageType = NODAMAGE;
        std::uint32_t capturedAtMs = 0;

        [[nodiscard]] bool valid() const
        {
            return amount > 0 && !sourceGuid.IsEmpty();
        }
    };

    struct GroupCombatHandoffSnapshot
    {
        ObjectGuid targetGuid;
        std::uint32_t adoptedAtMs = 0;

        void Reset()
        {
            targetGuid.Clear();
            adoptedAtMs = 0;
        }
    };

    struct DebugAuraObservationSnapshot
    {
        ObjectGuid targetGuid;
        bool hasAura = false;
        bool initialized = false;

        void Reset()
        {
            targetGuid.Clear();
            hasAura = false;
            initialized = false;
        }
    };

    struct JudgementOfWisdomSnapshot
    {
        ObjectGuid targetGuid;
        std::uint64_t castWorldMs = 0;

        void Reset()
        {
            targetGuid.Clear();
            castWorldMs = 0;
        }
    };

    struct TerrainSurveyCacheSnapshot
    {
        bool valid = false;
        ObjectGuid targetGuid;
        bool puntAwareTarget = false;
        float moverX = 0.0f;
        float moverY = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        float preferredRange = 0.0f;
        float maxTravelDistance = 0.0f;
        float bestX = 0.0f;
        float bestY = 0.0f;
        float bestZ = 0.0f;
        float bestFacingAngle = 0.0f;
        float bestOrbitOffset = 0.0f;
        float bestOrbitRadius = 0.0f;
        float bestBackDrop = 1000.0f;
        float bestSideDrop = 1000.0f;
        float bestRearSupportDistance = 0.0f;
        float bestTravelDelta = 0.0f;
        float bestScore = 0.0f;

        void Reset()
        {
            valid = false;
            targetGuid.Clear();
            puntAwareTarget = false;
            moverX = 0.0f;
            moverY = 0.0f;
            targetX = 0.0f;
            targetY = 0.0f;
            preferredRange = 0.0f;
            maxTravelDistance = 0.0f;
            bestX = 0.0f;
            bestY = 0.0f;
            bestZ = 0.0f;
            bestFacingAngle = 0.0f;
            bestOrbitOffset = 0.0f;
            bestOrbitRadius = 0.0f;
            bestBackDrop = 1000.0f;
            bestSideDrop = 1000.0f;
            bestRearSupportDistance = 0.0f;
            bestTravelDelta = 0.0f;
            bestScore = 0.0f;
        }
    };

    struct PendingPullArmState
    {
        bool active = false;
        ObjectGuid targetGuid;
        std::uint32_t remainingMs = 0;
        bool waitingForStandoff = false;
        float preferredRange = 0.0f;
        std::uint64_t lastStandoffMoveWorldMs = 0;
        std::string reason;

        void Reset()
        {
            active = false;
            targetGuid.Clear();
            remainingMs = 0;
            waitingForStandoff = false;
            preferredRange = 0.0f;
            lastStandoffMoveWorldMs = 0;
            reason.clear();
        }
    };

    enum class AmbientEncounterIntent : std::uint8_t
    {
        Ignore = 0,
        Observe = 1,
        Attack = 2,
        Avoid = 3,
    };

    struct AmbientEncounterDecisionSnapshot
    {
        ObjectGuid targetGuid;
        AmbientEncounterIntent intent = AmbientEncounterIntent::Ignore;
        std::uint64_t reevaluateWorldMs = 0;

        void Reset()
        {
            targetGuid.Clear();
            intent = AmbientEncounterIntent::Ignore;
            reevaluateWorldMs = 0;
        }
    };

    struct AmbientFleeState
    {
        bool active = false;
        ObjectGuid threatGuid;
        std::uint64_t startedAtMs = 0;
        std::uint64_t lastRefreshWorldMs = 0;
        float lastThreatDistance = 0.0f;

        void Reset()
        {
            active = false;
            threatGuid.Clear();
            startedAtMs = 0;
            lastRefreshWorldMs = 0;
            lastThreatDistance = 0.0f;
        }
    };

    struct AmbientPursuitState
    {
        bool active = false;
        ObjectGuid targetGuid;
        std::uint64_t startedAtMs = 0;
        bool targetWasCoward = false;

        void Reset()
        {
            active = false;
            targetGuid.Clear();
            startedAtMs = 0;
            targetWasCoward = false;
        }
    };

    struct GatherRouteState
    {
        bool active = false;
        bool closedLoop = false;
        std::string routeKey;
        std::size_t waypointIndex = 0;
        std::vector<service::WorldBotRoutePlanner::RoutePoint> points;

        void Reset()
        {
            active = false;
            closedLoop = false;
            routeKey.clear();
            waypointIndex = 0;
            points.clear();
        }
    };

    struct TimedSpellMemory
    {
        bool active = false;
        std::uint32_t spellBaseId = 0;
        ObjectGuid targetGuid;
        std::uint32_t castWorldMs = 0;

        void Reset()
        {
            active = false;
            spellBaseId = 0;
            targetGuid.Clear();
            castWorldMs = 0;
        }
    };

    struct DistressTracker
    {
        bool active = false;
        ObjectGuid attackerGuid;
        std::uint64_t startedAtMs = 0;
        std::uint64_t lastDamageAtMs = 0;
        service::AmbientGroupDistressTier publishedTier =
            service::AmbientGroupDistressTier::None;

        void Reset()
        {
            active = false;
            attackerGuid.Clear();
            startedAtMs = 0;
            lastDamageAtMs = 0;
            publishedTier = service::AmbientGroupDistressTier::None;
        }
    };

    struct RoguePoisonState
    {
        bool active = false;
        std::uint32_t primaryPoisonBaseSpellId = 0;
        std::uint32_t primaryPoisonSpellId = 0;
        std::uint32_t secondaryPoisonBaseSpellId = 0;
        std::uint32_t secondaryPoisonSpellId = 0;
        std::uint32_t appliedAtMs = 0;
        std::uint32_t lastProcAtMs = 0;

        void Reset()
        {
            active = false;
            primaryPoisonBaseSpellId = 0;
            primaryPoisonSpellId = 0;
            secondaryPoisonBaseSpellId = 0;
            secondaryPoisonSpellId = 0;
            appliedAtMs = 0;
            lastProcAtMs = 0;
        }
    };

    struct GroupCombatTargetReply
    {
        Unit* target = nullptr;
        char const* source = "none";

        [[nodiscard]] bool HasTarget() const
        {
            return target != nullptr;
        }
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
        bool resumedFromAbstract = false,
        std::uint32_t completedSessionsThisActivation = 0);

    // CreatureAI overrides
    void InitializeAI() override;
    void UpdateAI(uint32 diff) override;
    void JustDied(Unit* /*killer*/) override;
    void JustEngagedWith(Unit* who) override;
    void JustReachedHome() override;
    void JustRespawned() override;
    void JustSummoned(Creature* summon) override;
    void SummonedCreatureDespawn(Creature* summon) override;
    void CorpseRemoved(uint32& respawnDelay) override;
    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType damageType, SpellSchoolMask damageSchoolMask) override;
    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask) override;
    void SpellHit(Unit* caster, SpellInfo const* spellInfo) override;

    bool BuildRuntimeSnapshot(RuntimeSnapshot& out) const;
    [[nodiscard]] bool HasShieldBaseline() const { return _hasShieldBaseline; }
    [[nodiscard]] model::WorldBotAssignedGearSummary const& GetAssignedGearSummary() const { return _preparedBuild.assignedGearSummary; }
    [[nodiscard]] integration::BotIdentityRecord const& GetIdentityRecord() const { return _identity; }
    void RecordCombatDamageDone(std::uint32_t amount);
    void RecordCombatDamageTaken(std::uint32_t amount);
    void RecordCombatHealingDone(std::uint32_t amount);
    void RecordCombatHealingTaken(std::uint32_t amount);
    void TryTriggerRoguePoisonProc(Unit* victim);
    void TryTriggerWorldBotReactiveGearProcs(
        Unit* victim,
        std::uint32_t damage,
        DamageEffectType damageType,
        SpellSchoolMask damageSchoolMask);

private:
    GameObject* ResolveGatherTarget() const;
    void TickStep(uint32 diff);
    void TickCombat(uint32 diff);
    void TickGatherStep(service::AmbientStep const& step);
    void TickGrindStep(service::AmbientStep const& step);
    void AdvanceStep();
    void CompletSession();
    bool TryChainFollowupSession();
    bool TryStartNextActivation(
        std::uint32_t lastSeenZoneId,
        std::uint32_t completedActivations,
        std::string const& lastSessionSourceKind,
        std::string const& lastSessionSourceKey,
        std::string const& lastTaskFamily,
        std::uint32_t lastTaskTargetZoneId,
        std::string const& lastTaskActivityKey,
        std::string const& lastQuestHubKey,
        std::uint64_t lastQuestHubElapsedMs);
    void PersistRuntimeLedgerState(std::string const& detailOverride = "") const;
    void RecordPositionSnapshot(char const* eventType, std::string const& detail) const;
    void RecordCombatTrace(std::string const& detail);
    void SuspendCurrentStepForCombat(Unit* target);
    void ResumeSuspendedStepAfterCombat();
    bool TryRequestSuspendedStepResume();
    void EnsureCombatProfile();
    void InvalidateCombatProfile();
    bool IsDebugCombatManaDrainIdentity() const;
    bool IsDebugForcedCombatIdentity() const;
    void MaybeStartDebugForcedCombat();
    bool ApplyDebugCombatManaTarget(Unit* target, char const* traceDecision, bool logAttempt = false);
    void MaybeApplyDebugCombatManaDrain(Unit* target);
    [[nodiscard]] model::BotCombatMode ResolveDebugForcedBotMode() const;
    void ResetGatherState();
    bool IsGatherNodeForStep(GameObject const* go, service::AmbientStep const& step) const;
    GameObject* FindNearestGatherNode(service::AmbientStep const& step) const;
    bool TryActivateGatherRoute(service::AmbientStep const& step);
    void MaybeAdvanceGatherRouteWaypoint();
    bool TryMoveAlongGatherRoute();
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
    [[nodiscard]] TravelNavigationPolicy ResolveTravelNavigationPolicy(
        service::AmbientStep const& step) const;
    bool TryBuildLocalAssistTravelPlan(
        service::AmbientStep const& step,
        service::WorldBotResolvedTravelPlan& outPlan) const;
    bool TryActivateLocalPoiConnectorFallback(service::AmbientStep const& step);
    void ResetLocalTravelFallbackState();
    float ResolveTravelProofArrivalThreshold() const;
    float ResolveTravelConnectorArrivalThreshold() const;
    float ResolveTravelDestinationArrivalThreshold(service::AmbientStep const& step) const;
    float ResolveActiveTravelArrivalThreshold(service::AmbientStep const& step) const;
    bool TryBuildDebugScoutTravelPlan(
        service::AmbientStep const& step,
        service::WorldBotTravelCapabilityTier tier);
    void ClearActiveRouteTravelPlan();
    bool MoveToActiveTravelTarget(service::AmbientStep const& step);
    float GetActiveTravelTargetDistance(service::AmbientStep const& step) const;
    bool AdvanceAlongActiveRouteTravelPlan();
    bool TryReanchorActiveRouteTravelPlan(service::AmbientStep const& step, char const* reason);
    void AbortCurrentTravelForNoPath(
        service::AmbientStep const& step,
        float targetX,
        float targetY,
        float targetZ,
        bool calculateResult,
        std::size_t pointCount,
        float pathLengthYards,
        std::uint32_t pathTypeBits,
        char const* reason);
    bool TryBuildBestTravelOption(
        service::AmbientStep const& step,
        service::WorldBotResolvedTravelOption& outOption) const;
    [[nodiscard]] bool IsAmbientGroupLeader() const;
    [[nodiscard]] bool IsAmbientTankRole() const;
    [[nodiscard]] bool IsAmbientHealerRole() const;
    [[nodiscard]] bool IsAmbientDpsRole() const;
    [[nodiscard]] Creature* FindAmbientGroupLeaderCreature(float radius) const;
    [[nodiscard]] float ResolveAmbientGroupFollowBaseDistance(bool pullStage = false) const;
    [[nodiscard]] std::vector<std::uint64_t> CollectAmbientGroupFollowerGuids(float radius) const;
    bool TryIssueAmbientGroupTravelFollow(service::AmbientStep const& step);
    [[nodiscard]] bool IsHealerDistressWithinWrangleWindow(
        service::AmbientGroupCombatSnapshot const& snapshot,
        std::uint64_t nowMs) const;
    [[nodiscard]] bool IsAmbientSharedDistressedAllyHealer(
        service::AmbientGroupCombatSnapshot const& snapshot) const;
    [[nodiscard]] bool IsAmbientSharedDistressedAllyDps(
        service::AmbientGroupCombatSnapshot const& snapshot) const;
    bool TryClaimAmbientGroupPeelTarget(
        service::AmbientGroupCombatSnapshot const& snapshot,
        std::uint64_t nowMs) const;
    bool TryClaimAmbientGroupPeelAssistTarget(
        service::AmbientGroupCombatSnapshot const& snapshot,
        std::uint64_t nowMs) const;
    bool TryAdoptClaimedPeelTarget(char const* reason);
    bool TryAdoptClaimedPeelAssistTarget(char const* reason);
    void ActivateRouteTravelPlan(service::WorldBotResolvedTravelPlan const& plan);
    void ClearActiveTaxiTravel();
    bool BeginActiveTaxiTransit(service::AmbientStep const& step);
    bool CompleteActiveTaxiTransit(service::AmbientStep const& step);
    void ClearActivePhysicalTransit();
    bool TryBeginPhysicalTransit(service::AmbientStep const& step);
    bool TryResumePhysicalTransit(service::AmbientStep const& step);
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
    void ResetCombatMetricsSegment();
    void RecordCombatSummary(char const* reason);
    void RecordEncounterTrace(std::string const& detail);
    void ResetIdleWatchdog();
    [[nodiscard]] bool IsIdleWatchdogEligible() const;
    [[nodiscard]] std::string BuildIdleWatchdogDetail(char const* reason, std::uint32_t stagnantMs) const;
    bool TickIdleWatchdog(std::uint32_t diff);
    [[nodiscard]] char const* ResolveAmbientTruceZoneName() const;
    [[nodiscard]] bool IsAmbientPlayerLikeTarget(Unit const* candidate) const;
    [[nodiscard]] Unit* FindNearbyAmbientPlayerLikeTarget(float radius) const;
    AmbientEncounterIntent ResolveAmbientEncounterIntent(
        Unit* target,
        std::uint32_t* chanceOut = nullptr,
        std::uint32_t* rollOut = nullptr);
    [[nodiscard]] bool CanInitiateAmbientCombatAgainstTarget(Unit* target, char const* reason);
    [[nodiscard]] std::string ResolveAmbientPersonalityKeyFor(Unit const* unit) const;
    [[nodiscard]] bool IsAmbientCowardWorldBotTarget(Unit const* target) const;
    [[nodiscard]] bool ShouldCowardFleeCombatAgainst(Unit* target) const;
    bool TryHandleAggressivePursuitTimeout();
    void RefreshAmbientPursuitState(Unit* target);
    bool TryHandleCowardCombatFlee();
    bool TryHandleAmbientPlayerEncounter();
    bool TryExecuteAmbientCowardAvoidance(Unit* target);
    [[nodiscard]] Unit* FindNearbyAmbientCombatTarget(float radius) const;
    [[nodiscard]] Unit* FindNearbyCreatureCombatTarget(float radius) const;
    [[nodiscard]] GroupCombatTargetReply RequestGroupedCombatTarget(float radius) const;
    [[nodiscard]] bool IsAmbientGroupedWith(Unit const* ally) const;
    [[nodiscard]] std::vector<Unit*> CollectAmbientGroupBuffTargets(float radius) const;
    [[nodiscard]] Creature* FindAmbientGroupTankCreature(float radius) const;
    [[nodiscard]] Creature* FindAmbientGroupHealerCreature(float radius) const;
    [[nodiscard]] bool IsActivelyTravelingForSelfState() const;
    [[nodiscard]] bool IsPreparedSelfStateActive(model::WorldBotPreparedSelfState const& state) const;
    [[nodiscard]] model::WorldBotPreparedSelfState const* SelectPreparedSelfStateForCurrentContext() const;
    bool TryApplyPreferredSelfState();
    [[nodiscard]] bool IsCurrentTaskCityPotionRefillEligible() const;
    bool TryRefillGenericPotionsFromCityService();
    void TryApplyOutOfCombatBuff();
    bool TryApplyPrePullSupport();
    bool TryUseAutomaticCombatPotion(Unit* target);
    bool TryStartPendingPullArm(Unit* target, char const* reason);
    bool TickPendingPullArm(std::uint32_t diff);
    bool TryAdoptGroupedCombatTarget(char const* reason);
    bool TryJoinNearbyAmbientCombat(char const* reason);
    bool TrySustainAmbientCombat(char const* reason);
    [[nodiscard]] Creature* FindNearestGrindTarget(service::AmbientStep const& step) const;
    bool TryStartGrindCombat(service::AmbientStep const& step);
    bool IsCombatAreaStep(service::AmbientStep const& step) const;
    std::uint32_t ResolveCombatResumeDelayMs() const;
    bool CanInterruptCurrentStepForCombat() const;
    [[nodiscard]] std::uint32_t GetCustomSpellWaitMs(std::uint32_t spellId) const;
    void NoteSuccessfulSpellCast(std::uint32_t spellId, Unit* target);
    void CaptureIncomingDamageSnapshot(
        Unit* attacker,
        std::uint32_t damage,
        DamageEffectType damageType,
        SpellSchoolMask schoolMask,
        char const* moveName = nullptr);
    [[nodiscard]] std::string BuildCombatSummaryReason(char const* fallbackReason, Unit* killer = nullptr) const;
    void NoteAmbientGroupDistressContact(Unit* attacker);
    void TickAmbientGroupDistressState();
    void PublishAmbientGroupTankAnchor(Unit* target) const;
    void PublishAmbientGroupPrimaryTarget(Unit* target) const;
    void PublishAmbientGroupPullArming() const;
    void PublishAmbientGroupPullCommitted() const;
    bool TryApplyRoguePoisonsOutOfCombat();
    [[nodiscard]] Creature* GetControlledGuardianPet() const;
    [[nodiscard]] std::vector<Creature*> CollectControlledGuardianPets() const;
    void PopulateProjectedCreatureSpellbook();
    bool TryMaintainBasicCompanionPet();
    bool TrySummonDirectHunterPet();
    void InitializeControlledGuardianPet(Creature* summon);
    void SyncControlledGuardianPetFollow();
    void SyncControlledGuardianPetAssist(Unit* target);
    void SyncControlledGuardianPetDefend(Unit* attacker);

    // Apply identity fields (level, display_id) to the creature.
    void ApplyIdentityToCreature();
    void ApplyNamedDebugRunShell();

    integration::BotIdentityRecord  _identity;
    service::AmbientSession         _session;

    std::size_t  _currentStep    = 0;
    std::uint32_t _activityTimer = 0;   // ms elapsed on current activity step
    bool          _traveling     = false;
    bool          _sessionReady  = false;
    bool          _sessionDone   = false;
    CombatInterruptionContext _combatInterrupt;
    bool          _preparedBuildReady = false;
    bool          _combatProfilePrepared = false;
    bool          _gatherMovingToNode = false;
    std::uint8_t  _gatherCompletedCycles = 0;
    std::uint64_t _worldOnlineMs = 0;
    std::uint64_t _nextGrindMeanderWorldMs = 0;
    std::uint32_t _completedSessionsThisActivation = 0;
    TravelWatchdogState _travelWatchdog;
    TravelWatchdogConfig _travelWatchdogConfig;
    TravelWatchdogState _idleWatchdog;
    TravelWatchdogConfig _idleWatchdogConfig;
    std::uint32_t _idleWatchdogLastWarningBucket = 0;
    bool          _idleWatchdogEnabled = false;
    bool          _idleWatchdogKillProcess = false;
    model::WorldBotPreparedBuild _preparedBuild;
    service::BotCombatPreparedProfile _combatPreparedProfile;
    std::unordered_set<std::uint32_t> _assignedGearItemIds;
    std::map<ObjectGuid, ObjectGuid> _controlledPetAssistTargets;
    std::uint64_t _lastControlledPetSummonAttemptWorldMs = 0;
    std::uint64_t _lastControlledPetStatusLogWorldMs = 0;
    ObjectGuid     _gatherTargetGuid;
    GatherRouteState _gatherRouteState;
    std::string    _lastCombatTraceDetail;
    std::uint64_t  _lastCombatTraceWorldMs = 0;
    std::string    _lastEncounterTraceDetail;
    std::uint64_t  _lastEncounterTraceWorldMs = 0;
    std::uint64_t  _lastDebugCombatManaDrainWorldMs = 0;
    std::uint64_t  _lastAmbientAvoidWorldMs = 0;
    bool           _debugCombatManaGemObserved = false;
    bool           _debugForcedCombatProbeLogged = false;
    bool           _hasShieldBaseline = false;
    LastIncomingDamageSnapshot _lastIncomingDamageSnapshot;
    GroupCombatHandoffSnapshot _groupCombatHandoffSnapshot;
    DebugAuraObservationSnapshot _debugJudgementAuraObservation;
    GroundEffectSnapshot _consecrationSnapshot;
    ActiveTravelExecutionPhase _activeTravelExecutionPhase = ActiveTravelExecutionPhase::None;
    TravelNavigationPolicy _activeTravelNavigationPolicy = TravelNavigationPolicy::MacroTravel;
    service::WorldBotTravelOptionMode _activeTravelOptionMode = service::WorldBotTravelOptionMode::Ground;
    std::uint32_t  _activeTaxiTransitElapsedMs = 0;
    bool           _activeTravelStepStartKnown = false;
    std::uint16_t  _activeTravelStepStartMapId = 0;
    float          _activeTravelStepStartX = 0.0f;
    float          _activeTravelStepStartY = 0.0f;
    float          _activeTravelStepStartZ = 0.0f;
    bool           _routeTravelPlanActive = false;
    bool           _debugScoutPathActive = false;
    std::size_t    _routeTravelWaypointIndex = 0;
    std::uint32_t  _routeTravelLastZoneId = 0;
    std::uint64_t  _routeTravelLastReanchorWorldMs = 0;
    std::uint8_t   _localPoiConnectorAttemptCount = 0;
    bool           _localHelperFallbackTried = false;
    bool           _localMacroFallbackTried = false;
    std::unordered_set<std::string> _localPoiConnectorTriedStartKeys;
    std::uint32_t  _syntheticGlobalCooldownRemainingMs = 0;
    JudgementOfWisdomSnapshot _judgementOfWisdomSnapshot;
    TerrainSurveyCacheSnapshot _terrainSurveyCache;
    PendingPullArmState _pendingPullArm;
    AmbientEncounterDecisionSnapshot _ambientEncounterDecision;
    AmbientFleeState _ambientFleeState;
    AmbientPursuitState _ambientPursuitState;
    TimedSpellMemory _recentOocBuff;
    TimedSpellMemory _recentPullPrep;
    DistressTracker _distressTracker;
    RoguePoisonState _roguePoisonState;
    bool           _lastUpdateObservedCombat = false;
    ObjectGuid     _lastUpdateObservedVictimGuid;
    bool           _combatConserving = false;
    std::uint32_t  _combatDisengageGraceMs = 0;
    bool           _pendingCorpseRecovery = false;
    std::uint8_t   _corpseRecoveryCount = 0;
    bool           _visibleTravelModeActive = false;
    std::uint32_t  _visibleTravelModeSpellId = 0;
    service::WorldBotTravelCapabilityTier _visibleTravelCapabilityTier = service::WorldBotTravelCapabilityTier::Foot;
    float          _visibleTravelSpeedRate = 1.0f;
    ActiveTransitExecutionPhase _activeTransitExecutionPhase = ActiveTransitExecutionPhase::None;
    std::unordered_set<std::uint32_t> _usedSimulatedItemsThisCombat;
    std::uint8_t   _genericPotionCharges = 5;
    std::uint8_t   _simulatedPotionUsesThisSession = 0;
    bool           _pendingLevelUpCelebration = false;
    std::uint8_t   _pendingLevelUpFromLevel = 0;
    std::unordered_set<std::uint32_t> _knownExploredZoneIds;
    service::SharedHazardEvaluationState _hazardEvaluationState;
    service::WorldBotResolvedTravelPlan _routeTravelPlan;
    service::WorldBotResolvedTaxiJourney _activeTaxiJourney;
    ActivePhysicalTransitState _activePhysicalTransit;
    CombatMetrics _combatMetricsCurrent;
    CombatMetrics _combatMetricsSession;

    // Accumulates UpdateAI diff for the 500ms tick gate.
    std::uint32_t _tickAccum     = 0;

    static constexpr std::uint32_t TickIntervalMs  = 500;
    static constexpr float         ArrivalThreshold = 15.f;
    static constexpr std::uint32_t PositionSnapshotIntervalMs = 15000;
    static constexpr std::uint32_t CorpseRecoveryCorpseDelaySec = 15;
    static constexpr std::uint32_t CorpseRecoveryRunbackDelaySec = 10;
    static constexpr std::uint32_t CombatDisengageGraceMs = 5000;
    static constexpr std::uint32_t ReactiveCombatResumeDelayMs = 5000;
    static constexpr float AmbientPersonalityEncounterRadius = 20.0f;
    static constexpr float AmbientCowardTriggerRadius = 12.0f;
    static constexpr float AmbientCowardAvoidDistance = 50.0f;
    static constexpr std::uint32_t AmbientEncounterDecisionWindowMs = 8000;
    static constexpr std::uint32_t AmbientAvoidMoveThrottleMs = 1500;
    static constexpr std::uint32_t AmbientAggressivePursuitBreakMs = 30000;
    static constexpr std::uint32_t AuthoredCombatResumeDelayMs = 5000;
    static constexpr float AutomaticHealingPotionThresholdPct = 35.0f;
    static constexpr float AutomaticManaPotionThresholdPct = 25.0f;
    static constexpr std::uint8_t MaxSimulatedPotionUsesPerSession = 5;
    static constexpr std::uint8_t MaxGenericPotionCharges = 5;
    static constexpr std::uint32_t JudgementOfWisdomCooldownMs = 30000;
    static constexpr std::uint32_t PullArmLeadTimeMs = 2500;
    static constexpr std::uint32_t DistressQuietClearMs = 1500;
    static constexpr std::uint32_t TankWrangleWindowMs = 3000;
    static constexpr float AmbientCombatAssistRadius = 60.0f;
};

} // namespace ai
} // namespace living_world
