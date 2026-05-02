#pragma once

#include "model/BotCombatProfile.h"

#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
// Read contract for baked-in server default combat doctrines. These defaults
// are the fallback when a player's selected profile slot has no custom rows or
// when the player is satisfied with the server's best-guess behavior.
class BotCombatDefaultProfileRepository
{
public:
    virtual ~BotCombatDefaultProfileRepository() = default;

    virtual std::vector<model::BotCombatDefaultProfileRecord> ListDefaultProfiles() const = 0;

    virtual std::optional<model::BotCombatDefaultProfileRecord> FindDefaultProfile(
        std::string const& specKey,
        std::string const& roleKey) const = 0;
};
} // namespace integration
} // namespace living_world
