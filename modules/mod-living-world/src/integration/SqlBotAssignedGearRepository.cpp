#include "integration/SqlBotAssignedGearRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "StringFormat.h"

namespace living_world
{
namespace integration
{
void SqlBotAssignedGearRepository::EnsureSchema() const
{
    CharacterDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_assigned_gear ("
        " identity_id INT UNSIGNED NOT NULL,"
        " slot_id TINYINT UNSIGNED NOT NULL,"
        " item_id INT UNSIGNED NOT NULL,"
        " item_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " quality TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " enchantments VARCHAR(512) NOT NULL DEFAULT '',"
        " PRIMARY KEY (identity_id, slot_id),"
        " KEY idx_lwbag_item (item_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    QueryResult hasEnchantmentsColumn = CharacterDatabase.Query(
        "SHOW COLUMNS FROM living_world_bot_assigned_gear LIKE 'enchantments'");
    if (!hasEnchantmentsColumn)
    {
        CharacterDatabase.Execute(
            "ALTER TABLE living_world_bot_assigned_gear "
            "ADD COLUMN enchantments VARCHAR(512) NOT NULL DEFAULT '' "
            "AFTER quality");
    }
}

std::vector<model::WorldBotAssignedGearEntry> SqlBotAssignedGearRepository::LoadAssignments(
    std::uint32_t identityId) const
{
    std::vector<model::WorldBotAssignedGearEntry> entries;

    QueryResult result = CharacterDatabase.Query(
        "SELECT slot_id, item_id, item_level, quality, enchantments "
        "FROM living_world_bot_assigned_gear "
        "WHERE identity_id = {} "
        "ORDER BY slot_id ASC",
        identityId);

    if (!result)
        return entries;

    do
    {
        Field const* fields = result->Fetch();
        model::WorldBotAssignedGearEntry entry;
        entry.slot = fields[0].Get<std::uint8_t>();
        entry.itemId = fields[1].Get<std::uint32_t>();
        entry.itemLevel = fields[2].Get<std::uint32_t>();
        entry.quality = fields[3].Get<std::uint8_t>();
        entry.enchantments = fields[4].Get<std::string>();
        entries.push_back(entry);
    } while (result->NextRow());

    return entries;
}

void SqlBotAssignedGearRepository::ReplaceAssignments(
    std::uint32_t identityId,
    std::uint8_t refreshBand,
    std::vector<model::WorldBotAssignedGearEntry> const& entries) const
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM living_world_bot_assigned_gear WHERE identity_id = {}",
            identityId));

    for (model::WorldBotAssignedGearEntry const& entry : entries)
    {
        std::string enchantments = entry.enchantments;
        CharacterDatabase.EscapeString(enchantments);
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "INSERT INTO living_world_bot_assigned_gear "
                "(identity_id, slot_id, item_id, item_level, quality, enchantments) "
                "VALUES ({}, {}, {}, {}, {}, '{}')",
                identityId,
                static_cast<std::uint32_t>(entry.slot),
                entry.itemId,
                entry.itemLevel,
                static_cast<std::uint32_t>(entry.quality),
                enchantments));
    }

    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "UPDATE living_world_bot_identity "
            "SET gear_refresh_pending = 0, last_gear_refresh_band = {} "
            "WHERE id = {}",
            static_cast<std::uint32_t>(refreshBand),
            identityId));

    CharacterDatabase.CommitTransaction(trans);
}
} // namespace integration
} // namespace living_world
