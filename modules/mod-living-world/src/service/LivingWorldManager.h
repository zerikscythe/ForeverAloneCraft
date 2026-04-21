#pragma once

// Top-level coordinator for the living-world module. In this slice it owns
// the minimum wiring needed to exercise the first orchestration flow:
//   - references to the external adapters (facade + roster repository)
//   - a concrete PartyRosterPlanner
//   - a PartyBotService built on top of the above
//
// Later slices will grow this class to own update scheduling, zone
// population, rival guild management, and progression sync. Keep it thin -
// do not let it become a junk drawer of cross-cutting logic. New
// subsystems belong in their own service classes; this manager only wires
// them together and owns their lifetime.

#include "integration/AzerothWorldFacade.h"
#include "integration/RosterRepository.h"
#include "planner/SimplePartyRosterPlanner.h"
#include "service/PartyBotService.h"

namespace living_world
{
namespace service
{
class LivingWorldManager
{
public:
    // Both adapter references must outlive the manager. Module loaders
    // are expected to own the concrete facade and repository instances
    // and pass them in here.
    LivingWorldManager(
        integration::AzerothWorldFacade const& facade,
        integration::RosterRepository const& rosterRepository);

    // Access points for the wired services. Returned as references
    // because callers should not rebuild these ad hoc - a single
    // instance per manager is the intended lifecycle.
    PartyBotService const& GetPartyBotService() const { return _partyBotService; }

private:
    integration::AzerothWorldFacade const& _facade;
    integration::RosterRepository const& _rosterRepository;
    planner::SimplePartyRosterPlanner _partyRosterPlanner;
    PartyBotService _partyBotService;
};
} // namespace service
} // namespace living_world
