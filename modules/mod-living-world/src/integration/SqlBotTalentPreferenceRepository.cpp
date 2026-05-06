#include "integration/SqlBotTalentPreferenceRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
void SqlBotTalentPreferenceRepository::EnsureSchema() const
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_talent_preference ("
        "  source_character_guid BIGINT UNSIGNED NOT NULL,"
        "  template_id           BIGINT UNSIGNED NOT NULL DEFAULT 0,"
        "  auto_apply_on_level   TINYINT(1) NOT NULL DEFAULT 1,"
        "  PRIMARY KEY (source_character_guid)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
}

std::optional<model::BotTalentPreference>
SqlBotTalentPreferenceRepository::GetPreference(
    std::uint64_t sourceCharGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT source_character_guid, template_id, auto_apply_on_level "
        "FROM living_world_bot_talent_preference "
        "WHERE source_character_guid = {}",
        sourceCharGuid);
    if (!result)
        return std::nullopt;

    Field const* fields = result->Fetch();
    model::BotTalentPreference pref;
    pref.sourceCharacterGuid = fields[0].Get<std::uint64_t>();
    pref.templateId          = fields[1].Get<std::uint64_t>();
    pref.autoApplyOnLevel    = fields[2].Get<bool>();
    return pref;
}

void SqlBotTalentPreferenceRepository::SavePreference(
    model::BotTalentPreference const& pref)
{
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_talent_preference "
        "(source_character_guid, template_id, auto_apply_on_level) "
        "VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "template_id=VALUES(template_id), "
        "auto_apply_on_level=VALUES(auto_apply_on_level)",
        pref.sourceCharacterGuid,
        pref.templateId,
        pref.autoApplyOnLevel ? 1 : 0);
}

void SqlBotTalentPreferenceRepository::ClearPreference(
    std::uint64_t sourceCharGuid)
{
    CharacterDatabase.Execute(
        "DELETE FROM living_world_bot_talent_preference "
        "WHERE source_character_guid = {}",
        sourceCharGuid);
}
} // namespace integration
} // namespace living_world
