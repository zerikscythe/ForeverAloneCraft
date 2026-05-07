#include "integration/SqlBotGlobalConfigRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{

void SqlBotGlobalConfigRepository::EnsureSchema() const
{
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_global_config ("
        "  config_key   VARCHAR(64) NOT NULL,"
        "  config_value FLOAT       NOT NULL,"
        "  notes        VARCHAR(255)         DEFAULT NULL,"
        "  PRIMARY KEY (config_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");

    // INSERT IGNORE: existing operator-edited values are never overwritten.
    WorldDatabase.DirectExecute(
        "INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES"
        "  ('follow_distance',          2.0, 'Fallback follow yards when role is unknown (e.g. Passive mode)'),"
        "  ('follow_distance_melee',   1.0, 'Follow yards: Tank and Melee DPS'),"
        "  ('follow_distance_healer',  1.5, 'Follow yards: Healer and Hybrid Healer'),"
        "  ('follow_distance_ranged',  2.5, 'Follow yards: Ranged and caster DPS'),"
        "  ('follow_formation',        0.0, '0=Ring  1=V-shape  2=Line  3=Cluster'),"
        "  ('follow_slot_count',       7.0, 'Number of positions in Ring formation (3-9)'),"
        "  ('mount_with_owner',        1.0, '1=bots mount when owner mounts (implementation pending)'),"
        "  ('auto_loot',               0.0, '1=bots auto-loot nearby corpses (implementation pending)')");
}

model::BotGlobalConfig SqlBotGlobalConfigRepository::Load() const
{
    model::BotGlobalConfig cfg; // struct defaults are the fallback

    QueryResult qr = WorldDatabase.Query(
        "SELECT config_key, config_value FROM living_world_bot_global_config");
    if (!qr)
        return cfg;

    do
    {
        Field const* f    = qr->Fetch();
        std::string  key  = f[0].Get<std::string>();
        float        val  = f[1].Get<float>();

        if      (key == "follow_distance")
            cfg.followDistanceFallback = val;
        else if (key == "follow_distance_melee")
            cfg.followDistanceMelee = val;
        else if (key == "follow_distance_healer")
            cfg.followDistanceHealer = val;
        else if (key == "follow_distance_ranged")
            cfg.followDistanceRanged = val;
        else if (key == "follow_formation")
            cfg.followFormation = static_cast<model::FollowFormation>(
                static_cast<uint8_t>(val));
        else if (key == "follow_slot_count")
            cfg.followSlotCount = std::max(3u,
                std::min(9u, static_cast<uint32_t>(val)));
        else if (key == "mount_with_owner")
            cfg.mountWithOwner = val >= 0.5f;
        else if (key == "auto_loot")
            cfg.autoLoot = val >= 0.5f;
    } while (qr->NextRow());

    return cfg;
}

} // namespace integration
} // namespace living_world
