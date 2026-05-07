#include "integration/SqlActivityLibraryRepository.h"
#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
namespace
{
model::ActivityEntry BuildActivityEntry(Field const* f)
{
    model::ActivityEntry e;
    e.activityId        = f[0].Get<std::uint32_t>();
    e.activityKey       = f[1].Get<std::string>();
    e.displayName       = f[2].Get<std::string>();
    e.activityType      = f[3].Get<std::string>();
    e.targetZoneId      = f[4].Get<std::uint32_t>();
    e.requiredFaction   = f[5].Get<std::uint8_t>();
    e.minLevel          = f[6].Get<std::uint8_t>();
    e.maxLevel          = f[7].Get<std::uint8_t>();
    e.requiresHerbalism = f[8].Get<std::uint8_t>() != 0;
    e.requiresMining    = f[9].Get<std::uint8_t>() != 0;
    e.requiresFishing   = f[10].Get<std::uint8_t>() != 0;
    e.weight            = f[11].Get<std::uint8_t>();
    e.durationMinSec    = f[12].Get<std::uint32_t>();
    e.durationMaxSec    = f[13].Get<std::uint32_t>();
    return e;
}
} // namespace

std::vector<model::ActivityEntry> SqlActivityLibraryRepository::LoadEligible(
    std::uint8_t faction,
    std::uint8_t level,
    bool hasHerbalism,
    bool hasMining,
    bool hasFishing) const
{
    QueryResult qr = WorldDatabase.Query(
        "SELECT activity_id, activity_key, display_name, activity_type, "
        "target_zone_id, required_faction, min_level, max_level, "
        "requires_herbalism, requires_mining, requires_fishing, "
        "weight, duration_min_sec, duration_max_sec "
        "FROM living_world_activity_library "
        "WHERE (required_faction = 0 OR required_faction = {}) "
        "  AND min_level <= {} AND max_level >= {} "
        "  AND (requires_herbalism = 0 OR {} = 1) "
        "  AND (requires_mining    = 0 OR {} = 1) "
        "  AND (requires_fishing   = 0 OR {} = 1)",
        faction, level, level,
        static_cast<int>(hasHerbalism),
        static_cast<int>(hasMining),
        static_cast<int>(hasFishing));

    std::vector<model::ActivityEntry> result;
    if (!qr)
        return result;
    do
    {
        result.push_back(BuildActivityEntry(qr->Fetch()));
    } while (qr->NextRow());
    return result;
}

} // namespace integration
} // namespace living_world
