#pragma once

#include "model/BotProfile.h"
#include "model/EncounterRecord.h"

#include <cstdint>

namespace living_world
{
namespace planner
{
class RespawnCooldownPolicy
{
public:
    virtual ~RespawnCooldownPolicy() = default;

    virtual std::uint32_t ComputeCooldownSeconds(
        model::BotProfile const& profile,
        model::EncounterRecord const& lastEncounter) const = 0;
};
} // namespace planner
} // namespace living_world

