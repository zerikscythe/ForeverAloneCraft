#pragma once

#include "model/AmbientBotTypes.h"

namespace living_world
{
namespace service
{

struct AmbientProfessionCapabilities
{
    bool hasHerbalism = false;
    bool hasMining = false;
    bool hasFishing = false;
};

inline bool MeetsProfessionRequirements(
    model::ActivityEntry const& entry,
    AmbientProfessionCapabilities const& capabilities)
{
    return (!entry.requiresHerbalism || capabilities.hasHerbalism)
        && (!entry.requiresMining || capabilities.hasMining)
        && (!entry.requiresFishing || capabilities.hasFishing);
}

inline bool MeetsProfessionRequirements(
    model::TaskTemplateEntry const& entry,
    AmbientProfessionCapabilities const& capabilities)
{
    return (!entry.requiresHerbalism || capabilities.hasHerbalism)
        && (!entry.requiresMining || capabilities.hasMining)
        && (!entry.requiresFishing || capabilities.hasFishing);
}

inline bool MeetsProfessionRequirements(
    model::PlaylistEntrySet const& entry,
    AmbientProfessionCapabilities const& capabilities)
{
    return (!entry.requiresHerbalism || capabilities.hasHerbalism)
        && (!entry.requiresMining || capabilities.hasMining)
        && (!entry.requiresFishing || capabilities.hasFishing);
}

} // namespace service
} // namespace living_world