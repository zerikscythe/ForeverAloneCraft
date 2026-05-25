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

std::string RollWorldBotSessionPersonalityKey(std::uint8_t level)
{
    // Session-scoped mood mix for active world bots:
    // - 65% uninterested
    // - 20% opportunistic/cautious
    // - 10% aggressive
    // -  5% coward
    //
    // Level 75+ bots never roll coward. We shift that 5% into uninterested so
    // high-end populations skew more toward non-PvP priorities unless a
    // specific task family later opts them into trouble.
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(1, 100);
    int const roll = dist(rng);

    if (level >= 75)
    {
        if (roll <= 70)
            return "uninterested";
        if (roll <= 90)
            return "opportunistic";
        return "aggressive";
    }

    if (roll <= 65)
        return "uninterested";
    if (roll <= 85)
        return "opportunistic";
    if (roll <= 95)
        return "aggressive";
    return "coward";
}

std::uint64_t RollWorldBotSessionBudgetMs()
{
    static thread_local std::mt19937 rng{ std::random_device{}() };
    // Session chunks run from 30 minutes up to 3 hours. Bots can still clock
    // out early if they run out of sensible chores before the budget expires.
    std::uniform_int_distribution<std::uint32_t> dist(
        30u * 60u * 1000u,
        3u * 60u * 60u * 1000u);
    return dist(rng);
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
        "has_herbalism, has_mining, has_fishing, home_zone_id, home_anchor_point_key, home_bind_point_key, generic_potion_charges, "
        "is_available, session_count, total_world_online_ms, "
        "world_online_ms_since_level, post_max_world_online_ms, active_world_session_ms, active_world_session_budget_ms, "
        "active_world_session_start, is_retired, successor_spawned, retired_at, last_seen_zone, last_seen_at) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, 1, 1, {}, 0, 0, 0, 0, {}, {}, {}, 1, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL, NULL)",
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

char const* GetBotIdentitySelectColumns()
{
    return
        "id, name, race_id, class_id, spec_key, loadout_key, faction, display_id, "
        "gender, display_loadout_key, doctrine_profile_key, level, gear_tier, personality_key, "
        "has_herbalism, has_mining, has_fishing, population_role, reserve_city_zone_id, ambient_group_id, "
        "ambient_group_leader_identity_id, ambient_group_role, home_zone_id, home_anchor_point_key, home_bind_point_key, "
        "generic_potion_charges, session_count, total_world_online_ms, world_online_ms_since_level, "
        "post_max_world_online_ms, active_world_session_ms, active_world_session_budget_ms, runtime_state, runtime_detail, "
        "shell_account_id, shell_character_guid, shell_state_version, pending_rebuild_reason, last_rehydrate_at, "
        "last_session_source_kind, last_session_source_key, last_task_family, last_task_activity_key, last_task_target_zone, "
        "last_quest_hub_key, last_quest_hub_elapsed_ms, gear_refresh_pending, last_gear_refresh_band, last_seen_zone, "
        "is_retired, is_available, skin, face, hair_style, hair_color, facial_style, appearance_resolved ";
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
    rec.displayLoadoutKey = f[9].IsNull() ? "" : f[9].Get<std::string>();
    rec.doctrineProfileKey = f[10].IsNull() ? "" : f[10].Get<std::string>();
    rec.level        = f[11].Get<std::uint8_t>();
    rec.gearTier     = f[12].Get<std::uint8_t>();
    rec.personalityKey = f[13].IsNull() ? "uninterested" : CanonicalizeWorldBotPersonalityKey(f[13].Get<std::string>());
    rec.hasHerbalism = f[14].Get<bool>();
    rec.hasMining    = f[15].Get<bool>();
    rec.hasFishing   = f[16].Get<bool>();
    rec.populationRole = f[17].IsNull() ? "world" : CanonicalizePopulationRole(f[17].Get<std::string>());
    rec.reserveCityZoneId = f[18].IsNull() ? 0u : f[18].Get<std::uint32_t>();
    rec.ambientGroupId = f[19].IsNull() ? 0u : f[19].Get<std::uint32_t>();
    rec.ambientGroupLeaderIdentityId = f[20].IsNull() ? 0u : f[20].Get<std::uint32_t>();
    rec.ambientGroupRole = f[21].IsNull() ? "" : f[21].Get<std::string>();
    rec.homeZoneId   = f[22].IsNull() ? 0u : f[22].Get<std::uint32_t>();
    rec.homeAnchorPointKey = f[23].IsNull() ? "" : f[23].Get<std::string>();
    rec.homeBindPointKey   = f[24].IsNull() ? "" : f[24].Get<std::string>();
    rec.genericPotionCharges = f[25].Get<std::uint8_t>();
    rec.sessionCount = f[26].Get<std::uint32_t>();
    rec.totalWorldOnlineMs = f[27].Get<std::uint64_t>();
    rec.worldOnlineMsSinceLevel = f[28].Get<std::uint64_t>();
    rec.postMaxWorldOnlineMs = f[29].Get<std::uint64_t>();
    rec.activeWorldSessionMs = f[30].Get<std::uint64_t>();
    rec.activeWorldSessionBudgetMs = f[31].Get<std::uint64_t>();
    rec.runtimeState = f[32].IsNull() ? "" : f[32].Get<std::string>();
    rec.runtimeDetail = f[33].IsNull() ? "" : f[33].Get<std::string>();
    rec.shellAccountId = f[34].IsNull() ? 0u : f[34].Get<std::uint32_t>();
    rec.shellCharacterGuid = f[35].IsNull() ? 0u : f[35].Get<std::uint64_t>();
    rec.shellStateVersion = f[36].IsNull() ? 0u : f[36].Get<std::uint32_t>();
    rec.pendingRebuildReason = f[37].IsNull() ? "" : f[37].Get<std::string>();
    rec.lastRehydrateAt = f[38].IsNull() ? "" : f[38].Get<std::string>();
    rec.lastSessionSourceKind = f[39].IsNull() ? "" : f[39].Get<std::string>();
    rec.lastSessionSourceKey = f[40].IsNull() ? "" : f[40].Get<std::string>();
    rec.lastTaskFamily = f[41].IsNull() ? "" : f[41].Get<std::string>();
    rec.lastTaskActivityKey = f[42].IsNull() ? "" : f[42].Get<std::string>();
    rec.lastTaskTargetZoneId = f[43].IsNull() ? 0u : f[43].Get<std::uint32_t>();
    rec.lastQuestHubKey = f[44].IsNull() ? "" : f[44].Get<std::string>();
    rec.lastQuestHubElapsedMs = f[45].Get<std::uint64_t>();
    rec.gearRefreshPending = f[46].Get<bool>();
    rec.lastGearRefreshBand = f[47].Get<std::uint8_t>();
    rec.lastSeenZoneId = f[48].IsNull() ? 0u : f[48].Get<std::uint32_t>();
    rec.isRetired    = f[49].Get<bool>();
    rec.isAvailable  = f[50].Get<bool>();
    rec.skin         = f[51].IsNull() ? 0u : f[51].Get<std::uint8_t>();
    rec.face         = f[52].IsNull() ? 0u : f[52].Get<std::uint8_t>();
    rec.hairStyle    = f[53].IsNull() ? 0u : f[53].Get<std::uint8_t>();
    rec.hairColor    = f[54].IsNull() ? 0u : f[54].Get<std::uint8_t>();
    rec.facialStyle  = f[55].IsNull() ? 0u : f[55].Get<std::uint8_t>();
    rec.appearanceResolved = f[56].Get<bool>();
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
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'ambient_group_id'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN ambient_group_id INT UNSIGNED NULL AFTER reserve_city_zone_id");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'ambient_group_leader_identity_id'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN ambient_group_leader_identity_id INT UNSIGNED NULL AFTER ambient_group_id");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'ambient_group_role'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN ambient_group_role VARCHAR(32) NOT NULL DEFAULT '' AFTER ambient_group_leader_identity_id");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'skin'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN skin TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER gender");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'face'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN face TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER skin");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'hair_style'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN hair_style TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER face");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'hair_color'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN hair_color TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER hair_style");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'facial_style'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN facial_style TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER hair_color");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'appearance_resolved'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN appearance_resolved TINYINT(1) NOT NULL DEFAULT 0 AFTER facial_style");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'display_loadout_key'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN display_loadout_key VARCHAR(64) NOT NULL DEFAULT '' AFTER gender");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'doctrine_profile_key'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN doctrine_profile_key VARCHAR(64) NOT NULL DEFAULT '' AFTER display_loadout_key");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'active_world_session_budget_ms'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN active_world_session_budget_ms BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER active_world_session_ms");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'runtime_state'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN runtime_state VARCHAR(64) NOT NULL DEFAULT '' AFTER active_world_session_budget_ms");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'runtime_detail'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN runtime_detail VARCHAR(255) NOT NULL DEFAULT '' AFTER runtime_state");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'shell_account_id'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN shell_account_id INT UNSIGNED NULL AFTER runtime_detail");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'shell_character_guid'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN shell_character_guid BIGINT UNSIGNED NULL AFTER shell_account_id");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'shell_state_version'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN shell_state_version INT UNSIGNED NOT NULL DEFAULT 0 AFTER shell_character_guid");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'pending_rebuild_reason'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN pending_rebuild_reason VARCHAR(64) NOT NULL DEFAULT '' AFTER shell_state_version");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_rehydrate_at'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_rehydrate_at DATETIME NULL AFTER pending_rebuild_reason");
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
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_task_activity_key'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_task_activity_key VARCHAR(128) NOT NULL DEFAULT '' AFTER last_task_family");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_task_target_zone'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_task_target_zone INT UNSIGNED NULL AFTER last_task_activity_key");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_quest_hub_key'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_quest_hub_key VARCHAR(128) NOT NULL DEFAULT '' AFTER last_task_target_zone");
    }

    if (!CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'last_quest_hub_elapsed_ms'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN last_quest_hub_elapsed_ms BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER last_quest_hub_key");
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
        "SHOW COLUMNS FROM living_world_bot_identity LIKE 'generic_potion_charges'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN generic_potion_charges TINYINT UNSIGNED NOT NULL DEFAULT 5 AFTER home_bind_point_key");
    }

    if (!CharacterDatabase.Query(
        "SHOW INDEX FROM living_world_bot_identity WHERE Key_name = 'idx_population_role'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD INDEX idx_population_role (population_role, reserve_city_zone_id, faction, is_available, is_retired)");
    }

    if (!CharacterDatabase.Query(
        "SHOW INDEX FROM living_world_bot_identity WHERE Key_name = 'idx_ambient_group'"))
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_identity "
            "ADD INDEX idx_ambient_group (ambient_group_id, ambient_group_leader_identity_id, is_available, is_retired)");
    }
}

