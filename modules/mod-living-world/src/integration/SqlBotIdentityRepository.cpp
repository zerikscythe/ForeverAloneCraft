#include "integration/SqlBotIdentityRepository.h"
#include "integration/BotActivityLog.h"
#include "model/BotSpecKey.h"
#include "service/WorldBotGearBand.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "QueryResult.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <random>

namespace
{
std::string QuoteCharactersString(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::string CanonicalizeWorldBotPersonalityKey(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value.empty() || value == "indifferent" || value == "uninterested")
        return "uninterested";
    if (value == "cautious" || value == "opportunistic")
        return "opportunistic";
    if (value == "aggressive")
        return "aggressive";
    if (value == "coward")
        return "coward";
    return "uninterested";
}

std::string CanonicalizePopulationRole(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value == "city_reserve")
        return "city_reserve";
    return "world";
}

bool IsPvPLoadoutKey(std::string const& loadoutKey)
{
    if (loadoutKey.empty())
        return false;

    std::string normalized = loadoutKey;
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    return normalized.find("_PVP_") != std::string::npos
        || normalized.ends_with("_PVP")
        || normalized.starts_with("PVP_");
}

double GetLoadoutPersonalityWeight(living_world::integration::BotIdentityRecord const& rec)
{
    if (!IsPvPLoadoutKey(rec.loadoutKey))
        return 1.0;

    if (rec.personalityKey == "aggressive")
        return 4.0;
    if (rec.personalityKey == "opportunistic")
        return 2.5;
    if (rec.personalityKey == "uninterested")
        return 0.75;
    if (rec.personalityKey == "coward")
        return 0.2;
    return 1.0;
}

void ApplyLoadoutSelectionBias(
    std::vector<living_world::integration::BotIdentityRecord>& results,
    std::uint32_t limit)
{
    if (limit == 0 || results.size() <= limit)
        return;

    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    struct WeightedCandidate
    {
        double key = 0.0;
        living_world::integration::BotIdentityRecord record;
    };

    std::vector<WeightedCandidate> weighted;
    weighted.reserve(results.size());
    for (auto& rec : results)
    {
        double const weight = std::max(0.0001, GetLoadoutPersonalityWeight(rec));
        double roll = dist(rng);
        if (roll <= 0.0)
            roll = std::numeric_limits<double>::min();

        WeightedCandidate candidate;
        candidate.key = std::pow(roll, 1.0 / weight);
        candidate.record = std::move(rec);
        weighted.push_back(std::move(candidate));
    }

    std::sort(
        weighted.begin(),
        weighted.end(),
        [](WeightedCandidate const& left, WeightedCandidate const& right)
        {
            return left.key > right.key;
        });

    results.clear();
    results.reserve(limit);
    for (std::size_t i = 0; i < weighted.size() && i < limit; ++i)
        results.push_back(std::move(weighted[i].record));
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
    std::string const& loadoutKey,
    std::string const& personalityKey,
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
        "(name, race_id, class_id, spec_key, loadout_key, faction, display_id, gender, level, gear_tier, personality_key, "
        "has_herbalism, has_mining, has_fishing, home_zone_id, home_anchor_point_key, home_bind_point_key, "
        "is_available, session_count, total_world_online_ms, "
        "world_online_ms_since_level, post_max_world_online_ms, active_world_session_ms, "
        "active_world_session_start, is_retired, successor_spawned, retired_at, last_seen_zone, last_seen_at) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, 1, 1, {}, 0, 0, 0, {}, {}, {}, 1, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL, NULL)",
        QuoteCharactersString(successorName),
        raceId,
        successorClassId,
        QuoteCharactersString(successorSpecKey),
        QuoteCharactersString(loadoutKey),
        faction,
        displayId,
        gender,
        QuoteCharactersString(CanonicalizeWorldBotPersonalityKey(personalityKey)),
        homeZoneId == 0 ? std::string("NULL") : std::to_string(homeZoneId),
        homeAnchorPointKey.empty() ? std::string("NULL") : QuoteCharactersString(homeAnchorPointKey),
        homeBindPointKey.empty() ? std::string("NULL") : QuoteCharactersString(homeBindPointKey));

    LOG_INFO("server.worldserver",
        "[LivingWorld] Spawned ledger successor for identity={} successor='{}' faction={} race={} class={} spec='{}' personality='{}' level=1",
        parentIdentityId,
        successorName,
        faction,
        raceId,
        successorClassId,
        successorSpecKey,
        CanonicalizeWorldBotPersonalityKey(personalityKey));
    return true;
}

