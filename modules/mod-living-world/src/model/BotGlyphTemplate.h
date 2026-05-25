#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{
struct BotGlyphTemplateEntry
{
    std::uint8_t slotIndex = 0;
    std::uint32_t glyphSpellId = 0;
};

struct WorldBotPreparedGlyphEntry
{
    std::uint8_t slotIndex = 0;
    std::uint32_t glyphId = 0;
    std::uint32_t spellId = 0;
};
} // namespace model
} // namespace living_world