std::optional<BotIdentityRecord> SqlBotIdentityRepository::FindById(std::uint32_t id) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT {} "
        "FROM living_world_bot_identity "
        "WHERE id = {}",
        GetBotIdentitySelectColumns(),
        id);

    if (!result)
        return std::nullopt;

    return ReadBotIdentityRecord(result->Fetch());
}

std::optional<BotIdentityRecord> SqlBotIdentityRepository::FindByName(std::string const& name) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT {} "
        "FROM living_world_bot_identity "
        "WHERE name = {}",
        GetBotIdentitySelectColumns(),
        QuoteCharactersString(name));

    if (!result)
        return std::nullopt;

    return ReadBotIdentityRecord(result->Fetch());
}

std::vector<BotIdentityRecord> SqlBotIdentityRepository::LoadAvailable(
    std::uint8_t  faction,
    std::uint32_t limit,
    std::uint8_t minLevel,
    std::uint8_t maxLevel) const
{
    std::vector<BotIdentityRecord> results;
    if (limit == 0)
        return results;

    std::uint32_t const candidateLimit = std::min<std::uint32_t>(
        std::max<std::uint32_t>(limit, limit * 4),
        256u);

    QueryResult result;
    std::string levelFilter;
    if (minLevel != 0)
        levelFilter += " AND level >= " + std::to_string(minLevel);
    if (maxLevel != 0)
        levelFilter += " AND level <= " + std::to_string(maxLevel);

    if (faction == 0)
    {
        result = CharacterDatabase.Query(
            "SELECT {} "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 "
            "AND population_role = 'world' "
            "{} "
            "ORDER BY RAND() LIMIT {}",
            GetBotIdentitySelectColumns(),
            levelFilter,
            candidateLimit);
    }
    else
    {
        result = CharacterDatabase.Query(
            "SELECT {} "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 AND faction = {} "
            "AND population_role = 'world' "
            "{} "
            "ORDER BY RAND() LIMIT {}",
            GetBotIdentitySelectColumns(),
            faction, levelFilter, candidateLimit);
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
            "SELECT {} "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 "
            "AND population_role = 'city_reserve' AND reserve_city_zone_id = {} "
            "ORDER BY RAND() LIMIT {}",
            GetBotIdentitySelectColumns(),
            reserveCityZoneId, candidateLimit);
    }
    else
    {
        result = CharacterDatabase.Query(
            "SELECT {} "
            "FROM living_world_bot_identity "
            "WHERE is_available = 1 AND is_retired = 0 AND faction = {} "
            "AND population_role = 'city_reserve' AND reserve_city_zone_id = {} "
            "ORDER BY RAND() LIMIT {}",
            GetBotIdentitySelectColumns(),
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

std::vector<BotIdentityRecord> SqlBotIdentityRepository::LoadAvailableAmbientGroup(
    std::uint32_t ambientGroupId) const
{
    std::vector<BotIdentityRecord> results;
    if (ambientGroupId == 0)
        return results;

    QueryResult result = CharacterDatabase.Query(
        "SELECT {} "
        "FROM living_world_bot_identity "
        "WHERE ambient_group_id = {} AND is_available = 1 AND is_retired = 0 "
        "ORDER BY "
        "CASE "
        "  WHEN ambient_group_leader_identity_id IS NOT NULL AND id = ambient_group_leader_identity_id THEN 0 "
        "  ELSE 1 "
        "END ASC, "
        "id ASC",
        GetBotIdentitySelectColumns(),
        ambientGroupId);

    if (!result)
        return results;

    do
    {
        results.push_back(ReadBotIdentityRecord(result->Fetch()));
    } while (result->NextRow());

    return results;
}

void SqlBotIdentityRepository::MarkActive(std::uint32_t id) const
{
    std::uint8_t level = 1;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT level FROM living_world_bot_identity WHERE id = {}",
            id))
    {
        level = result->Fetch()[0].Get<std::uint8_t>();
    }

    std::string const sessionPersonalityKey =
        QuoteCharactersString(RollWorldBotSessionPersonalityKey(level));
    std::uint64_t const sessionBudgetMs = RollWorldBotSessionBudgetMs();

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 0, session_count = session_count + 1, "
        "personality_key = {}, "
        "active_world_session_ms = 0, active_world_session_budget_ms = {}, active_world_session_start = NOW(), "
        "runtime_state = '', runtime_detail = '', "
        "last_task_activity_key = '', last_quest_hub_key = '', last_quest_hub_elapsed_ms = 0 "
        "WHERE id = {}",
        sessionPersonalityKey,
        sessionBudgetMs,
        id);
}

