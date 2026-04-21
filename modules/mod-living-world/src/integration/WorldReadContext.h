#pragma once

// Value types describing the minimum read-only view of AzerothCore state that
// planners and services need. These structs are filled in by an
// AzerothWorldFacade implementation and consumed as plain inputs; planners
// must never hold references to live server objects directly.

#include "model/EncounterTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
// Identity of the player driving a request. Kept minimal so the facade can
// resolve it from either a logged-in session or a stored GUID without
// pulling in WorldSession headers.
struct PlayerIdentity
{
    std::uint64_t characterGuid = 0;
    std::uint32_t accountId = 0;
};

// Positional fix for a player or spawn anchor. The coordinate fields are
// optional context for later slices (spawn placement, range checks); today
// only zone/map are consumed by the planner.
struct WorldPosition
{
    std::uint32_t mapId = 0;
    std::uint32_t zoneId = 0;
    std::uint32_t areaId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float orientation = 0.0f;
};

// Single entry in the player's current party from the server's perspective.
// Used to enforce party-size rules without the planner talking to Group.
struct PartyMemberSnapshot
{
    std::uint64_t characterGuid = 0;
    bool isOnline = true;
    bool isBotControlled = false;
};

// Snapshot of the player's party at request time.
struct PartySnapshot
{
    std::vector<PartyMemberSnapshot> members;
    std::uint32_t maxPartyMembers = 5;
};

// Aggregated read-only view of the player for the planner layer. This is the
// canonical input the service hands to PartyRosterPlanner / future planners.
struct PlayerWorldContext
{
    PlayerIdentity identity;
    WorldPosition position;
    PartySnapshot party;
    bool isInWorld = false;
    bool isInCombat = false;
    bool isDead = false;
    bool canControlCompanions = true;
};

// Candidate spawn point the population planner can score. Filled in by the
// facade from static spawn data or cached anchor tables.
struct SpawnAnchor
{
    std::uint64_t anchorId = 0;
    WorldPosition position;
    std::uint32_t softCapacity = 0;
    std::uint32_t currentOccupancy = 0;
    std::string tag;
};
} // namespace integration
} // namespace living_world
