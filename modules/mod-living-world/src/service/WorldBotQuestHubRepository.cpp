#include "service/WorldBotQuestHubRepository.h"

#include "Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string_view>

namespace living_world
{
namespace service
{
namespace
{

struct JsonValue
{
    enum class Type : std::uint8_t
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object,
    };

    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    Array arrayValue;
    Object objectValue;

    [[nodiscard]] bool IsNumber() const { return type == Type::Number; }
    [[nodiscard]] bool IsString() const { return type == Type::String; }
    [[nodiscard]] bool IsArray() const { return type == Type::Array; }
    [[nodiscard]] bool IsObject() const { return type == Type::Object; }
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view input)
        : _input(input)
    {
    }

    JsonValue Parse()
    {
        JsonValue value = ParseValue();
        SkipWhitespace();
        if (!AtEnd())
            throw std::runtime_error("Unexpected trailing JSON content");
        return value;
    }

private:
    JsonValue ParseValue()
    {
        SkipWhitespace();
        if (AtEnd())
            throw std::runtime_error("Unexpected end of JSON input");

        char const ch = Peek();
        if (ch == '{')
            return ParseObject();
        if (ch == '[')
            return ParseArray();
        if (ch == '"')
            return ParseString();
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)))
            return ParseNumber();
        if (StartsWith("true"))
            return ParseBoolean(true);
        if (StartsWith("false"))
            return ParseBoolean(false);
        if (StartsWith("null"))
            return ParseNull();

        throw std::runtime_error("Unsupported JSON token");
    }

    JsonValue ParseObject()
    {
        Consume('{');
        JsonValue value;
        value.type = JsonValue::Type::Object;

        SkipWhitespace();
        if (TryConsume('}'))
            return value;

        while (true)
        {
            JsonValue key = ParseString();
            SkipWhitespace();
            Consume(':');
            value.objectValue.emplace(std::move(key.stringValue), ParseValue());
            SkipWhitespace();
            if (TryConsume('}'))
                return value;
            Consume(',');
        }
    }

    JsonValue ParseArray()
    {
        Consume('[');
        JsonValue value;
        value.type = JsonValue::Type::Array;

        SkipWhitespace();
        if (TryConsume(']'))
            return value;

        while (true)
        {
            value.arrayValue.push_back(ParseValue());
            SkipWhitespace();
            if (TryConsume(']'))
                return value;
            Consume(',');
        }
    }

    JsonValue ParseString()
    {
        Consume('"');
        JsonValue value;
        value.type = JsonValue::Type::String;

        while (!AtEnd())
        {
            char const ch = Get();
            if (ch == '"')
                return value;

            if (ch == '\\')
            {
                if (AtEnd())
                    throw std::runtime_error("Unterminated JSON escape");

                char const escaped = Get();
                switch (escaped)
                {
                    case '"':
                    case '\\':
                    case '/':
                        value.stringValue.push_back(escaped);
                        break;
                    case 'b':
                        value.stringValue.push_back('\b');
                        break;
                    case 'f':
                        value.stringValue.push_back('\f');
                        break;
                    case 'n':
                        value.stringValue.push_back('\n');
                        break;
                    case 'r':
                        value.stringValue.push_back('\r');
                        break;
                    case 't':
                        value.stringValue.push_back('\t');
                        break;
                    default:
                        throw std::runtime_error("Unsupported JSON escape");
                }
                continue;
            }

            value.stringValue.push_back(ch);
        }

        throw std::runtime_error("Unterminated JSON string");
    }

    JsonValue ParseNumber()
    {
        std::size_t const start = _offset;
        if (Peek() == '-')
            ++_offset;

        while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
            ++_offset;

        if (!AtEnd() && Peek() == '.')
        {
            ++_offset;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
                ++_offset;
        }

        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E'))
        {
            ++_offset;
            if (!AtEnd() && (Peek() == '+' || Peek() == '-'))
                ++_offset;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
                ++_offset;
        }

        JsonValue value;
        value.type = JsonValue::Type::Number;
        value.numberValue = std::stod(std::string(_input.substr(start, _offset - start)));
        return value;
    }

    JsonValue ParseBoolean(bool boolValue)
    {
        _offset += boolValue ? 4u : 5u;
        JsonValue value;
        value.type = JsonValue::Type::Boolean;
        value.boolValue = boolValue;
        return value;
    }

    JsonValue ParseNull()
    {
        _offset += 4u;
        return {};
    }

    void SkipWhitespace()
    {
        while (!AtEnd() && std::isspace(static_cast<unsigned char>(_input[_offset])))
            ++_offset;
    }

    [[nodiscard]] bool StartsWith(char const* text) const
    {
        std::size_t const length = std::char_traits<char>::length(text);
        return _input.substr(_offset, length) == text;
    }

    void Consume(char expected)
    {
        SkipWhitespace();
        if (AtEnd() || _input[_offset] != expected)
            throw std::runtime_error("Unexpected JSON token");
        ++_offset;
    }

    [[nodiscard]] bool TryConsume(char expected)
    {
        SkipWhitespace();
        if (AtEnd() || _input[_offset] != expected)
            return false;
        ++_offset;
        return true;
    }

    [[nodiscard]] char Peek() const
    {
        return _input[_offset];
    }

    [[nodiscard]] char Get()
    {
        return _input[_offset++];
    }

    [[nodiscard]] bool AtEnd() const
    {
        return _offset >= _input.size();
    }

    std::string_view _input;
    std::size_t _offset = 0;
};

