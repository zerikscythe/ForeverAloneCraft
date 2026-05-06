#pragma once

#include "model/BotCombatProfile.h"

#include <cstdint>

namespace living_world
{
namespace integration
{

class BotOocConfigRepository
{
public:
    virtual ~BotOocConfigRepository() = default;

    // Load OOC config for the given bot character GUID.
    // If no row exists one is inserted with sensible defaults and returned.
    virtual model::BotOocBehavior Load(std::uint64_t sourceCharGuid) const = 0;

    virtual void Save(std::uint64_t sourceCharGuid,
                      model::BotOocBehavior const& ooc) const = 0;

    // Called once at startup to ensure the table exists.
    virtual void EnsureSchema() const = 0;
};

} // namespace integration
} // namespace living_world
