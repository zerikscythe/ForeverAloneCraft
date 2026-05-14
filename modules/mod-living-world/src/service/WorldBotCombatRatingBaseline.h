#pragma once

#include "DataStores/DBCStores.h"
#include "Unit.h"

#include <algorithm>
#include <cstdint>

namespace living_world
{
namespace service
{
inline float ResolveWorldBotCombatRatingBonus(
    Unit const* unit,
    CombatRating rating,
    std::int32_t ratingValue)
{
    if (!unit || ratingValue == 0)
        return 0.0f;

    std::uint8_t const classId = unit->getClass();
    if (classId == 0 || classId > MAX_CLASSES)
        return 0.0f;

    std::uint8_t const clampedLevel = std::clamp<std::uint8_t>(unit->GetLevel(), 1, GT_MAX_LEVEL);
    GtCombatRatingsEntry const* ratingEntry =
        sGtCombatRatingsStore.LookupEntry(rating * GT_MAX_LEVEL + clampedLevel - 1);
    GtOCTClassCombatRatingScalarEntry const* classRating =
        sGtOCTClassCombatRatingScalarStore.LookupEntry((classId - 1) * GT_MAX_RATING + rating + 1);
    if (!ratingEntry || !classRating)
        return static_cast<float>(ratingValue);

    return static_cast<float>(ratingValue) * classRating->ratio / ratingEntry->ratio;
}
} // namespace service
} // namespace living_world
