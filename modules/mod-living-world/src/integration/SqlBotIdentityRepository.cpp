#include "integration/SqlBotIdentityRepository.h"
#include "model/BotSpecKey.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "QueryResult.h"

#include <array>
#include <random>

namespace
{
std::string QuoteCharactersString(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::string MakeSuccessorName(std::uint8_t raceId, std::uint32_t parentIdentityId)
{
    static constexpr std::array<std::string_view, 25> HumanNames = {
        "Marcus", "Elena", "Thomas", "Claire", "Roland", "Sera", "Aldric", "Mira", "Gareth", "Lena",
        "Oswin", "Tara", "Bram", "Nessa", "Hugo", "Alys", "Corwin", "Delia", "Emric", "Fiona",
        "Hadwin", "Isolde", "Joric", "Kira", "Lewin"
    };
    static constexpr std::array<std::string_view, 16> OrcNames = {
        "Grak", "Thruk", "Morg", "Draka", "Vorn", "Kurg", "Raka", "Thok",
        "Brolgur", "Darkjaw", "Gorefist", "Ironscar", "Krom", "Lukar", "Malgok", "Narak"
    };
    static constexpr std::array<std::string_view, 16> DwarfNames = {
        "Bronk", "Thora", "Gimble", "Dugal", "Bera", "Thordin", "Kelga", "Rimdar",
        "Agna", "Borik", "Gunda", "Ulfar", "Snorra", "Dvallin", "Frika", "Hegir"
    };
    static constexpr std::array<std::string_view, 14> NightElfNames = {
        "Malfas", "Tyrenna", "Shal", "Elandir", "Dusk", "Moonfang", "Ashwhisper",
        "Celaen", "Leafsong", "Silvara", "Stormclaw", "Vanya", "Wynnara", "Zephyr"
    };
    static constexpr std::array<std::string_view, 13> UndeadNames = {
        "Mors", "Vellus", "Shade", "Grimwald", "Cryptar", "Gashmore", "Pallor",
        "Rotwick", "Sallow", "Tenebre", "Wormtongue", "Bleakhaven", "Deathmere"
    };
    static constexpr std::array<std::string_view, 12> TaurenNames = {
        "Hamuul", "Mornehoof", "Tarnis", "Bainestone", "Greathorn", "Earthshaker",
        "Stonehoof", "Swiftwind", "Thunderhoof", "Skydancer", "Ironhorn", "Duskmane"
    };
    static constexpr std::array<std::string_view, 14> GnomeNames = {
        "Fizz", "Cogsworth", "Tinkle", "Zapwick", "Nimbolt", "Sprocket", "Gizmo",
        "Whirly", "Clanksworth", "Doodad", "Fizzpop", "Glimmer", "Hacksaw", "Inkwhistle"
    };
    static constexpr std::array<std::string_view, 14> TrollNames = {
        "Zul", "Vol", "Jinrak", "Raxsha", "Kazzan", "Shadtusk", "Ziplax", "Bogtusk",
        "Darkfang", "Hexveil", "Mudcloth", "Razorbeak", "Skullsplitter", "Trollheim"
    };
    static constexpr std::array<std::string_view, 16> BloodElfNames = {
        "Arano", "Sylviel", "Kaelion", "Dawnblade", "Sunwhisper", "Aelindra", "Brightmantle", "Crimsonthorn",
        "Duskshroud", "Evelaith", "Goldmane", "Hawkspire", "Illyria", "Jadewing", "Keldorei", "Lunarglow"
    };
    static constexpr std::array<std::string_view, 14> DraeneiNames = {
        "Akama", "Veleth", "Kirana", "Azuremist", "Sorel", "Caiel", "Drakoris",
        "Elodra", "Faeron", "Galadar", "Holytear", "Imari", "Jaina", "Khanaros"
    };

    auto pickBase = [raceId]() -> std::string_view
    {
        static thread_local std::mt19937 rng{ std::random_device{}() };

        switch (raceId)
        {
            case 1: return HumanNames[rng() % HumanNames.size()];
            case 2: return OrcNames[rng() % OrcNames.size()];
            case 3: return DwarfNames[rng() % DwarfNames.size()];
            case 4: return NightElfNames[rng() % NightElfNames.size()];
            case 5: return UndeadNames[rng() % UndeadNames.size()];
            case 6: return TaurenNames[rng() % TaurenNames.size()];
            case 7: return GnomeNames[rng() % GnomeNames.size()];
            case 8: return TrollNames[rng() % TrollNames.size()];
            case 10: return BloodElfNames[rng() % BloodElfNames.size()];
            case 11: return DraeneiNames[rng() % DraeneiNames.size()];
            default: return HumanNames[rng() % HumanNames.size()];
        }
    };

    std::string name = std::string(pickBase()) + "Line" + std::to_string(parentIdentityId);
    if (name.size() > 32)
        name.resize(32);
    return name;
}

bool CreateLevelOneSuccessor(
    std::uint32_t parentIdentityId,
    std::uint8_t raceId,
    std::uint8_t classId,
    std::string const& specKey,
    std::uint8_t faction,
    std::uint32_t displayId,
    std::uint8_t gender,
    std::uint32_t homeZoneId,
    std::string const& homeAnchorPointKey,
    std::string const& homeBindPointKey)
{
    std::uint8_t successorClassId = classId;
    std::string successorSpecKey = living_world::model::CanonicalizeBotSpecKey(specKey);

    if (successorClassId == 6)
    {
        successorClassId = 1;
        successorSpecKey = "Arms";
    }

    std::string successorName = MakeSuccessorName(raceId, parentIdentityId);

    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_identity "
        "(name, race_id, class_id, spec_key, faction, display_id, gender, level, gear_tier, "
        "has_herbalism, has_mining, has_fishing, home_zone_id, home_anchor_point_key, home_bind_point_key, "
        "is_available, session_count, total_world_online_ms, "
        "world_online_ms_since_level, post_max_world_online_ms, active_world_session_ms, "
        "active_world_session_start, is_retired, successor_spawned, retired_at, last_seen_zone, last_seen_at) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, 1, 1, 0, 0, 0, {}, {}, {}, 1, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL, NULL)",
        QuoteCharactersString(successorName),
        raceId,
        successorClassId,
        QuoteCharactersString(successorSpecKey),
        faction,
        displayId,
        gender,
        homeZoneId == 0 ? std::string("NULL") : std::to_string(homeZoneId),
        homeAnchorPointKey.empty() ? std::string("NULL") : QuoteCharactersString(homeAnchorPointKey),
        homeBindPointKey.empty() ? std::string("NULL") : QuoteCharactersString(homeBindPointKey));

    LOG_INFO("server.worldserver",
        "[LivingWorld] Spawned ledger successor for identity={} successor='{}' faction={} race={} class={} spec='{}' level=1",
        parentIdentityId,
        successorName,
        faction,
        raceId,
        successorClassId,
        successorSpecKey);
    return true;
}
} // namespace

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
            "home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "session_count, total_world_online_ms, world_online_ms_since_level, "
            "post_max_world_online_ms, active_world_session_ms, last_seen_zone, is_retired "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 "
            "ORDER BY RAND() LIMIT {}",
            limit);
    }
    else
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, faction, display_id, "
            "gender, level, gear_tier, has_herbalism, has_mining, has_fishing, "
            "home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "session_count, total_world_online_ms, world_online_ms_since_level, "
            "post_max_world_online_ms, active_world_session_ms, last_seen_zone, is_retired "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 AND faction = {} "
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
        rec.specKey      = living_world::model::CanonicalizeBotSpecKey(f[4].Get<std::string>());
        rec.faction      = f[5].Get<std::uint8_t>();
        rec.displayId    = f[6].Get<std::uint32_t>();
        rec.gender       = f[7].Get<std::uint8_t>();
        rec.level        = f[8].Get<std::uint8_t>();
        rec.gearTier     = f[9].Get<std::uint8_t>();
        rec.hasHerbalism = f[10].Get<bool>();
        rec.hasMining    = f[11].Get<bool>();
        rec.hasFishing   = f[12].Get<bool>();
        rec.homeZoneId   = f[13].IsNull() ? 0u : f[13].Get<std::uint32_t>();
        rec.homeAnchorPointKey = f[14].IsNull() ? "" : f[14].Get<std::string>();
        rec.homeBindPointKey   = f[15].IsNull() ? "" : f[15].Get<std::string>();
        rec.sessionCount = f[16].Get<std::uint32_t>();
        rec.totalWorldOnlineMs = f[17].Get<std::uint64_t>();
        rec.worldOnlineMsSinceLevel = f[18].Get<std::uint64_t>();
        rec.postMaxWorldOnlineMs = f[19].Get<std::uint64_t>();
        rec.activeWorldSessionMs = f[20].Get<std::uint64_t>();
        rec.lastSeenZoneId = f[21].IsNull() ? 0u : f[21].Get<std::uint32_t>();
        rec.isRetired    = f[22].Get<bool>();
        results.push_back(std::move(rec));
    } while (result->NextRow());

    return results;
}

