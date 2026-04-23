#pragma once

#include "model/AccountAltRuntime.h"

#include <cstdint>
#include <optional>

namespace living_world
{
namespace integration
{
// Provides bot-owned accounts that can host one active runtime clone each.
// This avoids rewriting AzerothCore's one WorldSession per account model.
class BotAccountPoolRepository
{
public:
    virtual ~BotAccountPoolRepository() = default;

    virtual std::optional<model::BotAccountLease> ReserveAccountForSource(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid) = 0;
};
} // namespace integration
} // namespace living_world
