#pragma once

// Orchestrates the player-driven party-bot flow end to end without touching
// world state directly. The service is the narrowest useful runtime
// boundary: it takes a PlayerRosterRequest, queries the facade for the
// player context it needs, calls the roster planner, and converts the
// resulting plan into explicit WorldCommitAction records.
//
// This class deliberately does not spawn anything. The commit layer that
// executes the emitted actions is a future slice; until then the service
// returns the plan so callers (tests, command surfaces) can inspect the
// intended outcome.

#include "integration/AzerothWorldFacade.h"
#include "integration/WorldCommitAction.h"
#include "model/PlayerRosterRequest.h"
#include "model/RosterEntry.h"
#include "planner/PartyRosterPlanner.h"
#include "planner/PlannerTypes.h"

#include <vector>

namespace living_world
{
namespace service
{
// Result of a single roster request dispatch. Both approved and rejected
// outcomes are first-class so command surfaces can render either case
// without re-interpreting raw planner internals.
struct PartyBotDispatchResult
{
    bool isApproved = false;
    planner::PartyRosterFailureReason failureReason =
        planner::PartyRosterFailureReason::None;
    planner::PartyRosterPlan plan;
    std::vector<integration::WorldCommitAction> commitActions;
};

class PartyBotService
{
public:
    // Non-owning references. Both dependencies must outlive the service;
    // LivingWorldManager is responsible for lifetime. Keeping them as
    // references keeps the service cheap to construct and trivial to
    // stub in tests.
    PartyBotService(
        integration::AzerothWorldFacade const& facade,
        planner::PartyRosterPlanner const& plannerImpl);

    // Dispatch a roster request. The service will:
    //   1. resolve the player context via the facade
    //   2. reject fast if the player cannot be controlled
    //   3. call the planner with the supplied roster list
    //   4. translate an approved plan into commit actions
    // Rejections carry the first failure reason encountered so callers can
    // surface a clear message without inspecting intermediate state.
    PartyBotDispatchResult DispatchRosterRequest(
        std::vector<model::RosterEntry> const& rosterEntries,
        model::PlayerRosterRequest const& request) const;

private:
    integration::AzerothWorldFacade const& _facade;
    planner::PartyRosterPlanner const& _planner;
};
} // namespace service
} // namespace living_world
