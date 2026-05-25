#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace model
{
enum class WorldBotSelfStateCategory : std::uint8_t
{
    Unknown = 0,
    Form = 1,
    Stance = 2,
    Presence = 3,
    Aspect = 4,
    Armor = 5,
    Seal = 6,
};

struct WorldBotPreparedSelfState
{
    WorldBotSelfStateCategory category = WorldBotSelfStateCategory::Unknown;
    std::string key;
    std::uint32_t spellId = 0;
    std::uint32_t activeAuraSpellId = 0;
    std::uint8_t shapeshiftForm = 0;
    bool preferredInCombat = false;
    bool preferredOutOfCombat = false;
    bool preferredWhileTraveling = false;
};

using WorldBotPreparedSelfStates = std::vector<WorldBotPreparedSelfState>;
} // namespace model
} // namespace living_world
