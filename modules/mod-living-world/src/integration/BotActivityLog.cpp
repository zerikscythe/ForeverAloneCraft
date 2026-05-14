#include "integration/BotActivityLog.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "Unit.h"

namespace living_world
{
namespace integration
{

namespace
{
constexpr std::size_t MaxActivityDetailLength = 255;

std::string QuoteCharacterString(std::string_view value)
{
    std::string escaped(value);
    CharacterDatabase.EscapeString(escaped);
    return "'" + escaped + "'";
}

std::string NormalizeDetail(std::string_view detail)
{
    std::string normalized(detail);
    if (normalized.size() <= MaxActivityDetailLength)
        return normalized;

    constexpr char TruncationSuffix[] = "...";
    constexpr std::size_t TruncationSuffixLength = sizeof(TruncationSuffix) - 1;
    normalized.resize(MaxActivityDetailLength - TruncationSuffixLength);
    normalized += TruncationSuffix;
    return normalized;
}
}

void BotActivityLog::Record(
    Player* bot,
    std::string_view eventType,
    std::string_view detail)
{
    if (!bot) return;
    std::uint64_t const guid = bot->GetGUID().GetCounter();
    std::string const name = bot->GetName();
    std::string const normalizedDetail = NormalizeDetail(detail);
    std::uint16_t const mapId = static_cast<std::uint16_t>(bot->GetMapId());
    std::uint32_t const zoneId = bot->GetZoneId();
    float const x = bot->GetPositionX(), y = bot->GetPositionY(), z = bot->GetPositionZ();
    LOG_INFO("server.worldserver", "[LivingWorldBot] bot='{}' guid={} event='{}' detail='{}' map={} zone={} pos=({:.1f},{:.1f},{:.1f})", name, guid, eventType, normalizedDetail, mapId, zoneId, x, y, z);
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_activity_log (bot_guid, bot_name, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})",
        guid,
        QuoteCharacterString(name),
        QuoteCharacterString(eventType),
        QuoteCharacterString(normalizedDetail),
        mapId,
        zoneId,
        x,
        y,
        z);
}

void BotActivityLog::Record(Unit* bot, std::string_view botName, std::uint64_t botIdentityId, std::string_view eventType, std::string_view detail)
{
    if (!bot) return;
    std::string const normalizedDetail = NormalizeDetail(detail);
    std::uint16_t const mapId = static_cast<std::uint16_t>(bot->GetMapId());
    std::uint32_t const zoneId = bot->GetZoneId();
    float const x = bot->GetPositionX(), y = bot->GetPositionY(), z = bot->GetPositionZ();
    LOG_INFO("server.worldserver", "[LivingWorldBot] bot='{}' identity={} event='{}' detail='{}' map={} zone={} pos=({:.1f},{:.1f},{:.1f})", botName, botIdentityId, eventType, normalizedDetail, mapId, zoneId, x, y, z);
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_activity_log (bot_guid, bot_name, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})",
        botIdentityId,
        QuoteCharacterString(botName),
        QuoteCharacterString(eventType),
        QuoteCharacterString(normalizedDetail),
        mapId,
        zoneId,
        x,
        y,
        z);
}

void BotActivityLog::RecordAbstract(
    std::string_view botName,
    std::uint64_t botIdentityId,
    std::string_view eventType,
    std::string_view detail,
    std::uint16_t mapId,
    std::uint32_t zoneId,
    float x,
    float y,
    float z)
{
    std::string const normalizedDetail = NormalizeDetail(detail);
    LOG_INFO("server.worldserver", "[LivingWorldBot] bot='{}' identity={} event='{}' detail='{}' map={} zone={} pos=({:.1f},{:.1f},{:.1f})", botName, botIdentityId, eventType, normalizedDetail, mapId, zoneId, x, y, z);
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_activity_log (bot_guid, bot_name, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})",
        botIdentityId,
        QuoteCharacterString(botName),
        QuoteCharacterString(eventType),
        QuoteCharacterString(normalizedDetail),
        mapId,
        zoneId,
        x,
        y,
        z);
}

} // namespace integration
} // namespace living_world