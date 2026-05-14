#include "integration/SqlBotVirtualLoadoutRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
model::WorldBotVirtualLoadout SqlBotVirtualLoadoutRepository::BuildLoadout(Field const* fields)
{
    model::WorldBotVirtualLoadout loadout;
    loadout.loadoutId = fields[0].Get<std::uint64_t>();
    loadout.classId = fields[1].Get<std::uint8_t>();
    loadout.specKey = fields[2].IsNull() ? "" : fields[2].Get<std::string>();
    loadout.loadoutKey = fields[3].IsNull() ? "" : fields[3].Get<std::string>();
    loadout.gearTier = fields[4].Get<std::uint8_t>();
    loadout.displayName = fields[5].Get<std::string>();
    loadout.description = fields[6].IsNull() ? "" : fields[6].Get<std::string>();
    loadout.bonusStrength = fields[7].Get<std::int32_t>();
    loadout.bonusAgility = fields[8].Get<std::int32_t>();
    loadout.bonusStamina = fields[9].Get<std::int32_t>();
    loadout.bonusIntellect = fields[10].Get<std::int32_t>();
    loadout.bonusSpirit = fields[11].Get<std::int32_t>();
    loadout.bonusHealth = fields[12].Get<std::int32_t>();
    loadout.bonusMana = fields[13].Get<std::int32_t>();
    loadout.bonusArmor = fields[14].Get<std::int32_t>();
    loadout.bonusAttackPower = fields[15].Get<std::int32_t>();
    loadout.bonusRangedAttackPower = fields[16].Get<std::int32_t>();
    return loadout;
}

std::vector<model::WorldBotVirtualLoadout> SqlBotVirtualLoadoutRepository::ListLoadouts() const
{
    std::vector<model::WorldBotVirtualLoadout> loadouts;
    QueryResult result = WorldDatabase.Query(
        "SELECT loadout_id, class_id, spec_key, loadout_key, gear_tier, display_name, description, "
        "bonus_strength, bonus_agility, bonus_stamina, bonus_intellect, bonus_spirit, "
        "bonus_health, bonus_mana, bonus_armor, bonus_attack_power, bonus_ranged_attack_power "
        "FROM living_world_bot_virtual_loadout "
        "ORDER BY class_id ASC, gear_tier ASC, spec_key ASC, loadout_key ASC");
    if (!result)
        return loadouts;

    do
    {
        loadouts.push_back(BuildLoadout(result->Fetch()));
    } while (result->NextRow());

    return loadouts;
}

std::optional<model::WorldBotVirtualLoadout> SqlBotVirtualLoadoutRepository::FindLoadout(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& loadoutKey,
    std::uint8_t gearTier) const
{
    std::string escapedSpec = specKey;
    WorldDatabase.EscapeString(escapedSpec);
    std::string escapedLoadout = loadoutKey;
    WorldDatabase.EscapeString(escapedLoadout);

    QueryResult result = WorldDatabase.Query(
        "SELECT loadout_id, class_id, spec_key, loadout_key, gear_tier, display_name, description, "
        "bonus_strength, bonus_agility, bonus_stamina, bonus_intellect, bonus_spirit, "
        "bonus_health, bonus_mana, bonus_armor, bonus_attack_power, bonus_ranged_attack_power "
        "FROM living_world_bot_virtual_loadout "
        "WHERE class_id = {} AND gear_tier = {} "
        "AND (LOWER(spec_key) = LOWER('{}') OR spec_key IS NULL OR spec_key = '') "
        "AND (LOWER(loadout_key) = LOWER('{}') OR loadout_key IS NULL OR loadout_key = '') "
        "ORDER BY CASE "
        "WHEN LOWER(loadout_key) = LOWER('{}') THEN 0 "
        "WHEN loadout_key IS NULL OR loadout_key = '' THEN 1 "
        "ELSE 2 END, "
        "CASE "
        "WHEN LOWER(spec_key) = LOWER('{}') THEN 0 "
        "WHEN spec_key IS NULL OR spec_key = '' THEN 1 "
        "ELSE 2 END, "
        "loadout_id ASC "
        "LIMIT 1",
        static_cast<std::uint32_t>(classId),
        static_cast<std::uint32_t>(gearTier),
        escapedSpec,
        escapedLoadout,
        escapedLoadout,
        escapedSpec);
    if (!result)
        return std::nullopt;

    return BuildLoadout(result->Fetch());
}
} // namespace integration
} // namespace living_world