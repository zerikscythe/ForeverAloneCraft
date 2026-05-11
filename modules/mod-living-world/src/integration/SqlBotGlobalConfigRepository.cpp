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
        "  ('combat_follow_override_distance', 20.0, 'If a ranged/healer bot is farther than this from owner, snap back to follow behaviour'),"
        "  ('reposition_distance',      8.0, 'Passive-mode catch-up distance before reissuing follow'),"
        "  ('ranged_min_distance',      8.0, 'Back away when a ranged/healer bot is closer than this to its target'),"
        "  ('ranged_optimal_distance', 25.0, 'Preferred chase stop distance for ranged/healer combat positioning'),"
        "  ('ranged_cast_range',       30.0, 'Approach target when farther than this spell-usage range'),"
        "  ('ranged_retreat_distance',  5.0, 'Short backstep distance when retreating from melee range'),"
        "  ('ranged_retreat_trigger_pct', 80.0, 'Retreat when ranged bot HP drops below this percent in melee range'),"
        "  ('ranged_retreat_reset_pct',  60.0, 'Allow another retreat only after HP drops below this percent again'),"
        "  ('assist_use_current_victim', 1.0, 'Normal assist: keep fighting bot current victim if still valid'),"
        "  ('assist_use_owner_victim',   1.0, 'Normal assist: consider owner current victim as follow-up target'),"
        "  ('assist_owner_victim_must_target_owner', 1.0, 'Require owner victim to be actively fighting back against owner before assist picks it'),"
        "  ('attack_lock_use_owner_victim', 1.0, 'During attack-lock, consider owner current victim if current bot victim is unavailable'),"
        "  ('attack_lock_use_owner_selection', 1.0, 'During attack-lock, consider owner selected target if other sources are unavailable'),"
        "  ('guard_use_current_victim', 1.0, 'Guard mode: keep bot current victim if still valid'),"
        "  ('guard_use_owner_attackers', 1.0, 'Guard mode: consider units actively attacking the owner'),"
         "  ('assist_require_targetable_for_attack', 1.0, 'Normal assist and guard: require candidate to currently pass attackable-for-attack checks'),"
         "  ('command_require_targetable_for_attack', 0.0, 'Forced-target and attack-lock assist: require candidate to currently pass attackable-for-attack checks instead of allowing pull/setup flicker'),"
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
        else if (key == "combat_follow_override_distance")
            cfg.combatFollowOverrideDistance = val;
        else if (key == "reposition_distance")
            cfg.repositionDistance = val;
        else if (key == "ranged_min_distance")
            cfg.rangedMinDistance = val;
        else if (key == "ranged_optimal_distance")
            cfg.rangedOptimalDistance = val;
        else if (key == "ranged_cast_range")
            cfg.rangedCastRange = val;
        else if (key == "ranged_retreat_distance")
            cfg.rangedRetreatDistance = val;
        else if (key == "ranged_retreat_trigger_pct")
            cfg.rangedRetreatTriggerPct = val;
        else if (key == "ranged_retreat_reset_pct")
            cfg.rangedRetreatResetPct = val;
        else if (key == "assist_use_current_victim")
            cfg.assistUseCurrentVictim = val >= 0.5f;
        else if (key == "assist_use_owner_victim")
            cfg.assistUseOwnerVictim = val >= 0.5f;
        else if (key == "assist_owner_victim_must_target_owner")
            cfg.assistOwnerVictimMustTargetOwner = val >= 0.5f;
        else if (key == "attack_lock_use_owner_victim")
            cfg.attackLockUseOwnerVictim = val >= 0.5f;
        else if (key == "attack_lock_use_owner_selection")
            cfg.attackLockUseOwnerSelection = val >= 0.5f;
        else if (key == "guard_use_current_victim")
            cfg.guardUseCurrentVictim = val >= 0.5f;
        else if (key == "guard_use_owner_attackers")
            cfg.guardUseOwnerAttackers = val >= 0.5f;
        else if (key == "assist_require_targetable_for_attack")
            cfg.assistRequireTargetableForAttack = val >= 0.5f;
        else if (key == "command_require_targetable_for_attack")
            cfg.commandRequireTargetableForAttack = val >= 0.5f;
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
