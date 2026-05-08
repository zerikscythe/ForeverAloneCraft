#pragma once

#include "integration/BotActivityLog.h"
#include "integration/SqlBotIdentityRepository.h"
#include "service/BotActivitySessionComposer.h"

#include "CreatureAI.h"

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
    explicit WorldBotCreatureAI(Creature* creature);

    // Called by the population tick immediately after SummonCreature returns.
    void SetIdentityAndSession(
        integration::BotIdentityRecord const& identity,
        service::AmbientSession          const& session);

    // CreatureAI overrides
    void InitializeAI() override;
    void UpdateAI(uint32 diff) override;
    void JustDied(Unit* /*killer*/) override;

    // Prevent players from looting / interacting with world bots as enemies.
    void AttackStart(Unit* /*who*/) override {}
    bool CanAIAttack(Unit const* /*who*/) const override { return false; }

private:
    void TickStep(uint32 diff);
    void AdvanceStep();
    void CompletSession();

    // Apply identity fields (level, display_id) to the creature.
    void ApplyIdentityToCreature();

    integration::BotIdentityRecord  _identity;
    service::AmbientSession         _session;

    std::size_t  _currentStep    = 0;
    std::uint32_t _activityTimer = 0;   // ms elapsed on current activity step
    bool          _traveling     = false;
    bool          _sessionReady  = false;
    bool          _sessionDone   = false;

    // Accumulates UpdateAI diff for the 500ms tick gate.
    std::uint32_t _tickAccum     = 0;

    static constexpr std::uint32_t TickIntervalMs  = 500;
    static constexpr float         ArrivalThreshold = 15.f;
};

} // namespace ai
} // namespace living_world
