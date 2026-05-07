#pragma once

#include "service/BotActivitySessionComposer.h"
#include "ObjectGuid.h"
#include <cstddef>

class Player;

namespace living_world
{
namespace ai
{

// Schedule the ambient (ownerless, no-combat) AI for a bot that has an
// activity session assigned. The bot will travel to its destination then
// execute its activity step until the session completes, then log out.
void ScheduleAmbientBotAI(Player* botPlayer);

} // namespace ai
} // namespace living_world
