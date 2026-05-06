#pragma once

#include "integration/BotTalentTemplateRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotTalentTemplateRepository final : public BotTalentTemplateRepository
{
public:
    std::vector<model::BotTalentTemplateRecord> ListTemplates() const override;

    std::optional<model::BotTalentTemplateRecord> FindTemplate(
        std::uint64_t templateId) const override;

    std::optional<model::BotTalentTemplateRecord> FindTemplateForSpec(
        std::string const& specKey,
        std::uint8_t classId) const override;

private:
    static model::BotTalentTemplateRecord BuildTemplate(Field const* fields);
    static void LoadTemplateEntries(model::BotTalentTemplateRecord& tmpl);
};
} // namespace integration
} // namespace living_world