JsonValue const* FindObjectMember(JsonValue const& objectValue, std::string const& key)
{
    if (!objectValue.IsObject())
        return nullptr;

    auto const itr = objectValue.objectValue.find(key);
    return itr == objectValue.objectValue.end() ? nullptr : &itr->second;
}

std::string GetStringOrDefault(JsonValue const& objectValue, std::string const& key, std::string fallback = {})
{
    if (JsonValue const* value = FindObjectMember(objectValue, key); value && value->IsString())
        return value->stringValue;
    return fallback;
}

std::uint32_t GetUIntOrDefault(JsonValue const& objectValue, std::string const& key, std::uint32_t fallback = 0)
{
    if (JsonValue const* value = FindObjectMember(objectValue, key); value && value->IsNumber())
        return static_cast<std::uint32_t>(std::max(0.0, value->numberValue));
    return fallback;
}

float GetFloatOrDefault(JsonValue const& objectValue, std::string const& key, float fallback = 0.0f)
{
    if (JsonValue const* value = FindObjectMember(objectValue, key); value && value->IsNumber())
        return static_cast<float>(value->numberValue);
    return fallback;
}

std::uint8_t ParseFaction(std::string const& faction)
{
    std::string normalized;
    normalized.reserve(faction.size());
    for (char const ch : faction)
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

    if (normalized == "alliance")
        return 1;
    if (normalized == "horde")
        return 2;
    return 0;
}

bool HubMatchesFactionAndLevel(WorldBotQuestHub const& hub, std::uint8_t faction, std::uint8_t level)
{
    if (hub.requiredFaction != 0 && faction != 0 && hub.requiredFaction != faction)
        return false;

    std::int32_t const levelValue = static_cast<std::int32_t>(level);
    std::int32_t const minAllowed = std::max<std::int32_t>(1, static_cast<std::int32_t>(hub.minLevel) - 3);
    std::int32_t const maxAllowed = std::min<std::int32_t>(80, static_cast<std::int32_t>(hub.maxLevel) + 3);
    return levelValue >= minAllowed && levelValue <= maxAllowed;
}

