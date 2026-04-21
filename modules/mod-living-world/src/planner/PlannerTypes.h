#pragma once

#include "model/EncounterTypes.h"

#include <cstdint>
#include <vector>

namespace living_world
{
namespace planner
{
struct PlannedSpawn
{
    std::uint64_t botId = 0;
    std::uint32_t zoneId = 0;
    float score = 0.0f;
};

struct ZonePopulationPlan
{
    std::vector<PlannedSpawn> spawns;
    std::uint32_t reservedBudget = 0;
};

struct PlannedEncounter
{
    std::vector<std::uint64_t> participantBotIds;
    model::EncounterDisposition disposition = model::EncounterDisposition::PassBy;
    model::GroupAlertState initialGroupState = model::GroupAlertState::Idle;
};

enum class PartyRosterFailureReason : std::uint8_t
{
    None,
    RequesterUnavailable,
    RosterEntryNotFound,
    RosterEntryDisabled,
    RosterEntryAlreadySummoned,
    PartyFull,
    OwnershipMismatch,
    DirectControlNotSupported
};

struct PlannedPartySpawn
{
    std::uint64_t rosterEntryId = 0;
    std::uint64_t botId = 0;
    std::uint64_t characterGuid = 0;
    std::uint32_t zoneId = 0;
    model::RosterEntrySource source = model::RosterEntrySource::GenericBot;
    model::PossessionMode possessionMode = model::PossessionMode::Assisted;
};

struct PartyRosterPlan
{
    bool isApproved = false;
    PartyRosterFailureReason failureReason = PartyRosterFailureReason::None;
    bool shouldSpawnAtPlayer = false;
    bool shouldJoinPlayerParty = false;
    bool progressionShouldAccrueToControlledCharacter = false;
    PlannedPartySpawn plannedSpawn;
};
} // namespace planner
} // namespace living_world
