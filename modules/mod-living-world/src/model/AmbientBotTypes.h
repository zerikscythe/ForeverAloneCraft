#pragma once

#include <cstdint>
#include <string>

namespace living_world
{
namespace model
{

struct ZoneEntry
{
    std::uint32_t zoneId     = 0;
    std::uint16_t mapId      = 0;
    std::string   zoneName;
    std::uint8_t  faction    = 0;  // 0=both 1=alliance 2=horde
    std::string   zoneType;        // "city","wilderness","contested"
    bool          hasHerbs   = false;
    bool          hasOre     = false;
    bool          hasFish    = false;
    std::uint8_t  minLevel   = 1;
    std::uint8_t  maxLevel   = 80;
    float         anchorX    = 0.f;
    float         anchorY    = 0.f;
    float         anchorZ    = 0.f;
};

struct ActivityEntry
{
    std::uint32_t activityId         = 0;
    std::string   activityKey;
    std::string   displayName;
    std::string   activityType;      // "patrol","gather_herb","gather_ore","fish","idle_city","idle_inn"
    std::uint32_t targetZoneId       = 0;
    std::uint8_t  requiredFaction    = 0;
    std::uint8_t  minLevel           = 1;
    std::uint8_t  maxLevel           = 80;
    bool          requiresHerbalism  = false;
    bool          requiresMining     = false;
    bool          requiresFishing    = false;
    std::uint8_t  weight             = 1;
    std::uint32_t durationMinSec     = 600;
    std::uint32_t durationMaxSec     = 1800;
};

} // namespace model
} // namespace living_world
