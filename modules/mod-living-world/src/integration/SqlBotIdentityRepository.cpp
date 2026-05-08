#include "integration/SqlBotIdentityRepository.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{

std::vector<BotIdentityRecord> SqlBotIdentityRepository::LoadAvailable(
    std::uint8_t  faction,
    std::uint32_t limit) const
{
    std::vector<BotIdentityRecord> results;

    QueryResult result;
    if (faction == 0)
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, faction, display_id, "
            "gender, level, gear_tier, has_herbalism, has_mining, has_fishing, "
            "session_count "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 "
            "ORDER BY RAND() LIMIT {}",
            limit);
    }
    else
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, faction, display_id, "
            "gender, level, gear_tier, has_herbalism, has_mining, has_fishing, "
            "session_count "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND faction = {} "
            "ORDER BY RAND() LIMIT {}",
            faction, limit);
    }

    if (!result)
        return results;

    do
    {
        Field const* f = result->Fetch();
        BotIdentityRecord rec;
        rec.id           = f[0].Get<std::uint32_t>();
        rec.name         = f[1].Get<std::string>();
        rec.raceId       = f[2].Get<std::uint8_t>();
        rec.classId      = f[3].Get<std::uint8_t>();
        rec.specKey      = f[4].Get<std::string>();
        rec.faction      = f[5].Get<std::uint8_t>();
        rec.displayId    = f[6].Get<std::uint32_t>();
        rec.gender       = f[7].Get<std::uint8_t>();
        rec.level        = f[8].Get<std::uint8_t>();
        rec.gearTier     = f[9].Get<std::uint8_t>();
        rec.hasHerbalism = f[10].Get<bool>();
        rec.hasMining    = f[11].Get<bool>();
        rec.hasFishing   = f[12].Get<bool>();
        rec.sessionCount = f[13].Get<std::uint32_t>();
        results.push_back(std::move(rec));
    } while (result->NextRow());

    return results;
}

void SqlBotIdentityRepository::MarkActive(std::uint32_t id) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 0, session_count = session_count + 1 "
        "WHERE id = {}",
        id);
}

void SqlBotIdentityRepository::MarkAvailable(
    std::uint32_t id,
    std::uint32_t lastSeenZoneId) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 1, last_seen_zone = {}, last_seen_at = NOW() "
        "WHERE id = {}",
        lastSeenZoneId, id);
}

} // namespace integration
} // namespace living_world
