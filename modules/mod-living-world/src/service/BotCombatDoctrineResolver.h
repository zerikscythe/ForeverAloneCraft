#pragma once

#include "model/BotCombatProfile.h"

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
    None = 0,
    DefaultProfile = 1,
    CustomProfile = 2,
    CustomProfileWithDefaultFallback = 3
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
        BotCombatSpecRoleResolver const& specRoleResolver);

    [[nodiscard]] BotCombatDoctrineResolution ResolveForBot(
        std::uint64_t botCharacterGuid,
        std::uint8_t botClassId,
        std::uint32_t ownerAccountId) const;

private:
    integration::AccountAltRuntimeRepository const& _runtimeRepository;
    integration::BotCombatProfileRepository const& _profileRepository;
    integration::BotCombatProfileSelectionRepository const& _selectionRepository;
    integration::BotCombatDefaultProfileRepository const& _defaultProfileRepository;
    BotCombatSpecRoleResolver const& _specRoleResolver;
};
} // namespace service
} // namespace living_world