std::string NormalizeRuntimeLedgerText(std::string value, std::size_t maxLength)
{
    if (value.size() > maxLength)
        value.resize(maxLength);
    return value;
}

living_world::integration::BotIdentityRecord ReadBotIdentityRecord(Field const* f)
{
    living_world::integration::BotIdentityRecord rec;
    rec.id           = f[0].Get<std::uint32_t>();
    rec.name         = f[1].Get<std::string>();
    rec.raceId       = f[2].Get<std::uint8_t>();
    rec.classId      = f[3].Get<std::uint8_t>();
    rec.specKey      = living_world::model::CanonicalizeBotSpecKey(f[4].Get<std::string>());
    rec.loadoutKey   = f[5].IsNull() ? "" : f[5].Get<std::string>();
    rec.faction      = f[6].Get<std::uint8_t>();
    rec.displayId    = f[7].Get<std::uint32_t>();
    rec.gender       = f[8].Get<std::uint8_t>();
    rec.level        = f[9].Get<std::uint8_t>();
    rec.gearTier     = f[10].Get<std::uint8_t>();
    rec.personalityKey = f[11].IsNull() ? "uninterested" : CanonicalizeWorldBotPersonalityKey(f[11].Get<std::string>());
    rec.hasHerbalism = f[12].Get<bool>();
    rec.hasMining    = f[13].Get<bool>();
    rec.hasFishing   = f[14].Get<bool>();
    rec.populationRole = f[15].IsNull() ? "world" : CanonicalizePopulationRole(f[15].Get<std::string>());
    rec.reserveCityZoneId = f[16].IsNull() ? 0u : f[16].Get<std::uint32_t>();
    rec.homeZoneId   = f[17].IsNull() ? 0u : f[17].Get<std::uint32_t>();
    rec.homeAnchorPointKey = f[18].IsNull() ? "" : f[18].Get<std::string>();
    rec.homeBindPointKey   = f[19].IsNull() ? "" : f[19].Get<std::string>();
    rec.sessionCount = f[20].Get<std::uint32_t>();
    rec.totalWorldOnlineMs = f[21].Get<std::uint64_t>();
    rec.worldOnlineMsSinceLevel = f[22].Get<std::uint64_t>();
    rec.postMaxWorldOnlineMs = f[23].Get<std::uint64_t>();
    rec.activeWorldSessionMs = f[24].Get<std::uint64_t>();
    rec.runtimeState = f[25].IsNull() ? "" : f[25].Get<std::string>();
    rec.runtimeDetail = f[26].IsNull() ? "" : f[26].Get<std::string>();
    rec.lastSessionSourceKind = f[27].IsNull() ? "" : f[27].Get<std::string>();
    rec.lastSessionSourceKey = f[28].IsNull() ? "" : f[28].Get<std::string>();
    rec.lastTaskFamily = f[29].IsNull() ? "" : f[29].Get<std::string>();
    rec.lastTaskTargetZoneId = f[30].IsNull() ? 0u : f[30].Get<std::uint32_t>();
    rec.gearRefreshPending = f[31].Get<bool>();
    rec.lastGearRefreshBand = f[32].Get<std::uint8_t>();
    rec.lastSeenZoneId = f[33].IsNull() ? 0u : f[33].Get<std::uint32_t>();
    rec.isRetired    = f[34].Get<bool>();
    rec.isAvailable  = f[35].Get<bool>();
    return rec;
}
} // namespace

