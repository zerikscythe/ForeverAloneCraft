#pragma once

#include "model/BotCombatProfile.h"
#include "service/BotContextService.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
class AccountAltRuntimeRepository;
class BotCombatProfileRepository;
class BotCombatProfileSelectionRepository;
class BotCombatDefaultProfileRepository;
} // namespace integration

namespace service
{
class BotCombatSpecRoleResolver;

struct BotCombatResolvedProfile
{
    model::BotCombatProfileSettings settings;
    std::vector<model::BotCombatEntryDefinition> interruptEntries;
    std::vector<model::BotCombatEntryDefinition> rotationEntries;
};

enum class BotCombatDoctrineSource : std::uint8_t
{
    None                           = 0,
    DefaultProfile                 = 1,  // account bot — no custom overrides, using default
    CustomProfile                  = 2,  // account bot — has custom rotation entries
    CustomProfileWithDefaultFallback = 3, // account bot — custom header, default rotation
    WorldBot                       = 4,  // world/guild/BG bot — default profile IS the profile
};

struct BotCombatDoctrineResolution
{
    std::uint64_t botCharacterGuid = 0;
    std::uint64_t sourceCharacterGuid = 0;
    std::uint32_t ownerAccountId = 0;
    std::uint8_t activeProfileSlot = 0;
    std::string guessedSpecKey;
    std::string guessedRoleKey;
    std::string effectiveSpecKey;
    std::string effectiveRoleKey;
    BotCombatDoctrineSource source = BotCombatDoctrineSource::None;
    std::optional<std::uint64_t> customProfileId;
    std::optional<std::uint64_t> defaultProfileId;
    BotCombatResolvedProfile profile;
};

class BotCombatDoctrineResolver
{
public:
    BotCombatDoctrineResolver(
        integration::AccountAltRuntimeRepository const& runtimeRepository,
        integration::BotCombatProfileRepository const& profileRepository,
        integration::BotCombatProfileSelectionRepository const& selectionRepository,
        integration::BotCombatDefaultProfileRepository const& defaultProfileRepository,
        BotCombatSpecRoleResolver const& specRoleResolver,
        BotContextService const& contextService);

    [[nodiscard]] BotCombatDoctrineResolution ResolveForBot(
        std::uint64_t botCharacterGuid,
        std::uint8_t botClassId,
        std::uint32_t ownerAccountId) const;

    // Resolve doctrine for a world/guild/BG bot that has no account owner.
    // Bypasses the entire account-profile chain and goes directly to the
    // matching default profile for (specKey, roleKey, contextKey).
    // The specKey and roleKey are determined at bot spawn time by the
    // world population planner.
    [[nodiscard]] BotCombatDoctrineResolution ResolveForWorldBot(
        std::uint64_t botCharacterGuid,
        std::uint8_t botClassId,
        std::string const& specKey,
        std::string const& roleKey,
        std::string const& contextKey = "PvE",
        std::string const& variantKey = "") const;

private:
    integration::AccountAltRuntimeRepository const& _runtimeRepository;
    integration::BotCombatProfileRepository const& _profileRepository;
    integration::BotCombatProfileSelectionRepository const& _selectionRepository;
    integration::BotCombatDefaultProfileRepository const& _defaultProfileRepository;
    BotCombatSpecRoleResolver const& _specRoleResolver;
    BotContextService const& _contextService;
};
} // namespace service
} // namespace living_world
