#include "integration/SqlTaskTemplateRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

#include <algorithm>
#include <unordered_map>

namespace living_world
{
namespace integration
{
namespace
{
model::TaskTemplateEntry BuildTemplate(Field const* f)
{
    model::TaskTemplateEntry e;
    e.templateId        = f[0].Get<std::uint32_t>();
    e.templateKey       = f[1].Get<std::string>();
    e.displayName       = f[2].Get<std::string>();
    e.taskFamily        = f[3].Get<std::string>();
    e.requiredFaction   = f[4].Get<std::uint8_t>();
    e.minLevel          = f[5].Get<std::uint8_t>();
    e.maxLevel          = f[6].Get<std::uint8_t>();
    e.requiresHerbalism = f[7].Get<std::uint8_t>() != 0;
    e.requiresMining    = f[8].Get<std::uint8_t>() != 0;
    e.requiresFishing   = f[9].Get<std::uint8_t>() != 0;
    e.weight            = f[10].Get<std::uint8_t>();
    return e;
}

model::TaskTemplateStepEntry BuildStep(Field const* f)
{
    model::TaskTemplateStepEntry s;
    s.stepOrder      = f[1].Get<std::uint32_t>();
    s.stepType       = f[2].Get<std::string>();
    s.targetZoneId   = f[3].Get<std::uint32_t>();
    s.targetPointKey = f[4].IsNull() ? "" : f[4].Get<std::string>();
    s.resolverKind   = f[5].IsNull() ? "" : f[5].Get<std::string>();
    s.subjectKind    = f[6].IsNull() ? "" : f[6].Get<std::string>();
    s.subjectId      = f[7].IsNull() ? 0u : f[7].Get<std::uint32_t>();
    s.subjectKey     = f[8].IsNull() ? "" : f[8].Get<std::string>();
    s.returnAnchorRole = f[9].IsNull() ? "" : f[9].Get<std::string>();
    s.cycleCount     = f[10].Get<std::uint8_t>();
    s.durationMinSec = f[11].Get<std::uint32_t>();
    s.durationMaxSec = f[12].Get<std::uint32_t>();
    s.label          = f[13].Get<std::string>();
    return s;
}

model::PlaylistEntrySet BuildPlaylist(Field const* f)
{
    model::PlaylistEntrySet p;
    p.playlistId         = f[0].Get<std::uint32_t>();
    p.playlistKey        = f[1].Get<std::string>();
    p.displayName        = f[2].Get<std::string>();
    p.taskFamily         = f[3].Get<std::string>();
    p.requiredFaction    = f[4].Get<std::uint8_t>();
    p.minLevel           = f[5].Get<std::uint8_t>();
    p.maxLevel           = f[6].Get<std::uint8_t>();
    p.requiresHerbalism  = f[7].Get<std::uint8_t>() != 0;
    p.requiresMining     = f[8].Get<std::uint8_t>() != 0;
    p.requiresFishing    = f[9].Get<std::uint8_t>() != 0;
    p.weight             = f[10].Get<std::uint8_t>();
    return p;
}

model::PlaylistEntryRef BuildPlaylistEntry(Field const* f)
{
    model::PlaylistEntryRef e;
    e.entryOrder      = f[1].Get<std::uint32_t>();
    e.taskTemplateId  = f[2].Get<std::uint32_t>();
    e.repeatCount     = f[3].Get<std::uint8_t>();
    e.note            = f[4].IsNull() ? "" : f[4].Get<std::string>();
    return e;
}
} // namespace

std::vector<model::TaskTemplateEntry> SqlTaskTemplateRepository::LoadEligible(
    std::uint8_t faction,
    std::uint8_t level,
    bool hasHerbalism,
    bool hasMining,
    bool hasFishing) const
{
    QueryResult templateQr = WorldDatabase.Query(
        "SELECT template_id, template_key, display_name, task_family, "
        "required_faction, min_level, max_level, requires_herbalism, "
        "requires_mining, requires_fishing, weight "
        "FROM living_world_task_template "
        "WHERE is_enabled = 1 "
        "  AND (required_faction = 0 OR required_faction = {}) "
        "  AND min_level <= {} AND max_level >= {} "
        "  AND (requires_herbalism = 0 OR {} = 1) "
        "  AND (requires_mining    = 0 OR {} = 1) "
        "  AND (requires_fishing   = 0 OR {} = 1)",
        faction,
        level,
        level,
        static_cast<int>(hasHerbalism),
        static_cast<int>(hasMining),
        static_cast<int>(hasFishing));

    std::vector<model::TaskTemplateEntry> result;
    if (!templateQr)
        return result;

    std::unordered_map<std::uint32_t, std::size_t> indexById;
    do
    {
        model::TaskTemplateEntry tmpl = BuildTemplate(templateQr->Fetch());
        indexById.emplace(tmpl.templateId, result.size());
        result.push_back(std::move(tmpl));
    } while (templateQr->NextRow());

    if (result.empty())
        return result;

    QueryResult stepQr = WorldDatabase.Query(
        "SELECT template_id, step_order, step_type, target_zone_id, target_point_key, "
        "resolver_kind, subject_kind, subject_id, subject_key, return_anchor_role, cycle_count, "
        "duration_min_sec, duration_max_sec, label "
        "FROM living_world_task_template_step "
        "WHERE template_id IN ("
        "  SELECT template_id FROM living_world_task_template "
        "  WHERE is_enabled = 1 "
        "    AND (required_faction = 0 OR required_faction = {}) "
        "    AND min_level <= {} AND max_level >= {} "
        "    AND (requires_herbalism = 0 OR {} = 1) "
        "    AND (requires_mining    = 0 OR {} = 1) "
        "    AND (requires_fishing   = 0 OR {} = 1)) "
        "ORDER BY template_id ASC, step_order ASC",
        faction,
        level,
        level,
        static_cast<int>(hasHerbalism),
        static_cast<int>(hasMining),
        static_cast<int>(hasFishing));

    if (!stepQr)
        return result;

    do
    {
        Field const* f = stepQr->Fetch();
        std::uint32_t const templateId = f[0].Get<std::uint32_t>();
        auto const itr = indexById.find(templateId);
        if (itr == indexById.end())
            continue;

        result[itr->second].steps.push_back(BuildStep(f));
    } while (stepQr->NextRow());

    result.erase(
        std::remove_if(result.begin(), result.end(),
            [](model::TaskTemplateEntry const& tmpl)
            {
                return tmpl.steps.empty();
            }),
        result.end());

    return result;
}

std::vector<model::PlaylistEntrySet> SqlTaskTemplateRepository::LoadEligiblePlaylists(
    std::uint8_t faction,
    std::uint8_t level,
    bool hasHerbalism,
    bool hasMining,
    bool hasFishing) const
{
    QueryResult playlistQr = WorldDatabase.Query(
        "SELECT playlist_id, playlist_key, display_name, task_family, "
        "required_faction, min_level, max_level, requires_herbalism, "
        "requires_mining, requires_fishing, weight "
        "FROM living_world_playlist "
        "WHERE is_enabled = 1 "
        "  AND (required_faction = 0 OR required_faction = {}) "
        "  AND min_level <= {} AND max_level >= {} "
        "  AND (requires_herbalism = 0 OR {} = 1) "
        "  AND (requires_mining    = 0 OR {} = 1) "
        "  AND (requires_fishing   = 0 OR {} = 1)",
        faction,
        level,
        level,
        static_cast<int>(hasHerbalism),
        static_cast<int>(hasMining),
        static_cast<int>(hasFishing));

    std::vector<model::PlaylistEntrySet> result;
    if (!playlistQr)
        return result;

    std::unordered_map<std::uint32_t, std::size_t> indexById;
    do
    {
        model::PlaylistEntrySet playlist = BuildPlaylist(playlistQr->Fetch());
        indexById.emplace(playlist.playlistId, result.size());
        result.push_back(std::move(playlist));
    } while (playlistQr->NextRow());

    if (result.empty())
        return result;

    QueryResult entryQr = WorldDatabase.Query(
        "SELECT playlist_id, entry_order, task_template_id, repeat_count, note "
        "FROM living_world_playlist_entry "
        "WHERE playlist_id IN ("
        "  SELECT playlist_id FROM living_world_playlist "
        "  WHERE is_enabled = 1 "
        "    AND (required_faction = 0 OR required_faction = {}) "
        "    AND min_level <= {} AND max_level >= {} "
        "    AND (requires_herbalism = 0 OR {} = 1) "
        "    AND (requires_mining    = 0 OR {} = 1) "
        "    AND (requires_fishing   = 0 OR {} = 1)) "
        "ORDER BY playlist_id ASC, entry_order ASC",
        faction,
        level,
        level,
        static_cast<int>(hasHerbalism),
        static_cast<int>(hasMining),
        static_cast<int>(hasFishing));

    if (!entryQr)
        return result;

    do
    {
        Field const* f = entryQr->Fetch();
        std::uint32_t const playlistId = f[0].Get<std::uint32_t>();
        auto const itr = indexById.find(playlistId);
        if (itr == indexById.end())
            continue;

        result[itr->second].entries.push_back(BuildPlaylistEntry(f));
    } while (entryQr->NextRow());

    result.erase(
        std::remove_if(result.begin(), result.end(),
            [](model::PlaylistEntrySet const& playlist)
            {
                return playlist.entries.empty();
            }),
        result.end());

    return result;
}

} // namespace integration
} // namespace living_world