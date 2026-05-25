#pragma once

#include <cstdint>
#include <string>

namespace living_world
{
namespace service
{

class BotLedgerShellHydratorService
{
public:
    bool RehydrateIdentity(
        std::uint32_t identityId,
        std::uint32_t shellAccountId = 0,
        std::uint64_t shellCharacterGuid = 0,
        std::string* failureReason = nullptr) const;
};

} // namespace service
} // namespace living_world