namespace living_world
{
namespace integration
{

void SqlBotIdentityRepository::EnsureSchema() const
{
    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'population_role'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN population_role VARCHAR(32) NOT NULL DEFAULT 'world' AFTER has_fishing");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'reserve_city_zone_id'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN reserve_city_zone_id INT UNSIGNED NULL AFTER population_role");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'runtime_state'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN runtime_state VARCHAR(64) NOT NULL DEFAULT '' AFTER active_world_session_ms");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'runtime_detail'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN runtime_detail VARCHAR(255) NOT NULL DEFAULT '' AFTER runtime_state");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_session_source_kind'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_session_source_kind VARCHAR(64) NOT NULL DEFAULT '' AFTER runtime_detail");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_session_source_key'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_session_source_key VARCHAR(128) NOT NULL DEFAULT '' AFTER last_session_source_kind");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_task_family'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_task_family VARCHAR(32) NOT NULL DEFAULT '' AFTER last_session_source_key");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_task_target_zone'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_task_target_zone INT UNSIGNED NULL AFTER last_task_family");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'gear_refresh_pending'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN gear_refresh_pending TINYINT(1) NOT NULL DEFAULT 1 AFTER active_world_session_ms");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_gear_refresh_band'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_gear_refresh_band TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER gear_refresh_pending");
    }

    if (!CharacterDatabase.Query(
        "SHOW INDEX FROM living_world_bot_identity WHERE Key_name = 'idx_population_role'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD INDEX idx_population_role (population_role, reserve_city_zone_id, faction, is_available, is_retired)");
    }
}

std::optional<BotIdentityRecord> SqlBotIdentityRepository::FindById(std::uint32_t id) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
        "gender, level, gear_tier, personality_key, has_herbalism, has_mining, has_fishing, "
        "population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key, "
        "session_count, total_world_online_ms, world_online_ms_since_level, "
        "post_max_world_online_ms, active_world_session_ms, runtime_state, runtime_detail, "
        "last_session_source_kind, last_session_source_key, last_task_family, last_task_target_zone, "
        "gear_refresh_pending, last_gear_refresh_band, last_seen_zone, is_retired, is_available "
        "FROM living_world_bot_identity "
        "WHERE id = {}",
        id);

    if (!result)
        return std::nullopt;

    return ReadBotIdentityRecord(result->Fetch());
}

std::optional<BotIdentityRecord> SqlBotIdentityRepository::FindByName(std::string const& name) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
        "gender, level, gear_tier, personality_key, has_herbalism, has_mining, has_fishing, "
        "population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key, "
        "session_count, total_world_online_ms, world_online_ms_since_level, "
        "post_max_world_online_ms, active_world_session_ms, runtime_state, runtime_detail, "
        "last_session_source_kind, last_session_source_key, last_task_family, last_task_target_zone, "
        "gear_refresh_pending, last_gear_refresh_band, last_seen_zone, is_retired, is_available "
        "FROM living_world_bot_identity "
        "WHERE name = {}",
        QuoteCharactersString(name));

    if (!result)
        return std::nullopt;

    return ReadBotIdentityRecord(result->Fetch());
}

std::vector<BotIdentityRecord> SqlBotIdentityRepository::LoadAvailable(
    std::uint8_t  faction,
    std::uint32_t limit) const
{
    std::vector<BotIdentityRecord> results;
    if (limit == 0)
        return results;

    std::uint32_t const candidateLimit = std::min<std::uint32_t>(
        std::max<std::uint32_t>(limit, limit * 4),
        256u);

    QueryResult result;
    if (faction == 0)
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
            "gender, level, gear_tier, personality_key, has_herbalism, has_mining, has_fishing, "
            "population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "session_count, total_world_online_ms, world_online_ms_since_level, "
            "post_max_world_online_ms, active_world_session_ms, runtime_state, runtime_detail, "
            "last_session_source_kind, last_session_source_key, last_task_family, last_task_target_zone, "
            "gear_refresh_pending, last_gear_refresh_band, last_seen_zone, is_retired, is_available "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 "
            "AND population_role = 'world' "
            "ORDER BY RAND() LIMIT {}",
            candidateLimit);
    }
    else
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
            "gender, level, gear_tier, personality_key, has_herbalism, has_mining, has_fishing, "
            "population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "session_count, total_world_online_ms, world_online_ms_since_level, "
            "post_max_world_online_ms, active_world_session_ms, runtime_state, runtime_detail, "
            "last_session_source_kind, last_session_source_key, last_task_family, last_task_target_zone, "
            "gear_refresh_pending, last_gear_refresh_band, last_seen_zone, is_retired, is_available "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 AND faction = {} "
            "AND population_role = 'world' "
            "ORDER BY RAND() LIMIT {}",
            faction, candidateLimit);
    }

    if (!result)
        return results;

    do
    {
        BotIdentityRecord rec = ReadBotIdentityRecord(result->Fetch());
        results.push_back(std::move(rec));
    } while (result->NextRow());

    ApplyLoadoutSelectionBias(results, limit);

    return results;
}

