#pragma once

#include "model/BotCombatProfile.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace integration
{
// Runtime-facing selection contract. The active profile slot is expected to be
// stored separately from the profile rows themselves so a live bot/account-alt
// runtime can switch slots without rewriting the profile aggregate.
class BotCombatProfileSelectionRepository
{
public:
    virtual ~BotCombatProfileSelectionRepository() = default;

    virtual std::optional<model::BotCombatRuntimeSelection> FindRuntimeSelection(
        std::uint64_t sourceCharacterGuid) const = 0;

    virtual void SaveRuntimeSelection(
        model::BotCombatRuntimeSelection const& selection) = 0;
};
} // namespace integration
} // namespace living_world
