#pragma once

#include "integration/BotGlyphTemplateRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotGlyphTemplateRepository final : public BotGlyphTemplateRepository
{
public:
    void EnsureSchema() const;

    std::vector<model::BotGlyphTemplateEntry> LoadTemplate(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey) const override;
};
} // namespace integration
} // namespace living_world