WorldBotQuestHubTaskArea ParseTaskArea(JsonValue const& areaValue, std::uint16_t fallbackMapId)
{
    WorldBotQuestHubTaskArea area;
    area.taskAreaId = GetStringOrDefault(areaValue, "taskAreaId");
    area.kind = GetStringOrDefault(areaValue, "kind");
    area.weight = std::max<std::uint32_t>(1u, GetUIntOrDefault(areaValue, "weight", 1u));
    area.radius = GetFloatOrDefault(areaValue, "radius", 35.0f);

    if (JsonValue const* positionValue = FindObjectMember(areaValue, "position"); positionValue && positionValue->IsObject())
    {
        area.mapId = static_cast<std::uint16_t>(GetUIntOrDefault(*positionValue, "mapId", fallbackMapId));
        area.x = GetFloatOrDefault(*positionValue, "x");
        area.y = GetFloatOrDefault(*positionValue, "y");
        area.z = GetFloatOrDefault(*positionValue, "z");
    }
    else
    {
        area.mapId = fallbackMapId;
    }

    if (JsonValue const* targetEntries = FindObjectMember(areaValue, "targetEntries"); targetEntries && targetEntries->IsArray())
    {
        for (JsonValue const& entryValue : targetEntries->arrayValue)
        {
            if (!entryValue.IsNumber())
                continue;
            std::uint32_t const entry = static_cast<std::uint32_t>(std::max(0.0, entryValue.numberValue));
            if (entry != 0)
                area.targetEntries.push_back(entry);
        }
    }

    return area;
}

WorldBotQuestHubBranch ParseBranch(JsonValue const& branchValue, std::uint16_t fallbackMapId)
{
    WorldBotQuestHubBranch branch;
    branch.hubId = GetStringOrDefault(branchValue, "hubId");
    branch.zoneId = GetUIntOrDefault(branchValue, "zoneId");
    branch.zoneName = GetStringOrDefault(branchValue, "zoneName");
    branch.weight = std::max<std::uint32_t>(1u, GetUIntOrDefault(branchValue, "weight", 1u));

    if (JsonValue const* positionValue = FindObjectMember(branchValue, "position"); positionValue && positionValue->IsObject())
    {
        branch.mapId = static_cast<std::uint16_t>(GetUIntOrDefault(*positionValue, "mapId", fallbackMapId));
        branch.x = GetFloatOrDefault(*positionValue, "x");
        branch.y = GetFloatOrDefault(*positionValue, "y");
        branch.z = GetFloatOrDefault(*positionValue, "z");
    }
    else
    {
        branch.mapId = fallbackMapId;
    }

    return branch;
}

std::vector<WorldBotQuestHub> ParseQuestHubFile(std::filesystem::path const& filePath)
{
    std::ifstream input(filePath);
    if (!input)
        return {};

    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (content.empty())
        return {};

    JsonValue const root = JsonParser(content).Parse();
    if (!root.IsObject())
        return {};

    std::uint32_t const zoneId = GetUIntOrDefault(root, "zoneId");
    std::string const zoneName = GetStringOrDefault(root, "zoneName");
    std::uint16_t const mapId = static_cast<std::uint16_t>(GetUIntOrDefault(root, "mapId"));

    std::vector<WorldBotQuestHub> hubs;
    JsonValue const* hubsValue = FindObjectMember(root, "hubs");
    if (!hubsValue || !hubsValue->IsArray())
        return hubs;

    for (JsonValue const& hubValue : hubsValue->arrayValue)
    {
        if (!hubValue.IsObject())
            continue;

        WorldBotQuestHub hub;
        hub.hubId = GetStringOrDefault(hubValue, "hubId");
        hub.zoneId = zoneId;
        hub.zoneName = zoneName;
        hub.requiredFaction = ParseFaction(GetStringOrDefault(hubValue, "faction"));
        hub.totalQuests = GetUIntOrDefault(hubValue, "totalQuests");
        hub.estimatedMinutes = std::max<std::uint32_t>(5u, GetUIntOrDefault(hubValue, "estimatedMinutes", 10u));

        if (JsonValue const* levelRange = FindObjectMember(hubValue, "levelRange"); levelRange && levelRange->IsObject())
        {
            hub.minLevel = static_cast<std::uint8_t>(GetUIntOrDefault(*levelRange, "min", 1u));
            hub.maxLevel = static_cast<std::uint8_t>(GetUIntOrDefault(*levelRange, "max", 80u));
            hub.avgLevel = static_cast<std::uint8_t>(GetUIntOrDefault(*levelRange, "avg", hub.minLevel));
        }

        if (JsonValue const* positionValue = FindObjectMember(hubValue, "position"); positionValue && positionValue->IsObject())
        {
            hub.mapId = static_cast<std::uint16_t>(GetUIntOrDefault(*positionValue, "mapId", mapId));
            hub.x = GetFloatOrDefault(*positionValue, "x");
            hub.y = GetFloatOrDefault(*positionValue, "y");
            hub.z = GetFloatOrDefault(*positionValue, "z");
        }
        else
        {
            hub.mapId = mapId;
        }

        if (JsonValue const* taskAreasValue = FindObjectMember(hubValue, "taskAreas"); taskAreasValue && taskAreasValue->IsArray())
        {
            for (JsonValue const& areaValue : taskAreasValue->arrayValue)
            {
                if (!areaValue.IsObject())
                    continue;
                hub.taskAreas.push_back(ParseTaskArea(areaValue, hub.mapId));
            }
        }

        if (JsonValue const* nextHubsValue = FindObjectMember(hubValue, "nextHubs"); nextHubsValue && nextHubsValue->IsArray())
        {
            for (JsonValue const& branchValue : nextHubsValue->arrayValue)
            {
                if (!branchValue.IsObject())
                    continue;
                hub.nextHubs.push_back(ParseBranch(branchValue, hub.mapId));
            }
        }

        if (!hub.hubId.empty())
            hubs.push_back(std::move(hub));
    }

    return hubs;
}

} // namespace