void SqlBotIdentityRepository::MarkActive(std::uint32_t id) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 0, session_count = session_count + 1, "
        "active_world_session_ms = 0, active_world_session_start = NOW() "
        "WHERE id = {}",
        id);
}

void SqlBotIdentityRepository::MarkAvailable(
    std::uint32_t id,
    std::uint32_t lastSeenZoneId) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 1, last_seen_zone = {}, last_seen_at = NOW(), "
        "active_world_session_ms = 0, active_world_session_start = NULL "
        "WHERE id = {}",
        lastSeenZoneId, id);
}

void SqlBotIdentityRepository::CompleteWorldSession(
    std::uint32_t id,
    std::uint32_t lastSeenZoneId,
    std::uint64_t sessionWorldOnlineMs) const
{
    if (sessionWorldOnlineMs == 0)
    {
        MarkAvailable(id, lastSeenZoneId);
        return;
    }

    constexpr std::uint8_t MaxBotLevel = 80;
    constexpr std::uint64_t LevelHourMs = 60ull * 60ull * 1000ull;
    constexpr std::uint64_t RetirementGraceMs = 50ull * LevelHourMs;

    QueryResult result = CharacterDatabase.Query(
        "SELECT level, total_world_online_ms, world_online_ms_since_level, "
        "post_max_world_online_ms, is_retired, successor_spawned, race_id, class_id, spec_key, faction, display_id, gender, "
        "home_zone_id, home_anchor_point_key, home_bind_point_key "
        "FROM living_world_bot_identity WHERE id = {}",
        id);

    if (!result)
        return;

    Field const* f = result->Fetch();
    std::uint8_t level = f[0].Get<std::uint8_t>();
    std::uint64_t totalWorldOnlineMs = f[1].Get<std::uint64_t>();
    std::uint64_t worldOnlineMsSinceLevel = f[2].Get<std::uint64_t>();
    std::uint64_t postMaxWorldOnlineMs = f[3].Get<std::uint64_t>();
    bool isRetired = f[4].Get<bool>();
    bool successorSpawned = f[5].Get<bool>();
    std::uint8_t raceId = f[6].Get<std::uint8_t>();
    std::uint8_t classId = f[7].Get<std::uint8_t>();
    std::string specKey = living_world::model::CanonicalizeBotSpecKey(f[8].Get<std::string>());
    std::uint8_t faction = f[9].Get<std::uint8_t>();
    std::uint32_t displayId = f[10].Get<std::uint32_t>();
    std::uint8_t gender = f[11].Get<std::uint8_t>();
    std::uint32_t homeZoneId = f[12].IsNull() ? 0u : f[12].Get<std::uint32_t>();
    std::string homeAnchorPointKey = f[13].IsNull() ? "" : f[13].Get<std::string>();
    std::string homeBindPointKey = f[14].IsNull() ? "" : f[14].Get<std::string>();

    if (isRetired)
    {
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET is_available = 0, last_seen_zone = {}, last_seen_at = NOW(), "
            "active_world_session_ms = 0, active_world_session_start = NULL "
            "WHERE id = {}",
            lastSeenZoneId, id);
        return;
    }

    std::uint64_t newTotal = totalWorldOnlineMs + sessionWorldOnlineMs;
    std::uint8_t newLevel = level;
    std::uint64_t newSinceLevel = worldOnlineMsSinceLevel;
    std::uint64_t newPostMax = postMaxWorldOnlineMs;
    bool newSuccessorSpawned = successorSpawned;

    if (newLevel < MaxBotLevel)
    {
        newSinceLevel += sessionWorldOnlineMs;

        while (newLevel < MaxBotLevel && newSinceLevel >= LevelHourMs)
        {
            newSinceLevel -= LevelHourMs;
            ++newLevel;
        }

        if (newLevel >= MaxBotLevel)
        {
            newPostMax += newSinceLevel;
            newSinceLevel = 0;

            if (!newSuccessorSpawned)
            {
                newSuccessorSpawned = CreateLevelOneSuccessor(
                    id,
                    raceId,
                    classId,
                    specKey,
                    faction,
                    displayId,
                    gender,
                    homeZoneId,
                    homeAnchorPointKey,
                    homeBindPointKey);
            }
        }
    }
    else
    {
        newPostMax += sessionWorldOnlineMs;
    }

    bool retireNow = newLevel >= MaxBotLevel && newPostMax >= RetirementGraceMs;

    if (retireNow)
    {
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET level = {}, total_world_online_ms = {}, "
            "world_online_ms_since_level = {}, post_max_world_online_ms = {}, successor_spawned = {}, "
            "is_available = 0, is_retired = 1, retired_at = NOW(), "
            "last_seen_zone = {}, last_seen_at = NOW(), "
            "active_world_session_ms = 0, active_world_session_start = NULL "
            "WHERE id = {}",
            newLevel, newTotal, newSinceLevel, newPostMax, newSuccessorSpawned, lastSeenZoneId, id);
        return;
    }

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET level = {}, total_world_online_ms = {}, "
        "world_online_ms_since_level = {}, post_max_world_online_ms = {}, successor_spawned = {}, "
        "is_available = 1, is_retired = 0, retired_at = NULL, "
        "last_seen_zone = {}, last_seen_at = NOW(), "
        "active_world_session_ms = 0, active_world_session_start = NULL "
        "WHERE id = {}",
        newLevel, newTotal, newSinceLevel, newPostMax, newSuccessorSpawned, lastSeenZoneId, id);
}

std::uint32_t SqlBotIdentityRepository::RecoverStaleActiveSessions() const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM living_world_bot_identity "
        "WHERE is_available = 0 AND is_retired = 0");

    std::uint32_t recovered = 0;
    if (result)
        recovered = result->Fetch()[0].Get<std::uint32_t>();

    if (recovered == 0)
        return 0;

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 1, "
        "active_world_session_ms = 0, active_world_session_start = NULL "
        "WHERE is_available = 0 AND is_retired = 0");

    return recovered;
}

} // namespace integration
} // namespace living_world
