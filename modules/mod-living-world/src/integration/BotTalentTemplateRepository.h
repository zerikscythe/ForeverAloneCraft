#pragma once

#include "model/BotTalentTemplate.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
class BotTalentTemplateRepository
{
public:
    virtual ~BotTalentTemplateRepository() = default;

    virtual std::vector<model::BotTalentTemplateRecord> ListTemplates() const = 0;

    virtual std::optional<model::BotTalentTemplateRecord> FindTemplate(
        std::uint64_t templateId) const = 0;

    virtual std::optional<model::BotTalentTemplateRecord> FindTemplateForSpec(
        std::string const& specKey,
        std::uint8_t classId) const = 0;
};
} // namespace integration
} // namespace living_world
