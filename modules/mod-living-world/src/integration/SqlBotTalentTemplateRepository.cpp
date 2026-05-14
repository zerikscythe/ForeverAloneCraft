#include "integration/SqlBotTalentTemplateRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
model::BotTalentTemplateRecord
SqlBotTalentTemplateRepository::BuildTemplate(Field const* fields)
{
    model::BotTalentTemplateRecord tmpl;
    tmpl.templateId  = fields[0].Get<std::uint64_t>();
    tmpl.specKey     = fields[1].Get<std::string>();
    tmpl.classId     = fields[2].Get<std::uint8_t>();
    tmpl.displayName = fields[3].Get<std::string>();
    tmpl.variantKey  = fields[4].IsNull() ? "" : fields[4].Get<std::string>();
    tmpl.description = fields[5].IsNull() ? "" : fields[5].Get<std::string>();
    return tmpl;
}

void SqlBotTalentTemplateRepository::LoadTemplateEntries(
    model::BotTalentTemplateRecord& tmpl)
{
    QueryResult result = WorldDatabase.Query(
        "SELECT entry_id, priority, talent_id, desired_rank "
        "FROM living_world_bot_talent_template_entry "
        "WHERE template_id = {} "
        "ORDER BY priority ASC",
        tmpl.templateId);
    if (!result)
        return;

    do
    {
        Field const* fields = result->Fetch();
        model::BotTalentTemplateEntry entry;
        entry.entryId     = fields[0].Get<std::uint64_t>();
        entry.priority    = fields[1].Get<std::uint16_t>();
        entry.talentId    = fields[2].Get<std::uint16_t>();
        entry.desiredRank = fields[3].Get<std::uint8_t>();
        tmpl.entries.push_back(entry);
    } while (result->NextRow());
}

std::vector<model::BotTalentTemplateRecord>
SqlBotTalentTemplateRepository::ListTemplates() const
{
    std::vector<model::BotTalentTemplateRecord> templates;
    QueryResult result = WorldDatabase.Query(
        "SELECT template_id, spec_key, class_id, display_name, variant_key, description "
        "FROM living_world_bot_talent_template "
        "ORDER BY class_id, spec_key ASC");
    if (!result)
        return templates;

    do
    {
        model::BotTalentTemplateRecord tmpl = BuildTemplate(result->Fetch());
        LoadTemplateEntries(tmpl);
        templates.push_back(std::move(tmpl));
    } while (result->NextRow());

    return templates;
}

std::optional<model::BotTalentTemplateRecord>
SqlBotTalentTemplateRepository::FindTemplate(std::uint64_t templateId) const
{
    QueryResult result = WorldDatabase.Query(
        "SELECT template_id, spec_key, class_id, display_name, variant_key, description "
        "FROM living_world_bot_talent_template "
        "WHERE template_id = {} LIMIT 1",
        templateId);
    if (!result)
        return std::nullopt;

    model::BotTalentTemplateRecord tmpl = BuildTemplate(result->Fetch());
    LoadTemplateEntries(tmpl);
    return tmpl;
}

std::optional<model::BotTalentTemplateRecord>
SqlBotTalentTemplateRepository::FindTemplateForSpec(
    std::string const& specKey,
    std::uint8_t classId,
    std::string const& variantKey) const
{
    std::string escapedSpec = specKey;
    WorldDatabase.EscapeString(escapedSpec);
    std::string escapedVariant = variantKey;
    WorldDatabase.EscapeString(escapedVariant);

    QueryResult result;
    if (escapedVariant.empty())
    {
        result = WorldDatabase.Query(
            "SELECT template_id, spec_key, class_id, display_name, variant_key, description "
            "FROM living_world_bot_talent_template "
            "WHERE LOWER(spec_key) = LOWER('{}') AND class_id = {} "
            "AND (variant_key IS NULL OR variant_key = '') "
            "LIMIT 1",
            escapedSpec, static_cast<std::uint32_t>(classId));
    }
    else
    {
        result = WorldDatabase.Query(
            "SELECT template_id, spec_key, class_id, display_name, variant_key, description "
            "FROM living_world_bot_talent_template "
            "WHERE LOWER(spec_key) = LOWER('{}') AND class_id = {} "
            "AND (LOWER(variant_key) = LOWER('{}') OR variant_key IS NULL OR variant_key = '') "
            "ORDER BY CASE "
            "WHEN LOWER(variant_key) = LOWER('{}') THEN 0 "
            "WHEN variant_key IS NULL OR variant_key = '' THEN 1 "
            "ELSE 2 END "
            "LIMIT 1",
            escapedSpec,
            static_cast<std::uint32_t>(classId),
            escapedVariant,
            escapedVariant);
    }

    if (!result)
        return std::nullopt;

    model::BotTalentTemplateRecord tmpl = BuildTemplate(result->Fetch());
    LoadTemplateEntries(tmpl);
    return tmpl;
}
} // namespace integration
} // namespace living_world
