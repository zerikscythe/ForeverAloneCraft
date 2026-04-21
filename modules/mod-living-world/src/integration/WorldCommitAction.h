#pragma once

// Explicit world-mutation actions produced by the service layer after a
// planner has approved a plan. Services never mutate world state directly;
// they emit these records, and a future authoritative commit layer
// translates them into AzerothCore operations. This keeps planners pure and
// keeps every write path visible in one data structure.

#include "model/EncounterTypes.h"
#include "integration/WorldReadContext.h"

#include <cstdint>
#include <variant>
#include <vector>

namespace living_world
{
namespace integration
{
// Spawn a controllable bot body tied to a roster entry at the given
// position. Consumed by the future PartyBot commit path.
struct SpawnRosterBodyAction
{
    std::uint64_t rosterEntryId = 0;
    std::uint64_t botId = 0;
    std::uint64_t characterGuid = 0;
    WorldPosition position;
    model::RosterEntrySource source = model::RosterEntrySource::GenericBot;
    model::PossessionMode possessionMode = model::PossessionMode::Assisted;
};

// Tear down a previously spawned roster body. Kept separate from spawn so
// the commit layer can log and audit each half of the lifecycle.
struct DespawnRosterBodyAction
{
    std::uint64_t rosterEntryId = 0;
    std::uint64_t characterGuid = 0;
};

// Attach a spawned body to the requesting player's party group.
struct AttachToPartyAction
{
    std::uint64_t rosterEntryId = 0;
    std::uint64_t leaderCharacterGuid = 0;
};

// Update the abstract (off-world) state record for a bot. Used by the
// population planner when a bot transitions between abstract and live, or
// between activities.
struct UpdateAbstractStateAction
{
    std::uint64_t botId = 0;
    model::BotActivity newActivity = model::BotActivity::Abstract;
    std::uint32_t newBoundZoneId = 0;
};

// Queue an encounter record for later resolution by the rival / encounter
// service. Used to keep encounter continuity out of the spawn planner.
struct EnqueueEncounterAction
{
    std::uint64_t encounterId = 0;
    std::vector<std::uint64_t> participantBotIds;
    model::EncounterDisposition disposition = model::EncounterDisposition::PassBy;
    model::GroupAlertState initialGroupState = model::GroupAlertState::Idle;
};

// Tagged union of every commit action the service layer can produce. Adding
// a new action means extending this variant and teaching the commit layer
// to handle it - no hidden side-channels.
using WorldCommitAction = std::variant<
    SpawnRosterBodyAction,
    DespawnRosterBodyAction,
    AttachToPartyAction,
    UpdateAbstractStateAction,
    EnqueueEncounterAction>;
} // namespace integration
} // namespace living_world
