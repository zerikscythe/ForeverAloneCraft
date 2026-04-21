#include "service/LivingWorldManager.h"

namespace living_world
{
namespace service
{
LivingWorldManager::LivingWorldManager(
    integration::AzerothWorldFacade const& facade)
    : _facade(facade),
      _partyRosterPlanner(),
      _partyBotService(_facade, _partyRosterPlanner)
{
}
} // namespace service
} // namespace living_world
