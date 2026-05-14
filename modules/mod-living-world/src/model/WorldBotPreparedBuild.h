#pragma once

#include "model/WorldBotAssignedGear.h"
#include "model/WorldBotVirtualLoadout.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace living_world
{
namespace model
{
enum class WorldBotPreparationStatus : std::uint8_t
{
    Ready = 0,
    MissingDefaultCombatProfile = 1,
    MissingTalentTemplate = 2,
    MissingPlayerInfo = 3,
};

struct WorldBotPreparedTalentEntry
{
    std::uint64_t templateEntryId = 0;
    std::uint16_t talentId = 0;
    std::uint8_t allocatedRank = 0;
    std::uint32_t grantedSpellId = 0;
};

struct WorldBotPreparedBuild
{
    std::uint32_t identityId = 0;
    std::uint8_t raceId = 0;
    std::uint8_t classId = 0;
    std::uint8_t level = 1;
    std::string personalityKey = "uninterested";
    std::string requestedLoadoutKey;
    std::string canonicalSpecKey;
    std::string resolvedRoleKey;
    std::string contextKey = "PvE";
    WorldBotPreparationStatus status = WorldBotPreparationStatus::MissingDefaultCombatProfile;
    std::string failureReason;

    std::uint64_t defaultCombatProfileId = 0;
    std::string defaultCombatProfileName;
    std::string defaultCombatProfileVariantKey;
    std::string defaultCombatProfileDescription;

    std::uint64_t talentTemplateId = 0;
    std::string talentTemplateName;
    std::string talentTemplateVariantKey;
    std::string talentTemplateDescription;

    std::uint8_t availableTalentPoints = 0;
    std::uint8_t allocatedTalentPoints = 0;
    std::vector<WorldBotPreparedTalentEntry> allocatedTalents;
    std::unordered_set<std::uint32_t> knownSpellIds;
    std::optional<WorldBotVirtualLoadout> virtualLoadout;
    std::vector<WorldBotAssignedGearEntry> assignedGear;
    WorldBotAssignedGearSummary assignedGearSummary;
    std::uint8_t assignedGearRefreshBand = 0;
    bool assignedGearRefreshed = false;

    [[nodiscard]] bool IsReady() const
    {
        return status == WorldBotPreparationStatus::Ready;
    }
};
} // namespace model
} // namespace living_world