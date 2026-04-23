#pragma once

#include "model/AccountAltRuntime.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace integration
{
// Persistence boundary for account-alt runtime clones. The future SQL-backed
// implementation should make Find + Save transactional around reservation
// so two requests cannot materialize two clones for the same source alt.
class AccountAltRuntimeRepository
{
public:
    virtual ~AccountAltRuntimeRepository() = default;

    virtual std::optional<model::AccountAltRuntimeRecord>
    FindBySourceCharacter(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid) const = 0;

    virtual void SaveRuntime(
        model::AccountAltRuntimeRecord const& runtime) = 0;
};
} // namespace integration
} // namespace living_world
