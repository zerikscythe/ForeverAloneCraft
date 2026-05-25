#pragma once

#include "model/BotGlyphTemplate.h"

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace integration
{
class BotGlyphTemplateRepository
{
public:
    virtual ~BotGlyphTemplateRepository() = default;

    virtual std::vector<model::BotGlyphTemplateEntry> LoadTemplate(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey) const = 0;
};
} // namespace integration
} // namespace living_world