void SqlBotIdentityRepository::MarkAvailable(
    std::uint32_t id,
    std::uint32_t lastSeenZoneId) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET is_available = 1, last_seen_zone = {}, last_seen_at = NOW(), "
        "personality_key = 'uninterested', "
        "active_world_session_ms = 0, active_world_session_budget_ms = 0, active_world_session_start = NULL, "
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

std::vector<BotIdentityRecord> SqlBotIdentityRepository::LoadAppearanceUnresolved(
    std::uint32_t limit) const
{
    std::vector<BotIdentityRecord> results;
    std::string limitClause;
    if (limit > 0)
        limitClause = " LIMIT " + std::to_string(limit);

    QueryResult result = CharacterDatabase.Query(
        "SELECT {} "
        "FROM living_world_bot_identity "
        "WHERE appearance_resolved = 0 "
        "ORDER BY id ASC{}",
        GetBotIdentitySelectColumns(),
        limitClause);

    if (!result)
        return results;

    do
    {
        results.push_back(ReadBotIdentityRecord(result->Fetch()));
    } while (result->NextRow());

    return results;
}

void SqlBotIdentityRepository::UpdateAppearance(
    std::uint32_t id,
    std::uint8_t skin,
    std::uint8_t face,
    std::uint8_t hairStyle,
    std::uint8_t hairColor,
    std::uint8_t facialStyle,
    bool appearanceResolved) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET skin = {}, face = {}, hair_style = {}, hair_color = {}, facial_style = {}, appearance_resolved = {} "
        "WHERE id = {}",
        static_cast<std::uint32_t>(skin),
        static_cast<std::uint32_t>(face),
        static_cast<std::uint32_t>(hairStyle),
        static_cast<std::uint32_t>(hairColor),
        static_cast<std::uint32_t>(facialStyle),
        appearanceResolved ? 1u : 0u,
        id);
}

