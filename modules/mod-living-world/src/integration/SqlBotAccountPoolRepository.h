#pragma once

#include "integration/BotAccountPoolRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotAccountPoolRepository final : public BotAccountPoolRepository
{
public:
    std::optional<model::BotAccountLease> ReserveAccountForSource(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid) override;
};
} // namespace integration
} // namespace living_world
