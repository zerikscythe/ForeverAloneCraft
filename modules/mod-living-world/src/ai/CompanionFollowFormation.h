#pragma once

#include "model/BotGlobalConfig.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace living_world
{
namespace ai
{

struct CompanionFollowFormationInput
{
    model::FollowFormation formation = model::FollowFormation::Ring;
    float baseDistance = 0.0f;
    std::uint32_t slotCount = 1;
    std::uint64_t botGuid = 0;
    std::vector<std::uint64_t> ownerBotGuids;
};

struct CompanionFollowFormationResult
{
    std::uint32_t slot = 0;
    std::uint32_t rosterIndex = 0;
    std::uint32_t rosterSize = 0;
    bool usedRosterSlot = false;
    float angle = 3.14159265358979323846f;
    float distance = 0.0f;
};

inline CompanionFollowFormationResult ResolveCompanionFollowFormation(
    CompanionFollowFormationInput input)
{
    constexpr float FollowAngle = 3.14159265358979323846f;

    CompanionFollowFormationResult result;
    std::uint32_t const slots = std::max(1u, input.slotCount);

    result.distance = input.baseDistance;

    if (!input.ownerBotGuids.empty())
    {
        std::sort(input.ownerBotGuids.begin(), input.ownerBotGuids.end());
        result.rosterSize = static_cast<std::uint32_t>(input.ownerBotGuids.size());

        auto const it = std::find(input.ownerBotGuids.begin(), input.ownerBotGuids.end(), input.botGuid);
        if (it != input.ownerBotGuids.end())
        {
            result.rosterIndex = static_cast<std::uint32_t>(std::distance(input.ownerBotGuids.begin(), it));
            result.slot = result.rosterIndex % slots;
            result.usedRosterSlot = true;
        }
    }

    if (!result.usedRosterSlot)
        result.slot = static_cast<std::uint32_t>(input.botGuid % slots);

    switch (input.formation)
    {
        case model::FollowFormation::Ring:
        {
            float const angleStep = 2.0f * FollowAngle / static_cast<float>(slots);
            result.angle = FollowAngle + (static_cast<float>(result.slot) * angleStep);
            break;
        }
        case model::FollowFormation::V:
        {
            float const spread = FollowAngle / 6.0f;
            int const side = (result.slot == 0) ? 0 : ((result.slot % 2 == 1) ? 1 : -1);
            int const depth = static_cast<int>((result.slot + 1) / 2);
            result.angle = FollowAngle + static_cast<float>(side * depth) * spread;
            break;
        }
        case model::FollowFormation::Line:
        {
            result.angle = FollowAngle;
            result.distance = input.baseDistance + static_cast<float>(result.slot) * 1.5f;
            break;
        }
        case model::FollowFormation::Cluster:
        default:
        {
            result.angle = FollowAngle;
            break;
        }
    }

    return result;
}

} // namespace ai
} // namespace living_world