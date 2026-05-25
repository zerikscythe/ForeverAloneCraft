#include "integration/SqlBotAssignedGearTemplateRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

#include <unordered_set>

namespace living_world
{
namespace integration
{
void SqlBotAssignedGearTemplateRepository::EnsureSchema() const
{
    WorldDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_assigned_gear_template ("
        " template_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        " class_id TINYINT UNSIGNED NOT NULL,"
        " spec_key VARCHAR(32) NOT NULL DEFAULT '',"
        " loadout_key VARCHAR(64) NOT NULL DEFAULT '',"
        " race_mask INT UNSIGNED NOT NULL DEFAULT 0,"
        " endgame_stage TINYINT UNSIGNED NOT NULL,"
        " slot_id TINYINT UNSIGNED NOT NULL,"
        " item_id INT UNSIGNED NOT NULL,"
        " enchantments VARCHAR(512) NOT NULL DEFAULT '',"
        " PRIMARY KEY (template_id),"
        " UNIQUE KEY uk_lw_bot_assigned_gear_template "
        " (class_id, spec_key, loadout_key, race_mask, endgame_stage, slot_id),"
        " KEY idx_lw_bot_assigned_gear_template_lookup "
        " (class_id, endgame_stage, spec_key, loadout_key, race_mask)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    if (!WorldDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_assigned_gear_template LIKE 'race_mask'"))
    {
        WorldDatabase.Execute(
            "ALTER TABLE living_world_bot_assigned_gear_template "
            "ADD COLUMN race_mask INT UNSIGNED NOT NULL DEFAULT 0 AFTER loadout_key");
    }

    if (!WorldDatabase.Query(
        "SHOW INDEX FROM living_world_bot_assigned_gear_template "
        "WHERE Key_name = 'uk_lw_bot_assigned_gear_template'"))
    {
        WorldDatabase.Execute(
            "ALTER TABLE living_world_bot_assigned_gear_template "
            "ADD UNIQUE KEY uk_lw_bot_assigned_gear_template "
            "(class_id, spec_key, loadout_key, race_mask, endgame_stage, slot_id)");
    }

    if (!WorldDatabase.Query(
        "SHOW INDEX FROM living_world_bot_assigned_gear_template "
        "WHERE Key_name = 'idx_lw_bot_assigned_gear_template_lookup'"))
    {
        WorldDatabase.Execute(
            "ALTER TABLE living_world_bot_assigned_gear_template "
            "ADD KEY idx_lw_bot_assigned_gear_template_lookup "
            "(class_id, endgame_stage, spec_key, loadout_key, race_mask)");
    }
}

std::vector<model::WorldBotAssignedGearEntry> SqlBotAssignedGearTemplateRepository::LoadEndgameStageTemplate(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& loadoutKey,
    std::uint8_t endgameStage,
    std::uint8_t raceId) const
{
    std::vector<model::WorldBotAssignedGearEntry> entries;

    std::string escapedSpec = specKey;
    WorldDatabase.EscapeString(escapedSpec);
    std::string escapedLoadout = loadoutKey;
    WorldDatabase.EscapeString(escapedLoadout);
    std::uint32_t const raceMask = raceId == 0
        ? 0u
        : (1u << (static_cast<std::uint32_t>(raceId) - 1u));

    QueryResult result = WorldDatabase.Query(
        "SELECT slot_id, item_id, enchantments, race_mask "
        "FROM living_world_bot_assigned_gear_template "
        "WHERE class_id = {} AND endgame_stage = {} "
        "AND (LOWER(spec_key) = LOWER('{}') OR spec_key = '') "
        "AND (LOWER(loadout_key) = LOWER('{}') OR loadout_key = '') "
        "AND (race_mask = 0 OR (race_mask & {}) <> 0) "
        "ORDER BY CASE "
        "WHEN LOWER(loadout_key) = LOWER('{}') THEN 0 "
        "WHEN loadout_key = '' THEN 1 "
        "ELSE 2 END, "
        "CASE "
        "WHEN LOWER(spec_key) = LOWER('{}') THEN 0 "
        "WHEN spec_key = '' THEN 1 "
        "ELSE 2 END, "
        "CASE "
        "WHEN race_mask = 0 THEN 2 "
        "ELSE 0 END, "
        "BIT_COUNT(race_mask) ASC, "
        "slot_id ASC",
        static_cast<std::uint32_t>(classId),
        static_cast<std::uint32_t>(endgameStage),
        escapedSpec,
        escapedLoadout,
        raceMask,
        escapedLoadout,
        escapedSpec);
    if (!result)
        return entries;

    std::unordered_set<std::uint8_t> selectedSlots;
    do
    {
        Field const* fields = result->Fetch();
        model::WorldBotAssignedGearEntry entry;
        entry.slot = fields[0].Get<std::uint8_t>();
        if (selectedSlots.find(entry.slot) != selectedSlots.end())
            continue;

        entry.itemId = fields[1].Get<std::uint32_t>();
        entry.enchantments = fields[2].Get<std::string>();
        entries.push_back(entry);
        selectedSlots.insert(entry.slot);
    } while (result->NextRow());

    return entries;
}
} // namespace integration
} // namespace living_world
