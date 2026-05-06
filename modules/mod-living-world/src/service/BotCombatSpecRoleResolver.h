#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace living_world
{
namespace service
{
struct BotCombatSpecRoleResolutionRequest
{
    std::uint64_t sourceCharacterGuid = 0;
    std::uint8_t classId = 0;
    std::optional<std::string> specOverrideKey;
    std::optional<std::string> roleOverrideKey;
};

struct BotCombatSpecRoleResolutionResult
{
    std::string guessedSpecKey;
    std::string guessedRoleKey;
    std::string effectiveSpecKey;
    std::string effectiveRoleKey;
};

// Service seam for server-side best-guess doctrine selection. The first real
// implementation is expected to use talent-tree information when available,
// with class heuristics as the fallback, then apply any player profile
// overrides to produce the effective spec/role pair.
class BotCombatSpecRoleResolver
{
public:
    virtual ~BotCombatSpecRoleResolver() = default;

    virtual BotCombatSpecRoleResolutionResult Resolve(
        BotCombatSpecRoleResolutionRequest const& request) const = 0;
};
} // namespace service
} // namespace living_world
