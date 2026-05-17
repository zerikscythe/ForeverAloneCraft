#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
    std::string   activityType;      // runtime primitive: patrol|gather_herb|gather_ore|fish|idle_city|idle_inn
    std::string   taskFamily;        // authoring family: city_errand|gathering|patrol|fishing|questing
    std::string   requiredZoneType;  // ""/any|city|wilderness|contested
    std::uint8_t  maxPerSession      = 1;
    std::uint8_t  openerBias         = 1;  // relative weight when choosing the first task in a chain
    std::uint8_t  followupBias       = 1;  // relative weight for subsequent picks
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

struct TaskTemplateStepEntry
{
    std::uint32_t stepOrder      = 0;
    std::string   stepType;          // travel|gather_herb|gather_ore|fish|idle_city|idle_inn|patrol|grind
    std::uint32_t targetZoneId   = 0;
    std::string   targetPointKey;
    std::string   resolverKind;      // point|zone|home_city|resource_auto|resource_zone|quest_auto|quest_zone|creature_auto|creature_zone
    std::string   subjectKind;       // ore|herb|fish|quest|creature|city_service
    std::uint32_t subjectId      = 0;
    std::string   subjectKey;
    std::string   returnAnchorRole;
    std::uint8_t  cycleCount     = 1;
    std::uint32_t durationMinSec = 0;
    std::uint32_t durationMaxSec = 0;
    std::string   label;
};

struct TaskPointEntry
{
    std::uint32_t pointId     = 0;
    std::string   pointKey;
    std::uint32_t zoneId      = 0;
    std::uint16_t mapId       = 0;
    std::string   pointType;      // bank|mailbox|auction_house|inn|trainer|vendor
    std::string   pointName;
    float         x           = 0.f;
    float         y           = 0.f;
    float         z           = 0.f;
};

struct ZoneAnchorEntry
{
    std::uint32_t anchorId        = 0;
    std::uint32_t zoneId          = 0;
    std::string   pointKey;
    std::string   anchorRole;
    std::uint8_t  requiredFaction = 0;
    std::uint8_t  minLevel        = 1;
    std::uint8_t  maxLevel        = 80;
    std::uint8_t  weight          = 1;
    std::string   notes;
};

struct ZoneContentEntry
{
    std::uint32_t contentId        = 0;
    std::uint32_t zoneId           = 0;
    std::string   contentKind;
    std::uint32_t subjectId        = 0;
    std::string   subjectKey;
    std::string   displayName;
    std::uint8_t  requiredFaction  = 0;
    std::uint8_t  minLevel         = 1;
    std::uint8_t  maxLevel         = 80;
    std::uint16_t minSkill         = 0;
    std::uint16_t maxSkill         = 0;
    std::uint8_t  weight           = 1;
    std::string   anchorPointKey;
    std::string   returnAnchorRole;
    std::string   notes;
};

struct TaskTransitRouteEntry
{
    std::uint32_t routeId          = 0;
    std::string   routeKey;
    std::string   sourcePointKey;
    std::string   destPointKey;
    std::string   transitType;      // taxi|boat|zeppelin|portal
    std::uint8_t  requiredFaction   = 0;
    std::uint8_t  minLevel          = 1;
    std::uint8_t  maxLevel          = 80;
    std::uint32_t durationSec       = 0;
    std::string   displayName;
    std::uint32_t sourceZoneId      = 0;
    std::uint16_t sourceMapId       = 0;
    std::string   sourcePointName;
    float         sourceX           = 0.f;
    float         sourceY           = 0.f;
    float         sourceZ           = 0.f;
    std::uint32_t destZoneId        = 0;
    std::uint16_t destMapId         = 0;
    std::string   destPointName;
    float         destX             = 0.f;
    float         destY             = 0.f;
    float         destZ             = 0.f;
};

struct TaskTemplateEntry
{
    std::uint32_t templateId        = 0;
    std::string   templateKey;
    std::string   displayName;
    std::string   taskFamily;        // gathering|patrol|city_errand|fishing
    std::uint8_t  requiredFaction   = 0;
    std::uint8_t  minLevel          = 1;
    std::uint8_t  maxLevel          = 80;
    bool          requiresHerbalism = false;
    bool          requiresMining    = false;
    bool          requiresFishing   = false;
    std::uint8_t  weight            = 1;
    std::vector<TaskTemplateStepEntry> steps;
};

struct PlaylistEntry
{
    std::uint32_t entryId        = 0;
    std::uint32_t entryOrder     = 0;
    std::uint32_t taskTemplateId = 0;
    std::uint8_t  repeatCount    = 1;
    std::string   note;
};

struct PlaylistEntryResolved
{
    PlaylistEntry       entry;
    TaskTemplateEntry   taskTemplate;
};

struct PlaylistEntrySummary
{
    std::uint32_t entryId          = 0;
    std::uint32_t entryOrder       = 0;
    std::uint32_t taskTemplateId   = 0;
    std::string   taskTemplateKey;
    std::string   taskTemplateName;
    std::uint8_t  repeatCount      = 1;
    std::string   note;
};

struct PlaylistEntryRef
{
    std::uint32_t entryOrder       = 0;
    std::uint32_t taskTemplateId   = 0;
    std::uint8_t  repeatCount      = 1;
    std::string   note;
};

struct PlaylistEntryResolvedSet
{
    std::uint32_t playlistId       = 0;
    std::string   playlistKey;
    std::string   displayName;
    std::vector<PlaylistEntryResolved> entries;
};

struct PlaylistEntrySet
{
    std::uint32_t playlistId        = 0;
    std::string   playlistKey;
    std::string   displayName;
    std::string   taskFamily;
    std::uint8_t  requiredFaction   = 0;
    std::uint8_t  minLevel          = 1;
    std::uint8_t  maxLevel          = 80;
    bool          requiresHerbalism = false;
    bool          requiresMining    = false;
    bool          requiresFishing   = false;
    std::uint8_t  weight            = 1;
    std::vector<PlaylistEntryRef> entries;
};

} // namespace model
} // namespace living_world
