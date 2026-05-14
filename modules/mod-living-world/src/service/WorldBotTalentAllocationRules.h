#pragma once

#include "model/BotTalentTemplate.h"
#include "model/WorldBotPreparedBuild.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace living_world
{
namespace service
{
namespace detail
{
constexpr std::size_t WorldBotMaxTalentRank = 5;
}

struct WorldBotTalentDefinition
{
    std::uint16_t talentId = 0;
    std::uint32_t talentTabId = 0;
    std::uint32_t classMask = 0;
    std::uint8_t row = 0;
    std::uint16_t dependsOnTalentId = 0;
    std::uint8_t dependsOnRequiredRank = 0;
    std::array<std::uint32_t, detail::WorldBotMaxTalentRank> rankSpellIds{};
};

inline std::uint8_t GetWorldBotTalentMaxRank(WorldBotTalentDefinition const& definition)
{
    std::uint8_t maxRank = 0;
    for (std::uint32_t spellId : definition.rankSpellIds)
    {
        if (spellId != 0)
            ++maxRank;
    }

    return maxRank;
}

inline bool CanAllocateWorldBotTalentRank(
    WorldBotTalentDefinition const& definition,
    std::uint32_t playerClassMask,
    std::unordered_map<std::uint16_t, std::uint8_t> const& allocatedRanksByTalentId,
    std::unordered_map<std::uint32_t, std::uint32_t> const& spentPointsByTabId)
{
    if (definition.talentId == 0)
        return false;

    if (definition.classMask == 0 || (definition.classMask & playerClassMask) == 0)
        return false;

    auto const currentRankIt = allocatedRanksByTalentId.find(definition.talentId);
    std::uint8_t const currentRank = currentRankIt != allocatedRanksByTalentId.end()
        ? currentRankIt->second
        : 0;
    if (currentRank >= GetWorldBotTalentMaxRank(definition))
        return false;

    if (definition.dependsOnTalentId != 0)
    {
        auto const dependencyRankIt = allocatedRanksByTalentId.find(definition.dependsOnTalentId);
        std::uint8_t const dependencyRank = dependencyRankIt != allocatedRanksByTalentId.end()
            ? dependencyRankIt->second
            : 0;
        if (dependencyRank < definition.dependsOnRequiredRank)
            return false;
    }

    if (definition.row > 0)
    {
        auto const spentPointsIt = spentPointsByTabId.find(definition.talentTabId);
        std::uint32_t const spentPointsInTab = spentPointsIt != spentPointsByTabId.end()
            ? spentPointsIt->second
            : 0u;
        if (spentPointsInTab < (static_cast<std::uint32_t>(definition.row) * detail::WorldBotMaxTalentRank))
            return false;
    }

    return true;
}

inline std::vector<model::WorldBotPreparedTalentEntry> AllocateWorldBotTalents(
    std::vector<model::BotTalentTemplateEntry> const& templateEntries,
    std::vector<WorldBotTalentDefinition> const& talentDefinitions,
    std::uint32_t playerClassMask,
    std::uint8_t availableTalentPoints,
    std::uint8_t& allocatedTalentPoints)
{
    std::unordered_map<std::uint16_t, WorldBotTalentDefinition const*> definitionsByTalentId;
    definitionsByTalentId.reserve(talentDefinitions.size());
    for (WorldBotTalentDefinition const& definition : talentDefinitions)
        definitionsByTalentId[definition.talentId] = &definition;

    std::unordered_map<std::uint16_t, std::uint8_t> allocatedRanksByTalentId;
    std::unordered_map<std::uint32_t, std::uint32_t> spentPointsByTabId;
    allocatedTalentPoints = 0;

    bool madeProgress = true;
    while (allocatedTalentPoints < availableTalentPoints && madeProgress)
    {
        madeProgress = false;

        for (model::BotTalentTemplateEntry const& entry : templateEntries)
        {
            if (allocatedTalentPoints >= availableTalentPoints)
                break;

            auto const definitionIt = definitionsByTalentId.find(entry.talentId);
            if (definitionIt == definitionsByTalentId.end() || !definitionIt->second)
                continue;

            WorldBotTalentDefinition const& definition = *definitionIt->second;
            std::uint8_t const desiredRank = std::min(entry.desiredRank, GetWorldBotTalentMaxRank(definition));
            if (desiredRank == 0)
                continue;

            auto const currentRankIt = allocatedRanksByTalentId.find(entry.talentId);
            std::uint8_t const currentRank = currentRankIt != allocatedRanksByTalentId.end()
                ? currentRankIt->second
                : 0;
            if (currentRank >= desiredRank)
                continue;

            if (!CanAllocateWorldBotTalentRank(
                    definition,
                    playerClassMask,
                    allocatedRanksByTalentId,
                    spentPointsByTabId))
            {
                continue;
            }

            allocatedRanksByTalentId[entry.talentId] = static_cast<std::uint8_t>(currentRank + 1);
            ++spentPointsByTabId[definition.talentTabId];
            ++allocatedTalentPoints;
            madeProgress = true;
        }
    }

    std::vector<model::WorldBotPreparedTalentEntry> allocated;
    allocated.reserve(templateEntries.size());

    std::unordered_set<std::uint16_t> emittedTalentIds;
    for (model::BotTalentTemplateEntry const& entry : templateEntries)
    {
        if (!emittedTalentIds.insert(entry.talentId).second)
            continue;

        auto const rankIt = allocatedRanksByTalentId.find(entry.talentId);
        if (rankIt == allocatedRanksByTalentId.end() || rankIt->second == 0)
            continue;

        auto const definitionIt = definitionsByTalentId.find(entry.talentId);
        if (definitionIt == definitionsByTalentId.end() || !definitionIt->second)
            continue;

        model::WorldBotPreparedTalentEntry preparedEntry;
        preparedEntry.templateEntryId = entry.entryId;
        preparedEntry.talentId = entry.talentId;
        preparedEntry.allocatedRank = rankIt->second;
        preparedEntry.grantedSpellId = definitionIt->second->rankSpellIds[rankIt->second - 1];
        allocated.push_back(preparedEntry);
    }

    return allocated;
}
} // namespace service
} // namespace living_world