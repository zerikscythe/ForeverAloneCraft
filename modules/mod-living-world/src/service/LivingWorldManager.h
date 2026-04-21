#pragma once

// Top-level coordinator for the living-world module. In this slice it owns
// the minimum wiring needed to exercise the first orchestration flow:
//   - a reference to the AzerothWorldFacade implementation in use
//   - a concrete PartyRosterPlanner
//   - a PartyBotService built on top of those two
//
// Later slices will grow this class to own update scheduling, zone
// population, rival guild management, and progression sync. Keep it thin -
// do not let it become a junk drawer of cross-cutting logic. New
// subsystems belong in their own service classes; this manager only wires
// them together and owns their lifetime.

#include "integration/AzerothWorldFacade.h"
#include "planner/SimplePartyRosterPlanner.h"
#include "service/PartyBotService.h"

namespace living_world
{
namespace service
{
class LivingWorldManager
{
public:
    // The facade reference must outlive the manager. Module loaders are
    // expected to own the concrete facade instance and pass it in here.
    explicit LivingWorldManager(integration::AzerothWorldFacade const& facade);

    // Access points for the wired services. Returned as references because
    // callers should not rebuild these ad hoc - a single instance per
    // manager is the intended lifecycle.
    PartyBotService const& GetPartyBotService() const { return _partyBotService; }

private:
    integration::AzerothWorldFacade const& _facade;
    planner::SimplePartyRosterPlanner _partyRosterPlanner;
    PartyBotService _partyBotService;
};
} // namespace service
} // namespace living_world
