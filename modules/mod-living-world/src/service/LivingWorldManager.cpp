#include "service/LivingWorldManager.h"

namespace living_world
{
namespace service
{
LivingWorldManager::LivingWorldManager(
    integration::AzerothWorldFacade const& facade,
    integration::RosterRepository const& rosterRepository)
    : _facade(facade),
      _rosterRepository(rosterRepository),
      _partyRosterPlanner(),
      _partyBotService(_facade, _rosterRepository, _partyRosterPlanner)
{
}
} // namespace service
} // namespace living_world