std::vector<BotIdentityRecord> SqlBotIdentityRepository::LoadAvailableReserveForCity(
    std::uint32_t reserveCityZoneId,
    std::uint8_t  faction,
    std::uint32_t limit) const
{
    std::vector<BotIdentityRecord> results;
    if (limit == 0 || reserveCityZoneId == 0)
        return results;

    std::uint32_t const candidateLimit = std::min<std::uint32_t>(
        std::max<std::uint32_t>(limit, limit * 4),
        256u);

    QueryResult result;
    if (faction == 0)
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
            "gender, level, gear_tier, personality_key, has_herbalism, has_mining, has_fishing, "
            "population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "session_count, total_world_online_ms, world_online_ms_since_level, "
            "post_max_world_online_ms, active_world_session_ms, runtime_state, runtime_detail, "
            "last_session_source_kind, last_session_source_key, last_task_family, last_task_target_zone, "
            "gear_refresh_pending, last_gear_refresh_band, last_seen_zone, is_retired, is_available "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 "
            "AND population_role = 'city_reserve' AND reserve_city_zone_id = {} "
            "ORDER BY RAND() LIMIT {}",
            reserveCityZoneId, candidateLimit);
    }
    else
    {
        result = CharacterDatabase.Query(
            "SELECT id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
            "gender, level, gear_tier, personality_key, has_herbalism, has_mining, has_fishing, "
            "population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "session_count, total_world_online_ms, world_online_ms_since_level, "
            "post_max_world_online_ms, active_world_session_ms, runtime_state, runtime_detail, "
            "last_session_source_kind, last_session_source_key, last_task_family, last_task_target_zone, "
            "gear_refresh_pending, last_gear_refresh_band, last_seen_zone, is_retired, is_available "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 AND faction = {} "
            "AND population_role = 'city_reserve' AND reserve_city_zone_id = {} "
            "ORDER BY RAND() LIMIT {}",
            faction, reserveCityZoneId, candidateLimit);
    }

    if (!result)
        return results;

    do
    {
        BotIdentityRecord rec = ReadBotIdentityRecord(result->Fetch());
        results.push_back(std::move(rec));
    } while (result->NextRow());

    ApplyLoadoutSelectionBias(results, limit);

    return results;
}

void SqlBotIdentityRepository::MarkActive(std::uint32_t id) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 0, session_count = session_count + 1, "
        "active_world_session_ms = 0, active_world_session_start = NOW(), "
        "runtime_state = '', runtime_detail = '' "
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
        "active_world_session_ms = 0, active_world_session_start = NULL, "
        "runtime_state = '', runtime_detail = '' "
        "WHERE id = {}",
        lastSeenZoneId, id);
}

void SqlBotIdentityRepository::UpdateGearRefreshState(
    std::uint32_t id,
    bool gearRefreshPending,
    std::uint8_t lastGearRefreshBand) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET gear_refresh_pending = {}, last_gear_refresh_band = {} "
        "WHERE id = {}",
        gearRefreshPending ? 1 : 0,
        lastGearRefreshBand,
        id);
}

void SqlBotIdentityRepository::UpdateActiveRuntimeState(
    std::uint32_t id,
    std::uint32_t zoneId,
    std::uint64_t activeWorldSessionMs,
    std::string const& runtimeState,
    std::string const& runtimeDetail) const
{
    std::string const normalizedState =
        QuoteCharactersString(NormalizeRuntimeLedgerText(runtimeState, 64));
    std::string const normalizedDetail =
        QuoteCharactersString(NormalizeRuntimeLedgerText(runtimeDetail, 255));

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET active_world_session_ms = {}, last_seen_zone = {}, "
        "runtime_state = {}, runtime_detail = {} "
        "WHERE id = {} AND is_available = 0",
        activeWorldSessionMs,
        zoneId == 0 ? std::string("NULL") : std::to_string(zoneId),
        normalizedState,
        normalizedDetail,
        id);
}

