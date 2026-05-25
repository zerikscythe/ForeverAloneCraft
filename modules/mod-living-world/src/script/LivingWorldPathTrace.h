#pragma once

#include "Player.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace living_world
{
namespace script
{
namespace pathtrace
{
struct Sample
{
    std::uint64_t elapsedMs = 0;
    std::uint64_t updateTick = 0;
    std::uint32_t mapId = 0;
    std::uint32_t zoneId = 0;
    std::uint32_t areaId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

struct Session
{
    std::uint32_t accountId = 0;
    std::uint64_t playerGuid = 0;
    std::string playerName;
    std::string traceLabel;
    std::chrono::system_clock::time_point startedAt = std::chrono::system_clock::now();
    std::uint64_t elapsedMs = 0;
    std::uint64_t updateTicks = 0;
    std::uint32_t pollEveryUpdates = 4;
    std::vector<Sample> samples;
};

struct StopResult
{
    bool hadActiveTrace = false;
    std::size_t sampleCount = 0;
    std::filesystem::path outputPath;
};

inline std::mutex g_mutex;
inline std::unordered_map<std::uint64_t, Session> g_sessions;

inline std::string JsonEscape(std::string_view value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value)
    {
        switch (c)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    std::ostringstream hex;
                    hex << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                    out += hex.str();
                }
                else
                {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

inline std::string SanitizeFileStem(std::string_view value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            out.push_back(c);
        else
            out.push_back('_');
    }

    if (out.empty())
        out = "player";

    return out;
}

inline std::string NormalizeTraceLabel(std::string_view value)
{
    auto begin = value.begin();
    auto end = value.end();

    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        ++begin;
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))))
        --end;

    if (begin == end)
        return {};

    if ((end - begin) >= 2)
    {
        char const first = *begin;
        char const last = *(end - 1);
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            ++begin;
            --end;
            while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
                ++begin;
            while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))))
                --end;
        }
    }

    return std::string(begin, end);
}

inline std::string FormatTimestampForFilename(std::chrono::system_clock::time_point tp)
{
    std::time_t const raw = std::chrono::system_clock::to_time_t(tp);
    std::tm tmValue {};
#if defined(_WIN32)
    localtime_s(&tmValue, &raw);
#else
    localtime_r(&raw, &tmValue);
#endif
    std::ostringstream out;
    out << std::put_time(&tmValue, "%Y%m%d-%H%M%S");
    return out.str();
}

inline std::string FormatTimestampIso8601(std::chrono::system_clock::time_point tp)
{
    std::time_t const raw = std::chrono::system_clock::to_time_t(tp);
    std::tm tmValue {};
#if defined(_WIN32)
    localtime_s(&tmValue, &raw);
#else
    localtime_r(&raw, &tmValue);
#endif
    std::ostringstream out;
    out << std::put_time(&tmValue, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

inline std::filesystem::path ResolveOutputDirectory()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::current_path(ec);
    if (ec)
        dir = ".";
    dir /= "path_traces";
    std::filesystem::create_directories(dir, ec);
    return dir;
}

inline void AppendSample(Session& session, Player* player)
{
    if (!player)
        return;

    Sample sample;
    sample.elapsedMs = session.elapsedMs;
    sample.updateTick = session.updateTicks;
    sample.mapId = player->GetMapId();
    sample.zoneId = player->GetZoneId();
    sample.areaId = player->GetAreaId();
    sample.x = player->GetPositionX();
    sample.y = player->GetPositionY();
    sample.z = player->GetPositionZ();
    sample.o = player->GetOrientation();
    session.samples.push_back(sample);
}

inline StopResult FlushAndStop(Player* player, std::string_view reason)
{
    StopResult result;
    if (!player)
        return result;

    Session session;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto itr = g_sessions.find(player->GetGUID().GetCounter());
        if (itr == g_sessions.end())
            return result;

        session = std::move(itr->second);
        g_sessions.erase(itr);
    }

    result.hadActiveTrace = true;
    result.sampleCount = session.samples.size();

    std::filesystem::path const outputDir = ResolveOutputDirectory();
    std::string fileStem = SanitizeFileStem(session.playerName);
    if (!session.traceLabel.empty())
        fileStem += "-" + SanitizeFileStem(session.traceLabel);
    fileStem += "-" + FormatTimestampForFilename(session.startedAt);

    std::filesystem::path const outputPath = outputDir / (fileStem + ".json");
    result.outputPath = outputPath;

    std::ofstream output(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
        return result;

    output << "{\n";
    output << "  \"player_name\": \"" << JsonEscape(session.playerName) << "\",\n";
    output << "  \"player_guid\": " << session.playerGuid << ",\n";
    output << "  \"account_id\": " << session.accountId << ",\n";
    output << "  \"trace_label\": \"" << JsonEscape(session.traceLabel) << "\",\n";
    output << "  \"started_at\": \"" << FormatTimestampIso8601(session.startedAt) << "\",\n";
    output << "  \"stopped_at\": \"" << FormatTimestampIso8601(std::chrono::system_clock::now()) << "\",\n";
    output << "  \"stop_reason\": \"" << JsonEscape(reason) << "\",\n";
    output << "  \"poll_every_updates\": " << session.pollEveryUpdates << ",\n";
    output << "  \"sample_count\": " << session.samples.size() << ",\n";
    output << "  \"samples\": [\n";
    for (std::size_t i = 0; i < session.samples.size(); ++i)
    {
        Sample const& sample = session.samples[i];
        output << "    {"
               << "\"elapsed_ms\": " << sample.elapsedMs
               << ", \"update_tick\": " << sample.updateTick
               << ", \"map_id\": " << sample.mapId
               << ", \"zone_id\": " << sample.zoneId
               << ", \"area_id\": " << sample.areaId
               << ", \"x\": " << sample.x
               << ", \"y\": " << sample.y
               << ", \"z\": " << sample.z
               << ", \"o\": " << sample.o
               << "}";
        if (i + 1 < session.samples.size())
            output << ",";
        output << "\n";
    }
    output << "  ]\n";
    output << "}\n";

    return result;
}

inline bool IsActive(Player* player)
{
    if (!player)
        return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    return g_sessions.find(player->GetGUID().GetCounter()) != g_sessions.end();
}

inline Session Start(Player* player, std::uint32_t pollEveryUpdates = 4, std::string_view traceLabel = {})
{
    Session session;
    if (!player || !player->GetSession())
        return session;

    session.accountId = player->GetSession()->GetAccountId();
    session.playerGuid = player->GetGUID().GetCounter();
    session.playerName = player->GetName();
    session.traceLabel = NormalizeTraceLabel(traceLabel);
    session.startedAt = std::chrono::system_clock::now();
    session.pollEveryUpdates = std::max<std::uint32_t>(1, pollEveryUpdates);
    AppendSample(session, player);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_sessions[player->GetGUID().GetCounter()] = session;
    return session;
}

inline void Update(Player* player, std::uint32_t diff)
{
    if (!player)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    auto itr = g_sessions.find(player->GetGUID().GetCounter());
    if (itr == g_sessions.end())
        return;

    Session& session = itr->second;
    session.elapsedMs += diff;
    ++session.updateTicks;
    if ((session.updateTicks % session.pollEveryUpdates) != 0)
        return;

    AppendSample(session, player);
}
} // namespace pathtrace
} // namespace script
} // namespace living_world