void SqlBotIdentityRepository::UpdateShellState(
    std::uint32_t id,
    std::uint32_t shellAccountId,
    std::uint64_t shellCharacterGuid,
    std::uint32_t shellStateVersion,
    std::string const& pendingRebuildReason) const
{
    std::string const normalizedReason =
        QuoteCharactersString(NormalizeRuntimeLedgerText(pendingRebuildReason, 64));

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET shell_account_id = {}, shell_character_guid = {}, shell_state_version = {}, "
        "pending_rebuild_reason = {} "
        "WHERE id = {}",
        shellAccountId == 0 ? std::string("NULL") : std::to_string(shellAccountId),
        shellCharacterGuid == 0 ? std::string("NULL") : std::to_string(shellCharacterGuid),
        shellStateVersion,
        normalizedReason,
        id);
}

void SqlBotIdentityRepository::MarkShellRehydrated(
    std::uint32_t id,
    std::uint32_t shellStateVersion) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET shell_state_version = {}, pending_rebuild_reason = '', last_rehydrate_at = NOW() "
        "WHERE id = {}",
        shellStateVersion,
        id);
}

void SqlBotIdentityRepository::UpdateActiveRuntimeState(
    std::uint32_t id,
    std::uint32_t zoneId,
    std::uint64_t activeWorldSessionMs,
    std::string const& runtimeState,
    std::string const& runtimeDetail,
    std::string const& currentTaskActivityKey,
    std::string const& currentQuestHubKey,
    std::uint64_t currentQuestHubElapsedMs) const
{
    std::string const normalizedState =
        QuoteCharactersString(NormalizeRuntimeLedgerText(runtimeState, 64));
    std::string const normalizedDetail =
        QuoteCharactersString(NormalizeRuntimeLedgerText(runtimeDetail, 255));
    std::string const normalizedTaskActivityKey =
        QuoteCharactersString(NormalizeRuntimeLedgerText(currentTaskActivityKey, 128));
    std::string const normalizedQuestHubKey =
        QuoteCharactersString(NormalizeRuntimeLedgerText(currentQuestHubKey, 128));

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET active_world_session_ms = {}, last_seen_zone = {}, "
        "runtime_state = {}, runtime_detail = {}, "
        "last_task_activity_key = {}, last_quest_hub_key = {}, last_quest_hub_elapsed_ms = {} "
        "WHERE id = {} AND is_available = 0",
        activeWorldSessionMs,
        zoneId == 0 ? std::string("NULL") : std::to_string(zoneId),
        normalizedState,
        normalizedDetail,
        normalizedTaskActivityKey,
        normalizedQuestHubKey,
        currentQuestHubElapsedMs,
        id);
}

