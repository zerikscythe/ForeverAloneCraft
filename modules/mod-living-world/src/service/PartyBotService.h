#pragma once

// Orchestrates the player-driven party-bot flow end to end without touching
// world state directly. The service is the narrowest useful runtime
// boundary: it takes a PlayerRosterRequest, queries the facade for the
// player context and the repository for the requested roster entry, calls
// the roster planner, and converts the resulting plan into explicit
// WorldCommitAction records.
//
// This class deliberately does not spawn anything. The commit layer that
// executes the emitted actions is a future slice; until then the service
// returns the plan so callers (tests, command surfaces) can inspect the
// intended outcome.

#include "integration/AzerothWorldFacade.h"
#include "integration/RosterRepository.h"
#include "integration/WorldCommitAction.h"
#include "model/PlayerRosterRequest.h"
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
    // Non-owning references. All dependencies must outlive the service;
    // LivingWorldManager is responsible for lifetime. Keeping them as
    // references keeps the service cheap to construct and trivial to
    // stub in tests.
    PartyBotService(
        integration::AzerothWorldFacade const& facade,
        integration::RosterRepository const& rosterRepository,
        planner::PartyRosterPlanner const& plannerImpl);

    // Dispatch a roster request. The service will:
    //   1. resolve the player context via the facade
    //   2. reject fast if the player cannot be controlled
    //   3. fetch the requested entry from the repository
    //   4. call the planner for final validation
    //   5. translate an approved plan into commit actions
    // Rejections carry the first failure reason encountered so callers
    // can surface a clear message without inspecting intermediate state.
    PartyBotDispatchResult DispatchRosterRequest(
        model::PlayerRosterRequest const& request) const;

private:
    integration::AzerothWorldFacade const& _facade;
    integration::RosterRepository const& _rosterRepository;
    planner::PartyRosterPlanner const& _planner;
};
} // namespace service
} // namespace living_world