WorldBotQuestHubRepository::WorldBotQuestHubRepository(std::filesystem::path exportRoot)
    : _exportRoot(std::move(exportRoot))
{
}

std::vector<WorldBotQuestHub> WorldBotQuestHubRepository::LoadAll() const
{
    std::vector<WorldBotQuestHub> hubs;
    std::error_code error;
    if (_exportRoot.empty() || !std::filesystem::exists(_exportRoot, error))
        return hubs;

    for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator(_exportRoot, error))
    {
        if (error)
            break;
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;

        try
        {
            std::vector<WorldBotQuestHub> parsed = ParseQuestHubFile(entry.path());
            hubs.insert(hubs.end(), parsed.begin(), parsed.end());
        }
        catch (std::exception const& ex)
        {
            LOG_ERROR("module", "LivingWorld: failed parsing quest hub export '{}': {}", entry.path().string(), ex.what());
        }
    }

    return hubs;
}

std::optional<WorldBotQuestHub> WorldBotQuestHubRepository::FindHubById(std::string const& hubId) const
{
    if (hubId.empty())
        return std::nullopt;

    std::vector<WorldBotQuestHub> const hubs = LoadAll();
    auto const itr = std::find_if(
        hubs.begin(),
        hubs.end(),
        [&](WorldBotQuestHub const& hub)
        {
            return hub.hubId == hubId;
        });

    if (itr == hubs.end())
        return std::nullopt;
    return *itr;
}

std::vector<WorldBotQuestHub> WorldBotQuestHubRepository::LoadEligibleHubsForZone(
    std::uint32_t zoneId,
    std::uint8_t faction,
    std::uint8_t level) const
{
    std::vector<WorldBotQuestHub> hubs = LoadAll();
    hubs.erase(
        std::remove_if(
            hubs.begin(),
            hubs.end(),
            [&](WorldBotQuestHub const& hub)
            {
                return hub.zoneId != zoneId || !HubMatchesFactionAndLevel(hub, faction, level);
            }),
        hubs.end());
    return hubs;
}

std::vector<WorldBotQuestHub> WorldBotQuestHubRepository::LoadEligibleHubs(
    std::uint8_t faction,
    std::uint8_t level) const
{
    std::vector<WorldBotQuestHub> hubs = LoadAll();
    hubs.erase(
        std::remove_if(
            hubs.begin(),
            hubs.end(),
            [&](WorldBotQuestHub const& hub)
            {
                return !HubMatchesFactionAndLevel(hub, faction, level);
            }),
        hubs.end());
    return hubs;
}

} // namespace service
} // namespace living_world
