#include "integration/SqlBotGlyphTemplateRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

#include <unordered_set>

namespace living_world
{
namespace integration
{
void SqlBotGlyphTemplateRepository::EnsureSchema() const
{
    WorldDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_glyph_template ("
        " template_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        " class_id TINYINT UNSIGNED NOT NULL,"
        " spec_key VARCHAR(32) NOT NULL DEFAULT '',"
        " loadout_key VARCHAR(64) NOT NULL DEFAULT '',"
        " slot_index TINYINT UNSIGNED NOT NULL,"
        " glyph_spell_id INT UNSIGNED NOT NULL,"
        " PRIMARY KEY (template_id),"
        " UNIQUE KEY uk_lw_bot_glyph_template "
        " (class_id, spec_key, loadout_key, slot_index),"
        " KEY idx_lw_bot_glyph_template_lookup "
        " (class_id, spec_key, loadout_key, slot_index)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

std::vector<model::BotGlyphTemplateEntry> SqlBotGlyphTemplateRepository::LoadTemplate(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& loadoutKey) const
{
    std::vector<model::BotGlyphTemplateEntry> entries;

    std::string escapedSpec = specKey;
    WorldDatabase.EscapeString(escapedSpec);
    std::string escapedLoadout = loadoutKey;
    WorldDatabase.EscapeString(escapedLoadout);

    QueryResult result = WorldDatabase.Query(
        "SELECT slot_index, glyph_spell_id "
        "FROM living_world_bot_glyph_template "
        "WHERE class_id = {} "
        "AND (LOWER(spec_key) = LOWER('{}') OR spec_key = '') "
        "AND (LOWER(loadout_key) = LOWER('{}') OR loadout_key = '') "
        "ORDER BY CASE "
        "WHEN LOWER(loadout_key) = LOWER('{}') THEN 0 "
        "WHEN loadout_key = '' THEN 1 "
        "ELSE 2 END, "
        "CASE "
        "WHEN LOWER(spec_key) = LOWER('{}') THEN 0 "
        "WHEN spec_key = '' THEN 1 "
        "ELSE 2 END, "
        "slot_index ASC",
        static_cast<std::uint32_t>(classId),
        escapedSpec,
        escapedLoadout,
        escapedLoadout,
        escapedSpec);
    if (!result)
        return entries;

    std::unordered_set<std::uint8_t> selectedSlots;
    do
    {
        Field const* fields = result->Fetch();
        model::BotGlyphTemplateEntry entry;
        entry.slotIndex = fields[0].Get<std::uint8_t>();
        if (selectedSlots.find(entry.slotIndex) != selectedSlots.end())
            continue;

        entry.glyphSpellId = fields[1].Get<std::uint32_t>();
        entries.push_back(entry);
        selectedSlots.insert(entry.slotIndex);
    } while (result->NextRow());

    return entries;
}
} // namespace integration
} // namespace living_world
