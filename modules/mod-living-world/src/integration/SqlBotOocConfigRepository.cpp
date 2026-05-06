#include "integration/SqlBotOocConfigRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
namespace
{
// All loot category flags ON by default (0x3F = all 6 bits set).
// Bot grabs cloth, ore, herbs, enchanting mats, recipes and gems unless
// the operator explicitly unchecks a category.
constexpr std::uint32_t DefaultLootCategoryFlags = 0x3F;

model::BotOocBehavior BuildOocBehavior(Field const* f)
{
    model::BotOocBehavior ooc;
    ooc.buffScope         = static_cast<model::BotBuffScope>(f[0].Get<std::uint8_t>());
    ooc.buffReapplySecs   = f[1].Get<std::uint16_t>();
    ooc.buffOnSpawn       = f[2].Get<bool>();
    if (!f[3].IsNull())
        ooc.followDistOverride = f[3].Get<float>();
    if (!f[4].IsNull())
        ooc.autoLootOverride = f[4].Get<std::uint8_t>() != 0;
    ooc.lootQualityMin    = f[5].Get<std::uint8_t>();
    ooc.lootCategoryFlags = f[6].Get<std::uint32_t>();
    ooc.gatherNodes       = static_cast<model::BotGatherNodes>(f[7].Get<std::uint8_t>());
    ooc.gatherSkin        = static_cast<model::BotGatherSkin>(f[8].Get<std::uint8_t>());
    ooc.skinLootQualityMax= f[9].Get<std::uint8_t>();
    return ooc;
}
} // namespace

void SqlBotOocConfigRepository::EnsureSchema() const
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_ooc_config ("
        "  source_character_guid BIGINT UNSIGNED NOT NULL,"
        "  buff_scope             TINYINT  NOT NULL DEFAULT 2,"
        "  buff_reapply_secs      SMALLINT NOT NULL DEFAULT 30,"
        "  buff_on_spawn          TINYINT  NOT NULL DEFAULT 1,"
        "  follow_dist_override   FLOAT    DEFAULT NULL,"
        "  auto_loot_override     TINYINT  DEFAULT NULL,"
        "  loot_quality_min       TINYINT  NOT NULL DEFAULT 0,"
        "  loot_category_flags    INT      NOT NULL DEFAULT 63,"
        "  gather_nodes           TINYINT  NOT NULL DEFAULT 0,"
        "  gather_skin            TINYINT  NOT NULL DEFAULT 0,"
        "  skin_loot_quality_max  TINYINT  NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (source_character_guid)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
}

model::BotOocBehavior
SqlBotOocConfigRepository::Load(std::uint64_t sourceCharGuid) const
{
    QueryResult qr = CharacterDatabase.Query(
        "SELECT buff_scope, buff_reapply_secs, buff_on_spawn, "
        "follow_dist_override, auto_loot_override, loot_quality_min, "
        "loot_category_flags, gather_nodes, gather_skin, skin_loot_quality_max "
        "FROM living_world_bot_ooc_config "
        "WHERE source_character_guid = {}",
        sourceCharGuid);

    if (qr)
        return BuildOocBehavior(qr->Fetch());

    // No row yet — insert defaults and return them.
    CharacterDatabase.Execute(
        "INSERT IGNORE INTO living_world_bot_ooc_config "
        "(source_character_guid, loot_category_flags) "
        "VALUES ({}, {})",
        sourceCharGuid,
        DefaultLootCategoryFlags);

    model::BotOocBehavior defaults;
    defaults.lootCategoryFlags = DefaultLootCategoryFlags;
    return defaults;
}

void SqlBotOocConfigRepository::Save(std::uint64_t sourceCharGuid,
                                      model::BotOocBehavior const& ooc) const
{
    std::string followVal = ooc.followDistOverride.has_value()
        ? std::to_string(*ooc.followDistOverride) : "NULL";
    std::string autoLootVal = ooc.autoLootOverride.has_value()
        ? std::to_string(*ooc.autoLootOverride ? 1 : 0) : "NULL";

    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_ooc_config "
        "(source_character_guid, buff_scope, buff_reapply_secs, buff_on_spawn, "
        "follow_dist_override, auto_loot_override, loot_quality_min, "
        "loot_category_flags, gather_nodes, gather_skin, skin_loot_quality_max) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "buff_scope=VALUES(buff_scope), buff_reapply_secs=VALUES(buff_reapply_secs), "
        "buff_on_spawn=VALUES(buff_on_spawn), follow_dist_override=VALUES(follow_dist_override), "
        "auto_loot_override=VALUES(auto_loot_override), loot_quality_min=VALUES(loot_quality_min), "
        "loot_category_flags=VALUES(loot_category_flags), gather_nodes=VALUES(gather_nodes), "
        "gather_skin=VALUES(gather_skin), skin_loot_quality_max=VALUES(skin_loot_quality_max)",
        sourceCharGuid,
        static_cast<std::uint32_t>(ooc.buffScope),
        static_cast<std::uint32_t>(ooc.buffReapplySecs),
        ooc.buffOnSpawn ? 1 : 0,
        followVal,
        autoLootVal,
        static_cast<std::uint32_t>(ooc.lootQualityMin),
        ooc.lootCategoryFlags,
        static_cast<std::uint32_t>(ooc.gatherNodes),
        static_cast<std::uint32_t>(ooc.gatherSkin),
        static_cast<std::uint32_t>(ooc.skinLootQualityMax));
}

} // namespace integration
} // namespace living_world
