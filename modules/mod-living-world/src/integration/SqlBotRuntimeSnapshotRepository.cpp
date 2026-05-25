#include "integration/SqlBotRuntimeSnapshotRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
void SqlBotRuntimeSnapshotRepository::EnsureSchema() const
{
    CharacterDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_runtime_snapshot ("
        " identity_id INT UNSIGNED NOT NULL,"
        " map_id SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " zone_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " pos_x FLOAT NOT NULL DEFAULT 0,"
        " pos_y FLOAT NOT NULL DEFAULT 0,"
        " pos_z FLOAT NOT NULL DEFAULT 0,"
        " orientation FLOAT NOT NULL DEFAULT 0,"
        " runtime_state VARCHAR(64) NOT NULL DEFAULT '',"
        " runtime_detail VARCHAR(255) NOT NULL DEFAULT '',"
        " last_task_family VARCHAR(64) NOT NULL DEFAULT '',"
        " last_task_activity_key VARCHAR(96) NOT NULL DEFAULT '',"
        " last_task_target_zone_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " home_bind_point_key VARCHAR(64) NOT NULL DEFAULT '',"
        " generic_potion_charges TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        " PRIMARY KEY (identity_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

std::optional<model::BotRuntimeSnapshotRecord>
SqlBotRuntimeSnapshotRepository::LoadByIdentity(std::uint32_t identityId) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT identity_id, map_id, zone_id, pos_x, pos_y, pos_z, orientation, runtime_state, runtime_detail, "
        "last_task_family, last_task_activity_key, last_task_target_zone_id, home_bind_point_key, generic_potion_charges "
        "FROM living_world_bot_runtime_snapshot WHERE identity_id = {}",
        identityId);
    if (!result)
        return std::nullopt;

    Field const* fields = result->Fetch();
    model::BotRuntimeSnapshotRecord record;
    record.identityId = fields[0].Get<std::uint32_t>();
    record.mapId = fields[1].Get<std::uint16_t>();
    record.zoneId = fields[2].Get<std::uint32_t>();
    record.x = fields[3].Get<float>();
    record.y = fields[4].Get<float>();
    record.z = fields[5].Get<float>();
    record.o = fields[6].Get<float>();
    record.runtimeState = fields[7].Get<std::string>();
    record.runtimeDetail = fields[8].Get<std::string>();
    record.lastTaskFamily = fields[9].Get<std::string>();
    record.lastTaskActivityKey = fields[10].Get<std::string>();
    record.lastTaskTargetZoneId = fields[11].Get<std::uint32_t>();
    record.homeBindPointKey = fields[12].Get<std::string>();
    record.genericPotionCharges = fields[13].Get<std::uint8_t>();
    return record;
}

void SqlBotRuntimeSnapshotRepository::Upsert(
    model::BotRuntimeSnapshotRecord const& record) const
{
    std::string runtimeState = record.runtimeState;
    std::string runtimeDetail = record.runtimeDetail;
    std::string lastTaskFamily = record.lastTaskFamily;
    std::string lastTaskActivityKey = record.lastTaskActivityKey;
    std::string homeBindPointKey = record.homeBindPointKey;
    CharacterDatabase.EscapeString(runtimeState);
    CharacterDatabase.EscapeString(runtimeDetail);
    CharacterDatabase.EscapeString(lastTaskFamily);
    CharacterDatabase.EscapeString(lastTaskActivityKey);
    CharacterDatabase.EscapeString(homeBindPointKey);

    CharacterDatabase.Execute(
        "REPLACE INTO living_world_bot_runtime_snapshot "
        "(identity_id, map_id, zone_id, pos_x, pos_y, pos_z, orientation, runtime_state, runtime_detail, "
        "last_task_family, last_task_activity_key, last_task_target_zone_id, home_bind_point_key, generic_potion_charges) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, '{}', '{}', '{}', '{}', {}, '{}', {})",
        record.identityId,
        record.mapId,
        record.zoneId,
        record.x,
        record.y,
        record.z,
        record.o,
        runtimeState,
        runtimeDetail,
        lastTaskFamily,
        lastTaskActivityKey,
        record.lastTaskTargetZoneId,
        homeBindPointKey,
        static_cast<std::uint32_t>(record.genericPotionCharges));
}
} // namespace integration
} // namespace living_world
