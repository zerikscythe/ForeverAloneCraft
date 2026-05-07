#include "integration/BotActivityLog.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"

namespace living_world
{
namespace integration
{

void BotActivityLog::Record(
    Player* bot,
    std::string_view eventType,
    std::string_view detail)
{
    if (!bot)
        return;

    std::uint64_t const guid   = bot->GetGUID().GetCounter();
    std::string   const name   = bot->GetName();
    std::uint16_t const mapId  = static_cast<std::uint16_t>(bot->GetMapId());
    std::uint32_t const zoneId = bot->GetZoneId();
    float         const x      = bot->GetPositionX();
    float         const y      = bot->GetPositionY();
    float         const z      = bot->GetPositionZ();

    LOG_INFO("server.worldserver",
        "[LivingWorldBot] bot='{}' guid={} event='{}' detail='{}' "
        "map={} zone={} pos=({:.1f},{:.1f},{:.1f})",
        name, guid, eventType, detail, mapId, zoneId, x, y, z);

    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_activity_log "
        "(bot_guid, bot_name, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z) "
        "VALUES ({}, '{}', '{}', '{}', {}, {}, {}, {}, {})",
        guid, name, eventType, detail, mapId, zoneId, x, y, z);
}

} // namespace integration
} // namespace living_world
