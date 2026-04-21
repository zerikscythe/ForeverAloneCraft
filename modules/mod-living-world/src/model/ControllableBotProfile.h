#pragma once

#include "model/BotProfile.h"

namespace living_world
{
namespace model
{
struct ControllableBotProfile
{
    BotProfile profile;
    bool canBePlayerControlled = true;
    bool canEarnProgression = true;
    bool canJoinPlayerParty = true;
};
} // namespace model
} // namespace living_world