void SqlBotIdentityRepository::UpdateGenericPotionCharges(
    std::uint32_t id,
    std::uint8_t genericPotionCharges) const
{
    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET generic_potion_charges = {} "
        "WHERE id = {}",
        std::min<std::uint32_t>(5u, genericPotionCharges),
        id);
}

void SqlBotIdentityRepository::CompleteWorldSession(
    std::uint32_t id,
    std::uint32_t lastSeenZoneId,
    std::uint64_t sessionWorldOnlineMs,
    std::string const& lastSessionSourceKind,
    std::string const& lastSessionSourceKey,
    std::string const& lastTaskFamily,
    std::uint32_t lastTaskTargetZoneId,
    std::string const& lastTaskActivityKey,
    std::string const& lastQuestHubKey,
    std::uint64_t lastQuestHubElapsedMs) const
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
        "home_zone_id, home_anchor_point_key, home_bind_point_key, generic_potion_charges, population_role "
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
    std::string const populationRole = f[20].IsNull() ? "world" : CanonicalizePopulationRole(f[20].Get<std::string>());

    if (isRetired)
    {
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET is_available = 0, last_seen_zone = {}, last_seen_at = NOW(), "
            "active_world_session_ms = 0, active_world_session_budget_ms = 0, active_world_session_start = NULL, "
            "runtime_state = '', runtime_detail = '' "
            "WHERE id = {}",
            lastSeenZoneId, id);
        return;
    }

    if (populationRole == "city_reserve")
    {
        std::string const normalizedSourceKind =
            QuoteCharactersString(NormalizeRuntimeLedgerText(lastSessionSourceKind, 64));
        std::string const normalizedSourceKey =
            QuoteCharactersString(NormalizeRuntimeLedgerText(lastSessionSourceKey, 128));
        std::string const normalizedTaskFamily =
            QuoteCharactersString(NormalizeRuntimeLedgerText(lastTaskFamily, 32));
        std::string const normalizedTaskActivityKey =
            QuoteCharactersString(NormalizeRuntimeLedgerText(lastTaskActivityKey, 128));
        std::string const normalizedQuestHubKey =
            QuoteCharactersString(NormalizeRuntimeLedgerText(lastQuestHubKey, 128));

        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET is_available = 1, last_seen_zone = {}, last_seen_at = NOW(), "
            "last_session_source_kind = {}, last_session_source_key = {}, "
            "last_task_family = {}, last_task_activity_key = {}, last_task_target_zone = {}, "
            "last_quest_hub_key = {}, last_quest_hub_elapsed_ms = {}, "
            "personality_key = 'uninterested', "
            "active_world_session_ms = 0, active_world_session_budget_ms = 0, active_world_session_start = NULL, "
            "runtime_state = '', runtime_detail = '' "
            "WHERE id = {}",
            lastSeenZoneId,
            normalizedSourceKind,
            normalizedSourceKey,
            normalizedTaskFamily,
            normalizedTaskActivityKey,
            lastTaskTargetZoneId == 0 ? std::string("NULL") : std::to_string(lastTaskTargetZoneId),
            normalizedQuestHubKey,
            lastQuestHubElapsedMs,
            id);
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

    std::uint8_t const oldGearBand =
        service::ComputeWorldBotGearRefreshBand(level, postMaxWorldOnlineMs);
    std::uint8_t const newGearBand =
        service::ComputeWorldBotGearRefreshBand(newLevel, newPostMax);
    bool const crossedGearBand = newGearBand != oldGearBand;
    bool const newGearRefreshPending = gearRefreshPending || crossedGearBand;
    bool retireNow = newLevel >= MaxBotLevel && newPostMax >= RetirementGraceMs;
    std::string const normalizedSourceKind =
        NormalizeRuntimeLedgerText(lastSessionSourceKind, 64);
    std::string const normalizedSourceKey =
        NormalizeRuntimeLedgerText(lastSessionSourceKey, 128);
    std::string const normalizedTaskFamily =
        NormalizeRuntimeLedgerText(lastTaskFamily, 32);
    std::string const normalizedTaskActivityKey =
        NormalizeRuntimeLedgerText(lastTaskActivityKey, 128);
    std::string const normalizedQuestHubKey =
        NormalizeRuntimeLedgerText(lastQuestHubKey, 128);

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
            "last_session_source_kind = {}, last_session_source_key = {}, last_task_family = {}, last_task_activity_key = {}, last_task_target_zone = {}, "
            "last_quest_hub_key = {}, last_quest_hub_elapsed_ms = {}, "
            "personality_key = 'uninterested', "
            "active_world_session_ms = 0, active_world_session_budget_ms = 0, active_world_session_start = NULL, "
            "runtime_state = '', runtime_detail = '' "
            "WHERE id = {}",
            newLevel, newTotal, newSinceLevel, newPostMax, newSuccessorSpawned, newGearRefreshPending,
            lastSeenZoneId,
            QuoteCharactersString(normalizedSourceKind),
            QuoteCharactersString(normalizedSourceKey),
            QuoteCharactersString(normalizedTaskFamily),
            QuoteCharactersString(normalizedTaskActivityKey),
            lastTaskTargetZoneId == 0 ? std::string("NULL") : std::to_string(lastTaskTargetZoneId),
            QuoteCharactersString(normalizedQuestHubKey),
            lastQuestHubElapsedMs,
            id);
        return;
    }

    CharacterDatabase.Execute(
        "UPDATE living_world_bot_identity "
        "SET level = {}, total_world_online_ms = {}, "
        "world_online_ms_since_level = {}, post_max_world_online_ms = {}, successor_spawned = {}, gear_refresh_pending = {}, "
        "is_available = 1, is_retired = 0, retired_at = NULL, "
        "last_seen_zone = {}, last_seen_at = NOW(), "
        "last_session_source_kind = {}, last_session_source_key = {}, last_task_family = {}, last_task_activity_key = {}, last_task_target_zone = {}, "
        "last_quest_hub_key = {}, last_quest_hub_elapsed_ms = {}, "
        "personality_key = 'uninterested', "
        "active_world_session_ms = 0, active_world_session_budget_ms = 0, active_world_session_start = NULL, "
        "runtime_state = '', runtime_detail = '' "
        "WHERE id = {}",
        newLevel, newTotal, newSinceLevel, newPostMax, newSuccessorSpawned, newGearRefreshPending,
        lastSeenZoneId,
        QuoteCharactersString(normalizedSourceKind),
        QuoteCharactersString(normalizedSourceKey),
        QuoteCharactersString(normalizedTaskFamily),
        QuoteCharactersString(normalizedTaskActivityKey),
        lastTaskTargetZoneId == 0 ? std::string("NULL") : std::to_string(lastTaskTargetZoneId),
        QuoteCharactersString(normalizedQuestHubKey),
        lastQuestHubElapsedMs,
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
        "personality_key = 'uninterested', "
        "active_world_session_ms = 0, active_world_session_budget_ms = 0, active_world_session_start = NULL, "
        "runtime_state = '', runtime_detail = '' "
        "WHERE is_available = 0 AND is_retired = 0");

    return recovered;
}

} // namespace integration
} // namespace living_world
