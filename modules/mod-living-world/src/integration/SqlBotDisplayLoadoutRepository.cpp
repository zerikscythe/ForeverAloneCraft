#include "integration/SqlBotDisplayLoadoutRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "StringFormat.h"

namespace living_world
{
namespace integration
{
void SqlBotDisplayLoadoutRepository::EnsureSchema() const
{
    CharacterDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_display_loadout ("
        " identity_id INT UNSIGNED NOT NULL,"
        " display_loadout_key VARCHAR(64) NOT NULL DEFAULT '',"
        " helm_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " shoulder_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " shirt_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " chest_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " waist_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " legs_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " feet_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " wrist_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " hands_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " back_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " tabard_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " mainhand_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " offhand_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " ranged_item_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " hide_helm TINYINT(1) NOT NULL DEFAULT 0,"
        " hide_cloak TINYINT(1) NOT NULL DEFAULT 0,"
        " PRIMARY KEY (identity_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

std::optional<model::BotDisplayLoadoutRecord>
SqlBotDisplayLoadoutRepository::LoadByIdentity(std::uint32_t identityId) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT identity_id, display_loadout_key, helm_item_id, shoulder_item_id, shirt_item_id, chest_item_id, "
        "waist_item_id, legs_item_id, feet_item_id, wrist_item_id, hands_item_id, back_item_id, tabard_item_id, "
        "mainhand_item_id, offhand_item_id, ranged_item_id, hide_helm, hide_cloak "
        "FROM living_world_bot_display_loadout WHERE identity_id = {}",
        identityId);
    if (!result)
        return std::nullopt;

    Field const* fields = result->Fetch();
    model::BotDisplayLoadoutRecord record;
    record.identityId = fields[0].Get<std::uint32_t>();
    record.displayLoadoutKey = fields[1].Get<std::string>();
    record.helmItemId = fields[2].Get<std::uint32_t>();
    record.shoulderItemId = fields[3].Get<std::uint32_t>();
    record.shirtItemId = fields[4].Get<std::uint32_t>();
    record.chestItemId = fields[5].Get<std::uint32_t>();
    record.waistItemId = fields[6].Get<std::uint32_t>();
    record.legsItemId = fields[7].Get<std::uint32_t>();
    record.feetItemId = fields[8].Get<std::uint32_t>();
    record.wristItemId = fields[9].Get<std::uint32_t>();
    record.handsItemId = fields[10].Get<std::uint32_t>();
    record.backItemId = fields[11].Get<std::uint32_t>();
    record.tabardItemId = fields[12].Get<std::uint32_t>();
    record.mainHandItemId = fields[13].Get<std::uint32_t>();
    record.offHandItemId = fields[14].Get<std::uint32_t>();
    record.rangedItemId = fields[15].Get<std::uint32_t>();
    record.hideHelm = fields[16].Get<bool>();
    record.hideCloak = fields[17].Get<bool>();
    return record;
}

void SqlBotDisplayLoadoutRepository::Replace(
    model::BotDisplayLoadoutRecord const& record) const
{
    std::string displayLoadoutKey = record.displayLoadoutKey;
    CharacterDatabase.EscapeString(displayLoadoutKey);

    CharacterDatabase.Execute(
        "REPLACE INTO living_world_bot_display_loadout "
        "(identity_id, display_loadout_key, helm_item_id, shoulder_item_id, shirt_item_id, chest_item_id, "
        "waist_item_id, legs_item_id, feet_item_id, wrist_item_id, hands_item_id, back_item_id, tabard_item_id, "
        "mainhand_item_id, offhand_item_id, ranged_item_id, hide_helm, hide_cloak) "
        "VALUES ({}, '{}', {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
        record.identityId,
        displayLoadoutKey,
        record.helmItemId,
        record.shoulderItemId,
        record.shirtItemId,
        record.chestItemId,
        record.waistItemId,
        record.legsItemId,
        record.feetItemId,
        record.wristItemId,
        record.handsItemId,
        record.backItemId,
        record.tabardItemId,
        record.mainHandItemId,
        record.offHandItemId,
        record.rangedItemId,
        record.hideHelm ? 1u : 0u,
        record.hideCloak ? 1u : 0u);
}
} // namespace integration
} // namespace living_world
