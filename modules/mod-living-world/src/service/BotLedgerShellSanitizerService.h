#pragma once

#include "integration/SqlBotIdentityRepository.h"

#include <string>

class Player;

namespace living_world
{
namespace service
{

class BotLedgerShellSanitizerService
{
public:
    [[nodiscard]] bool IsCompatibleShell(
        Player const* shell,
        integration::BotIdentityRecord const& identity,
        std::string* reason = nullptr) const;

    bool SanitizeForIdentity(
        Player* shell,
        integration::BotIdentityRecord const& identity,
        std::string* reason = nullptr) const;
};

} // namespace service
} // namespace living_world