void SqlBotIdentityRepository::CompleteWorldSession(
    std::uint32_t id,
    std::uint32_t lastSeenZoneId,
    std::uint64_t sessionWorldOnlineMs,
    std::string const& lastSessionSourceKind,
    std::string const& lastSessionSourceKey,
    std::string const& lastTaskFamily,
    std::uint32_t lastTaskTargetZoneId) const
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
        "SELECT name, level, total_world_online_ms, world_online_ms_since_level, "
        "post_max_world_online_ms, is_retired, successor_spawned, race_id, class_id, spec_key, loadout_key, personality_key, faction, display_id, gender, gear_refresh_pending, "
        "home_zone_id, home_anchor_point_key, home_bind_point_key "
        "FROM living_world_bot_identity WHERE id = {}",
        id);

    if (!result)
        return;

    Field const* f = result->Fetch();
    std::string const name = f[0].Get<std::string>();
    std::uint8_t level = f[1].Get<std::uint8_t>();
    std::uint64_t totalWorldOnlineMs = f[2].Get<std::uint64_t>();
    std::uint64_t worldOnlineMsSinceLevel = f[3].Get<std::uint64_t>();
    std::uint64_t postMaxWorldOnlineMs = f[4].Get<std::uint64_t>();
    bool isRetired = f[5].Get<bool>();
    bool successorSpawned = f[6].Get<bool>();
    std::uint8_t raceId = f[7].Get<std::uint8_t>();
    std::uint8_t classId = f[8].Get<std::uint8_t>();
    std::string specKey = living_world::model::CanonicalizeBotSpecKey(f[9].Get<std::string>());
    std::string loadoutKey = f[10].IsNull() ? "" : f[10].Get<std::string>();
    std::string personalityKey = f[11].IsNull() ? "uninterested" : CanonicalizeWorldBotPersonalityKey(f[11].Get<std::string>());
    std::uint8_t faction = f[12].Get<std::uint8_t>();
    std::uint32_t displayId = f[13].Get<std::uint32_t>();
    std::uint8_t gender = f[14].Get<std::uint8_t>();
    bool gearRefreshPending = f[15].Get<bool>();
    std::uint32_t homeZoneId = f[16].IsNull() ? 0u : f[16].Get<std::uint32_t>();
    std::string homeAnchorPointKey = f[17].IsNull() ? "" : f[17].Get<std::string>();
    std::string homeBindPointKey = f[18].IsNull() ? "" : f[18].Get<std::string>();

    if (isRetired)
    {
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET is_available = 0, last_seen_zone = {}, last_seen_at = NOW(), "
            "active_world_session_ms = 0, active_world_session_start = NULL, "
            "runtime_state = '', runtime_detail = '' "
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
                    loadoutKey,
                    personalityKey,
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

    std::uint8_t const oldGearBand = service::ComputeWorldBotGearRefreshBand(level);
    std::uint8_t const newGearBand = service::ComputeWorldBotGearRefreshBand(newLevel);
    bool const crossedGearBand = newGearBand != oldGearBand;
    bool const newGearRefreshPending = gearRefreshPending || crossedGearBand;
    bool retireNow = newLevel >= MaxBotLevel && newPostMax >= RetirementGraceMs;
    std::string const normalizedSourceKind =
        NormalizeRuntimeLedgerText(lastSessionSourceKind, 64);
    std::string const normalizedSourceKey =
        NormalizeRuntimeLedgerText(lastSessionSourceKey, 128);
    std::string const normalizedTaskFamily =
        NormalizeRuntimeLedgerText(lastTaskFamily, 32);

    std::string const progressDetail =
        "personality='" + personalityKey
        + "' spec='" + specKey
        + "' old_level=" + std::to_string(level)
        + " new_level=" + std::to_string(newLevel)
        + " session_world_online_ms=" + std::to_string(sessionWorldOnlineMs)
        + " total_world_online_ms=" + std::to_string(newTotal)
        + " world_online_ms_since_level=" + std::to_string(newSinceLevel)
        + " post_max_world_online_ms=" + std::to_string(newPostMax)
        + " gear_refresh_pending=" + std::to_string(newGearRefreshPending ? 1 : 0)
        + " gear_refresh_band=" + std::to_string(newGearBand)
        + " rebuild_on_next_spawn=" + std::to_string(newLevel != level ? 1 : 0)
        + " retired=" + std::to_string(retireNow ? 1 : 0);

    LOG_INFO("server.worldserver",
        "[LivingWorld] WorldBotLedgerProgress identity={} name='{}' {}",
        id,
        name,
        progressDetail);

    if (newLevel != level)
    {
        living_world::integration::BotActivityLog::RecordAbstract(
            name,
            id,
            "ledger_level_up",
            progressDetail,
            0,
            lastSeenZoneId,
            0.0f,
            0.0f,
            0.0f);
    }

    if (newSuccessorSpawned && !successorSpawned)
    {
        living_world::integration::BotActivityLog::RecordAbstract(
            name,
            id,
            "ledger_successor_spawned",
            "personality='" + personalityKey + "' spec='" + specKey + "' level=" + std::to_string(newLevel),
            0,
            lastSeenZoneId,
            0.0f,
            0.0f,
            0.0f);
    }

    if (retireNow)
    {
        living_world::integration::BotActivityLog::RecordAbstract(
            name,
            id,
            "ledger_retired",
            progressDetail,
            0,
            lastSeenZoneId,
            0.0f,
            0.0f,
            0.0f);
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET level = {}, total_world_online_ms = {}, "
            "world_online_ms_since_level = {}, post_max_world_online_ms = {}, successor_spawned = {}, gear_refresh_pending = {}, "
            "is_available = 0, is_retired = 1, retired_at = NOW(), "
            "last_seen_zone = {}, last_seen_at = NOW(), "
            "last_session_source_kind = {}, last_session_source_key = {}, last_task_family = {}, last_task_target_zone = {}, "
            "active_world_session_ms = 0, active_world_session_start = NULL, "
            "runtime_state = '', runtime_detail = '' "
            "WHERE id = {}",
            newLevel, newTotal, newSinceLevel, newPostMax, newSuccessorSpawned, newGearRefreshPending,
            lastSeenZoneId,
            QuoteCharactersString(normalizedSourceKind),
            QuoteCharactersString(normalizedSourceKey),
            QuoteCharactersString(normalizedTaskFamily),
            lastTaskTargetZoneId == 0 ? std::string("NULL") : std::to_string(lastTaskTargetZoneId),
            id);
        return;
    }

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET level = {}, total_world_online_ms = {}, "
        "world_online_ms_since_level = {}, post_max_world_online_ms = {}, successor_spawned = {}, gear_refresh_pending = {}, "
        "is_available = 1, is_retired = 0, retired_at = NULL, "
        "last_seen_zone = {}, last_seen_at = NOW(), "
        "last_session_source_kind = {}, last_session_source_key = {}, last_task_family = {}, last_task_target_zone = {}, "
        "active_world_session_ms = 0, active_world_session_start = NULL, "
        "runtime_state = '', runtime_detail = '' "
        "WHERE id = {}",
        newLevel, newTotal, newSinceLevel, newPostMax, newSuccessorSpawned, newGearRefreshPending,
        lastSeenZoneId,
        QuoteCharactersString(normalizedSourceKind),
        QuoteCharactersString(normalizedSourceKey),
        QuoteCharactersString(normalizedTaskFamily),
        lastTaskTargetZoneId == 0 ? std::string("NULL") : std::to_string(lastTaskTargetZoneId),
        id);
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
        "active_world_session_ms = 0, active_world_session_start = NULL, "
        "runtime_state = '', runtime_detail = '' "
        "WHERE is_available = 0 AND is_retired = 0");

    return recovered;
}

} // namespace integration
} // namespace living_world
