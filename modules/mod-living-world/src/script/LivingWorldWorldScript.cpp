#include "Config.h"
#include "DatabaseEnv.h"
#include "IWorld.h"
#include "Log.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Transport.h"
#include "WorldSession.h"
#include "ai/AbstractWorldBotProgressor.h"
#include "ai/WorldBotCreatureAI.h"
#include "script/AmbientSpawnOverride.h"
#include "script/WorldBotMaterializationIdentity.h"
#include "script/WorldBotHotZoneTracker.h"
#include "integration/BotActivityLog.h"
#include "integration/SqlActivityLibraryRepository.h"
#include "integration/SqlBotIdentityRepository.h"
#include "integration/SqlBotExploredZoneRepository.h"
#include "integration/SqlBotGlobalConfigRepository.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "integration/SqlBotAssignedGearRepository.h"
#include "integration/SqlBotOocConfigRepository.h"
#include "integration/SqlBotTalentPreferenceRepository.h"
#include "integration/SqlTaskPointRepository.h"
#include "integration/SqlZoneIndexRepository.h"
#include "QueryResult.h"
#include "service/BotActivitySessionComposer.h"
#include "service/BotQuestRewardService.h"
#include "service/WorldBotRoutePlanning.h"
#include "service/WorldBotTaxiPlanning.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <map>
#include <optional>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace living_world
{
// Live economy scale. Initialised from config at startup and updated by the
// .lw economy command. All cost hooks read this instead of hitting ConfigMgr
// on every transaction, so the command takes effect immediately.
float g_economyScale = 1.0f;

void ApplyEconomyScale(float scale, bool isReload)
{
    if (scale <= 0.0f)
    {
        LOG_ERROR("server.worldserver",
            "[LivingWorld] EconomyScale must be > 0 (got {}). Keeping current value.",
            scale);
        return;
    }

    g_economyScale = scale;

    // Repair cost is read dynamically on every repair — takes effect now.
    sWorld->setRate(RATE_REPAIRCOST, scale);

    LOG_INFO("server.worldserver",
        "[LivingWorld] EconomyScale={}{} applied.",
        scale,
        isReload
            ? " (reload — repairs + trainers live; vendor prices need restart)"
            : "");
}
} // namespace living_world

namespace
{
std::filesystem::path ResolveWorldBotRouteExportRoot()
{
    std::string configured = sConfigMgr->GetOption<std::string>(
        "LivingWorld.RouteExportDir",
        "data/worldbot_routes");

    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(configured);
    if (std::filesystem::path(configured).is_relative())
    {
        candidates.emplace_back(std::filesystem::path("..") / configured);
        candidates.emplace_back(std::filesystem::path("..") / ".." / configured);
    }

    // Deployment-first fallbacks: prefer a server-local route bundle placed
    // alongside the worldserver environment before falling back to editor paths.
    candidates.emplace_back("data/worldbot_routes");
    candidates.emplace_back(std::filesystem::path("..") / "data" / "worldbot_routes");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "data" / "worldbot_routes");
    candidates.emplace_back("tools/lw-zone-editor/data/exported_routes");
    candidates.emplace_back(std::filesystem::path("..") / "tools" / "lw-zone-editor" / "data" / "exported_routes");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "tools" / "lw-zone-editor" / "data" / "exported_routes");

    for (std::filesystem::path const& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec))
            return candidate;
    }

    return std::filesystem::path(configured);
}

living_world::service::WorldBotRoutePlanner& GetWorldBotRoutePlanner()
{
    static living_world::service::WorldBotRoutePlanner planner(ResolveWorldBotRouteExportRoot());
    return planner;
}

living_world::service::WorldBotTaxiNetwork& GetWorldBotTaxiNetwork()
{
    static living_world::service::WorldBotTaxiNetwork network =
        living_world::service::LoadWorldBotTaxiNetwork();
    return network;
}

struct SessionCompletionMetadata
{
    std::string   sourceKind;
    std::string   sourceKey;
    std::string   taskFamily;
    std::uint32_t targetZoneId = 0;
};

SessionCompletionMetadata BuildSessionCompletionMetadata(
    living_world::service::AmbientSession const& session,
    std::size_t currentStep)
{
    SessionCompletionMetadata metadata;
    metadata.sourceKind = session.sourceKind;
    metadata.sourceKey = session.sourceKey.empty() ? session.activityKey : session.sourceKey;

    if (session.steps.empty() || session.tasks.empty())
        return metadata;

    std::size_t stepIndex = session.steps.size() - 1;
    if (currentStep < session.steps.size())
        stepIndex = currentStep;

    while (true)
    {
        living_world::service::AmbientStep const& step = session.steps[stepIndex];
        if (step.taskIndex >= 0)
        {
            std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
            if (taskIndex < session.tasks.size())
            {
                auto const& task = session.tasks[taskIndex];
                metadata.taskFamily = task.taskFamily;
                metadata.targetZoneId = task.targetZoneId;
                break;
            }
        }

        if (stepIndex == 0)
            break;
        --stepIndex;
    }

    return metadata;
}

std::vector<std::uint32_t> ParseDebugIdentityIdList(std::string const& value);

struct SimpleJsonValue
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

    using Array = std::vector<SimpleJsonValue>;
    using Object = std::map<std::string, SimpleJsonValue>;

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    Array arrayValue;
    Object objectValue;

    [[nodiscard]] bool IsNull() const { return type == Type::Null; }
    [[nodiscard]] bool IsNumber() const { return type == Type::Number; }
    [[nodiscard]] bool IsString() const { return type == Type::String; }
    [[nodiscard]] bool IsArray() const { return type == Type::Array; }
    [[nodiscard]] bool IsObject() const { return type == Type::Object; }
};

class SimpleJsonParser
{
public:
    explicit SimpleJsonParser(std::string_view input)
        : _input(input)
    {
    }

    SimpleJsonValue Parse()
    {
        SimpleJsonValue value = ParseValue();
        SkipWhitespace();
        if (!AtEnd())
            throw std::runtime_error("Unexpected trailing JSON content");
        return value;
    }

private:
    SimpleJsonValue ParseValue()
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

    SimpleJsonValue ParseObject()
    {
        Consume('{');
        SimpleJsonValue value;
        value.type = SimpleJsonValue::Type::Object;

        SkipWhitespace();
        if (TryConsume('}'))
            return value;

        while (true)
        {
            SimpleJsonValue key = ParseString();
            SkipWhitespace();
            Consume(':');
            value.objectValue.emplace(std::move(key.stringValue), ParseValue());
            SkipWhitespace();
            if (TryConsume('}'))
                return value;
            Consume(',');
        }
    }

    SimpleJsonValue ParseArray()
    {
        Consume('[');
        SimpleJsonValue value;
        value.type = SimpleJsonValue::Type::Array;

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

    SimpleJsonValue ParseString()
    {
        Consume('"');
        SimpleJsonValue value;
        value.type = SimpleJsonValue::Type::String;

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

    SimpleJsonValue ParseNumber()
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

        SimpleJsonValue value;
        value.type = SimpleJsonValue::Type::Number;
        value.numberValue = std::stod(std::string(_input.substr(start, _offset - start)));
        return value;
    }

    SimpleJsonValue ParseBoolean(bool boolValue)
    {
        _offset += boolValue ? 4u : 5u;
        SimpleJsonValue value;
        value.type = SimpleJsonValue::Type::Boolean;
        value.boolValue = boolValue;
        return value;
    }

    SimpleJsonValue ParseNull()
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

SimpleJsonValue const* TryGetJsonObjectMember(SimpleJsonValue const& object, std::string const& key)
{
    if (!object.IsObject())
        return nullptr;

    auto const itr = object.objectValue.find(key);
    if (itr == object.objectValue.end())
        return nullptr;
    return &itr->second;
}

double GetJsonNumberOrDefault(SimpleJsonValue const& object, std::string const& key, double fallback = 0.0)
{
    SimpleJsonValue const* value = TryGetJsonObjectMember(object, key);
    if (!value || !value->IsNumber())
        return fallback;
    return value->numberValue;
}

std::string GetJsonStringOrDefault(SimpleJsonValue const& object, std::string const& key)
{
    SimpleJsonValue const* value = TryGetJsonObjectMember(object, key);
    if (!value || !value->IsString())
        return {};
    return value->stringValue;
}

struct TransitionNodeRecord
{
    std::uint16_t mapId = 0;
    std::uint32_t zoneId = 0;
    std::string zoneName;
    std::string pathKey;
    std::int32_t pathIndex = -1;
    std::int32_t anchorIndex = -1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::uint32_t targetZoneId = 0;
};

struct GeneratedConnectorRecord
{
    std::uint16_t mapId = 0;
    std::string connectorKey;
    std::uint32_t fromZoneId = 0;
    std::uint32_t toZoneId = 0;
    std::string fromZoneName;
    std::string toZoneName;
    std::string fromPathKey;
    std::string toPathKey;
    std::int32_t fromAnchorIndex = -1;
    std::int32_t toAnchorIndex = -1;
    float fromX = 0.0f;
    float fromY = 0.0f;
    float fromZ = 0.0f;
    float toX = 0.0f;
    float toY = 0.0f;
    float toZ = 0.0f;
};

std::vector<GeneratedConnectorRecord> ParseExistingConnectorRecords(std::string const& payload)
{
    std::vector<GeneratedConnectorRecord> connectors;

    SimpleJsonValue const root = SimpleJsonParser(payload).Parse();
    if (!root.IsObject())
        return connectors;

    std::uint16_t const mapId = static_cast<std::uint16_t>(GetJsonNumberOrDefault(root, "map_id"));
    SimpleJsonValue const* connectorsValue = TryGetJsonObjectMember(root, "connectors");
    if (!connectorsValue || !connectorsValue->IsArray())
        return connectors;

    for (SimpleJsonValue const& connectorValue : connectorsValue->arrayValue)
    {
        if (!connectorValue.IsObject())
            continue;

        SimpleJsonValue const* fromValue = TryGetJsonObjectMember(connectorValue, "from");
        SimpleJsonValue const* toValue = TryGetJsonObjectMember(connectorValue, "to");
        if (!fromValue || !fromValue->IsObject() || !toValue || !toValue->IsObject())
            continue;

        GeneratedConnectorRecord connector;
        connector.mapId = mapId;
        connector.connectorKey = GetJsonStringOrDefault(connectorValue, "connector_key");
        connector.fromZoneId = static_cast<std::uint32_t>(GetJsonNumberOrDefault(connectorValue, "from_zone_id"));
        connector.toZoneId = static_cast<std::uint32_t>(GetJsonNumberOrDefault(connectorValue, "to_zone_id"));
        connector.fromPathKey = GetJsonStringOrDefault(connectorValue, "from_path_key");
        connector.toPathKey = GetJsonStringOrDefault(connectorValue, "to_path_key");
        connector.fromAnchorIndex = static_cast<std::int32_t>(GetJsonNumberOrDefault(connectorValue, "from_anchor_index", -1.0));
        connector.toAnchorIndex = static_cast<std::int32_t>(GetJsonNumberOrDefault(connectorValue, "to_anchor_index", -1.0));
        connector.fromX = static_cast<float>(GetJsonNumberOrDefault(*fromValue, "world_x"));
        connector.fromY = static_cast<float>(GetJsonNumberOrDefault(*fromValue, "world_y"));
        connector.fromZ = static_cast<float>(GetJsonNumberOrDefault(*fromValue, "world_z"));
        connector.toX = static_cast<float>(GetJsonNumberOrDefault(*toValue, "world_x"));
        connector.toY = static_cast<float>(GetJsonNumberOrDefault(*toValue, "world_y"));
        connector.toZ = static_cast<float>(GetJsonNumberOrDefault(*toValue, "world_z"));
        if (connector.fromZoneId == 0 || connector.toZoneId == 0 || connector.connectorKey.empty())
            continue;
        connectors.push_back(std::move(connector));
    }

    return connectors;
}

float Distance2D(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    return std::sqrt((dx * dx) + (dy * dy));
}

std::string SlugifyTransitionLabel(std::string value)
{
    for (char& ch : value)
    {
        if (std::isalnum(static_cast<unsigned char>(ch)))
            continue;
        ch = '_';
    }

    std::string normalized;
    normalized.reserve(value.size());
    bool lastUnderscore = false;
    for (char ch : value)
    {
        if (ch == '_')
        {
            if (!lastUnderscore)
                normalized.push_back(ch);
            lastUnderscore = true;
            continue;
        }

        normalized.push_back(ch);
        lastUnderscore = false;
    }

    while (!normalized.empty() && normalized.front() == '_')
        normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '_')
        normalized.pop_back();
    return normalized.empty() ? "Zone" : normalized;
}

std::vector<TransitionNodeRecord> ParseTransitionNodesFromRoutePayload(std::string const& payload)
{
    std::vector<TransitionNodeRecord> nodes;

    SimpleJsonValue const root = SimpleJsonParser(payload).Parse();
    if (!root.IsObject())
        return nodes;

    std::uint16_t const mapId = static_cast<std::uint16_t>(GetJsonNumberOrDefault(root, "map_id"));
    std::uint32_t const zoneId = static_cast<std::uint32_t>(GetJsonNumberOrDefault(root, "zone_id"));
    std::string const zoneName = GetJsonStringOrDefault(root, "zone_name");

    SimpleJsonValue const* pathsValue = TryGetJsonObjectMember(root, "paths");
    if (!pathsValue || !pathsValue->IsArray())
        return nodes;

    for (SimpleJsonValue const& pathValue : pathsValue->arrayValue)
    {
        if (!pathValue.IsObject())
            continue;

        std::int32_t const pathIndex = static_cast<std::int32_t>(GetJsonNumberOrDefault(pathValue, "path_index", -1.0));
        std::string const pathKey = GetJsonStringOrDefault(pathValue, "path_key");
        SimpleJsonValue const* anchorsValue = TryGetJsonObjectMember(pathValue, "anchors");
        if (!anchorsValue || !anchorsValue->IsArray())
            continue;

        for (std::size_t anchorIndex = 0; anchorIndex < anchorsValue->arrayValue.size(); ++anchorIndex)
        {
            SimpleJsonValue const& anchorValue = anchorsValue->arrayValue[anchorIndex];
            if (!anchorValue.IsObject())
                continue;

            SimpleJsonValue const* transitionValue = TryGetJsonObjectMember(anchorValue, "transition_node");
            if (!transitionValue || !transitionValue->IsObject())
                continue;

            std::uint32_t const targetZoneId = static_cast<std::uint32_t>(
                GetJsonNumberOrDefault(*transitionValue, "target_zone_id"));
            if (targetZoneId == 0)
                continue;

            TransitionNodeRecord node;
            node.mapId = mapId;
            node.zoneId = zoneId;
            node.zoneName = zoneName;
            node.pathKey = pathKey;
            node.pathIndex = pathIndex;
            node.anchorIndex = static_cast<std::int32_t>(anchorIndex);
            node.x = static_cast<float>(GetJsonNumberOrDefault(anchorValue, "world_x"));
            node.y = static_cast<float>(GetJsonNumberOrDefault(anchorValue, "world_y"));
            node.z = static_cast<float>(GetJsonNumberOrDefault(anchorValue, "world_z"));
            node.targetZoneId = targetZoneId;
            nodes.push_back(std::move(node));
        }
    }

    return nodes;
}

std::optional<GeneratedConnectorRecord> BuildGeneratedConnectorRecord(
    TransitionNodeRecord const& first,
    TransitionNodeRecord const& second,
    std::uint32_t ordinal)
{
    if (first.mapId != second.mapId)
        return std::nullopt;

    constexpr float MaxTransitionPairDistanceYards = 300.0f;
    constexpr float GlueTargetGapYards = 8.0f;
    constexpr float GlueMaxDistanceYards = 80.0f;

    float const seamDistance = Distance2D(first.x, first.y, second.x, second.y);
    if (seamDistance > MaxTransitionPairDistanceYards)
        return std::nullopt;

    TransitionNodeRecord const* fromNode = &first;
    TransitionNodeRecord const* toNode = &second;
    if (std::tie(first.zoneId, first.pathIndex, first.anchorIndex) > std::tie(second.zoneId, second.pathIndex, second.anchorIndex))
    {
        fromNode = &second;
        toNode = &first;
    }

    float fromX = fromNode->x;
    float fromY = fromNode->y;
    float toX = toNode->x;
    float toY = toNode->y;

    if (seamDistance > GlueTargetGapYards && seamDistance <= GlueMaxDistanceYards)
    {
        float const dx = toX - fromX;
        float const dy = toY - fromY;
        float const length = std::max(0.001f, std::sqrt((dx * dx) + (dy * dy)));
        float const midX = (fromX + toX) * 0.5f;
        float const midY = (fromY + toY) * 0.5f;
        float const halfGap = GlueTargetGapYards * 0.5f;
        float const ux = dx / length;
        float const uy = dy / length;
        fromX = midX - (ux * halfGap);
        fromY = midY - (uy * halfGap);
        toX = midX + (ux * halfGap);
        toY = midY + (uy * halfGap);
    }

    GeneratedConnectorRecord connector;
    connector.mapId = fromNode->mapId;
    connector.fromZoneId = fromNode->zoneId;
    connector.toZoneId = toNode->zoneId;
    connector.fromZoneName = fromNode->zoneName;
    connector.toZoneName = toNode->zoneName;
    connector.fromPathKey = fromNode->pathKey;
    connector.toPathKey = toNode->pathKey;
    connector.fromAnchorIndex = fromNode->anchorIndex;
    connector.toAnchorIndex = toNode->anchorIndex;
    connector.fromX = fromX;
    connector.fromY = fromY;
    connector.fromZ = fromNode->z;
    connector.toX = toX;
    connector.toY = toY;
    connector.toZ = toNode->z;

    std::ostringstream key;
    key << SlugifyTransitionLabel(connector.fromZoneName)
        << "_"
        << SlugifyTransitionLabel(connector.toZoneName)
        << "_Border_"
        << std::setw(2) << std::setfill('0') << ordinal;
    connector.connectorKey = key.str();
    return connector;
}

bool WriteGeneratedConnectorManifest(
    std::filesystem::path const& path,
    std::uint16_t mapId,
    std::vector<GeneratedConnectorRecord> const& connectors)
{
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
        return false;

    output << "{\n";
    output << "  \"map_id\": " << mapId << ",\n";
    output << "  \"z_baked\": false,\n";
    output << "  \"generated_from_transition_nodes\": true,\n";
    output << "  \"connectors\": [\n";
    for (std::size_t index = 0; index < connectors.size(); ++index)
    {
        auto const& connector = connectors[index];
        output << "    {\n";
        output << "      \"connector_key\": \"" << connector.connectorKey << "\",\n";
        output << "      \"from_zone_id\": " << connector.fromZoneId << ",\n";
        output << "      \"to_zone_id\": " << connector.toZoneId << ",\n";
        output << "      \"bidirectional\": true,\n";
        output << "      \"from_path_key\": \"" << connector.fromPathKey << "\",\n";
        output << "      \"from_anchor_index\": " << connector.fromAnchorIndex << ",\n";
        output << "      \"to_path_key\": \"" << connector.toPathKey << "\",\n";
        output << "      \"to_anchor_index\": " << connector.toAnchorIndex << ",\n";
        output << "      \"from\": {\n";
        output << "        \"world_x\": " << std::fixed << std::setprecision(4) << connector.fromX << ",\n";
        output << "        \"world_y\": " << std::fixed << std::setprecision(4) << connector.fromY << ",\n";
        output << "        \"world_z\": " << std::fixed << std::setprecision(4) << connector.fromZ << "\n";
        output << "      },\n";
        output << "      \"to\": {\n";
        output << "        \"world_x\": " << std::fixed << std::setprecision(4) << connector.toX << ",\n";
        output << "        \"world_y\": " << std::fixed << std::setprecision(4) << connector.toY << ",\n";
        output << "        \"world_z\": " << std::fixed << std::setprecision(4) << connector.toZ << "\n";
        output << "      }\n";
        output << "    }";
        if ((index + 1u) < connectors.size())
            output << ",";
        output << "\n";
    }
    output << "  ]\n";
    output << "}\n";
    return true;
}

std::vector<GeneratedConnectorRecord> MergeGeneratedConnectorsWithExisting(
    std::filesystem::path const& manifestPath,
    std::vector<GeneratedConnectorRecord> generated)
{
    std::ifstream input(manifestPath, std::ios::in | std::ios::binary);
    if (!input.is_open())
        return generated;

    std::ostringstream stream;
    stream << input.rdbuf();
    input.close();

    try
    {
        std::vector<GeneratedConnectorRecord> existing = ParseExistingConnectorRecords(stream.str());
        for (GeneratedConnectorRecord& generatedConnector : generated)
        {
            auto existingItr = std::find_if(
                existing.begin(),
                existing.end(),
                [&generatedConnector](GeneratedConnectorRecord const& existingConnector)
                {
                    return existingConnector.connectorKey == generatedConnector.connectorKey;
                });
            if (existingItr != existing.end())
                *existingItr = generatedConnector;
            else
                existing.push_back(std::move(generatedConnector));
        }
        return existing;
    }
    catch (std::exception const&)
    {
        return generated;
    }
}

std::uint32_t GenerateConnectorManifestsFromTransitionNodes(std::filesystem::path const& routeRoot)
{
    std::error_code ec;
    if (!std::filesystem::exists(routeRoot, ec) || !std::filesystem::is_directory(routeRoot, ec))
        return 0;

    std::unordered_map<std::uint16_t, std::vector<TransitionNodeRecord>> nodesByMap;
    for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator(routeRoot, ec))
    {
        if (ec || !entry.is_regular_file())
            continue;

        std::string const filename = entry.path().filename().string();
        if (filename.find("__routes.json") == std::string::npos)
            continue;

        std::ifstream input(entry.path(), std::ios::in | std::ios::binary);
        if (!input.is_open())
            continue;

        std::ostringstream stream;
        stream << input.rdbuf();
        try
        {
            for (TransitionNodeRecord& node : ParseTransitionNodesFromRoutePayload(stream.str()))
                nodesByMap[node.mapId].push_back(std::move(node));
        }
        catch (std::exception const& ex)
        {
            LOG_ERROR("server.worldserver",
                "[LivingWorld] Could not parse transition nodes from {}: {}",
                entry.path().string(),
                ex.what());
        }
    }

    std::uint32_t generatedManifestCount = 0;
    for (auto& [mapId, nodes] : nodesByMap)
    {
        struct PairCandidate
        {
            std::size_t firstIndex = 0;
            std::size_t secondIndex = 0;
            float distance = 0.0f;
        };

        std::vector<PairCandidate> candidates;
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            for (std::size_t j = i + 1u; j < nodes.size(); ++j)
            {
                if (nodes[i].zoneId == nodes[j].zoneId)
                    continue;
                if (nodes[i].targetZoneId != nodes[j].zoneId || nodes[j].targetZoneId != nodes[i].zoneId)
                    continue;

                candidates.push_back(PairCandidate{
                    i,
                    j,
                    Distance2D(nodes[i].x, nodes[i].y, nodes[j].x, nodes[j].y)
                });
            }
        }

        if (candidates.empty())
            continue;

        std::sort(candidates.begin(), candidates.end(), [](PairCandidate const& left, PairCandidate const& right)
        {
            return left.distance < right.distance;
        });

        std::vector<bool> used(nodes.size(), false);
        std::vector<GeneratedConnectorRecord> connectors;
        std::uint32_t ordinal = 1;
        for (PairCandidate const& candidate : candidates)
        {
            if (used[candidate.firstIndex] || used[candidate.secondIndex])
                continue;

            auto connector = BuildGeneratedConnectorRecord(
                nodes[candidate.firstIndex],
                nodes[candidate.secondIndex],
                ordinal);
            if (!connector)
                continue;

            used[candidate.firstIndex] = true;
            used[candidate.secondIndex] = true;
            connectors.push_back(std::move(*connector));
            ++ordinal;
        }

        if (connectors.empty())
            continue;

        std::ostringstream filename;
        filename << "map_" << std::setw(3) << std::setfill('0') << mapId << "__connectors.json";
        std::filesystem::path const manifestPath = routeRoot / filename.str();
        std::vector<GeneratedConnectorRecord> mergedConnectors =
            MergeGeneratedConnectorsWithExisting(manifestPath, std::move(connectors));
        if (!WriteGeneratedConnectorManifest(manifestPath, mapId, mergedConnectors))
        {
            LOG_ERROR("server.worldserver",
                "[LivingWorld] Failed to write generated connector manifest {}.",
                manifestPath.string());
            continue;
        }

        ++generatedManifestCount;
        LOG_INFO("server.worldserver",
            "[LivingWorld] Generated {} connector(s) from transition nodes into {}.",
            mergedConnectors.size(),
            manifestPath.string());
    }

    return generatedManifestCount;
}

void MaybeGenerateConnectorManifestsOnStartup()
{
    std::filesystem::path const routeRoot = ResolveWorldBotRouteExportRoot();
    std::uint32_t const generatedCount = GenerateConnectorManifestsFromTransitionNodes(routeRoot);
    if (generatedCount > 0)
    {
        LOG_INFO("server.worldserver",
            "[LivingWorld] Generated {} connector manifest(s) from transition-node markup under {}.",
            generatedCount,
            routeRoot.string());
    }
}

std::optional<std::uint32_t> TryParseNumericSuffix(
    std::string const& filename,
    std::string const& prefix,
    std::string const& suffix)
{
    if (!filename.starts_with(prefix) || !filename.ends_with(suffix))
        return std::nullopt;

    std::string const numeric = filename.substr(
        prefix.size(),
        filename.size() - prefix.size() - suffix.size());
    if (numeric.empty())
        return std::nullopt;

    try
    {
        return static_cast<std::uint32_t>(std::stoul(numeric));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<std::uint32_t> TryParseMapIdFromRouteFilename(std::string const& filename)
{
    if (!filename.starts_with("map_"))
        return std::nullopt;

    std::size_t const numericStart = 4u;
    std::size_t const numericEnd = filename.find("__", numericStart);
    if (numericEnd == std::string::npos || numericEnd <= numericStart)
        return std::nullopt;

    try
    {
        return static_cast<std::uint32_t>(
            std::stoul(filename.substr(numericStart, numericEnd - numericStart)));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<std::uint32_t> TryParseZoneIdFromRouteFilename(std::string const& filename)
{
    std::size_t const marker = filename.find("__zone_");
    if (marker == std::string::npos)
        return std::nullopt;

    std::size_t const numericStart = marker + 7u;
    std::size_t const numericEnd = filename.find("__", numericStart);
    if (numericEnd == std::string::npos || numericEnd <= numericStart)
        return std::nullopt;

    try
    {
        return static_cast<std::uint32_t>(
            std::stoul(filename.substr(numericStart, numericEnd - numericStart)));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<double> TryExtractJsonNumber(std::string const& line, char const* key)
{
    std::regex const pattern(
        std::string(R"(^\s*")") + key + R"("\s*:\s*(-?\d+(?:\.\d+)?)(?:\s*,\s*)?$)");
    std::smatch match;
    if (!std::regex_match(line, match, pattern))
        return std::nullopt;

    try
    {
        return std::stod(match[1].str());
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::string ReplaceJsonNumber(std::string const& line, char const* key, double value)
{
    std::regex const pattern(
        std::string(R"((^\s*")") + key + R"("\s*:\s*)(-?\d+(?:\.\d+)?)(\s*,?\s*$))");
    std::smatch match;
    if (!std::regex_match(line, match, pattern))
        return line;

    std::ostringstream valueStream;
    valueStream << std::fixed << std::setprecision(4) << value;
    return match[1].str() + valueStream.str() + match[3].str();
}

std::string BuildJsonNumberLine(std::string const& referenceLine, char const* key, double value)
{
    std::size_t const indentLength = referenceLine.find_first_not_of(" \t");
    std::string const indent = indentLength == std::string::npos
        ? std::string()
        : referenceLine.substr(0, indentLength);

    std::ostringstream valueStream;
    valueStream << std::fixed << std::setprecision(4) << value;
    return indent + "\"" + key + "\": " + valueStream.str() + ",";
}

std::string ReplaceJsonBoolean(std::string const& line, char const* key, bool value)
{
    std::regex const pattern(
        std::string(R"((^\s*")") + key + R"("\s*:\s*)(true|false)(\s*,?\s*$))");
    std::smatch match;
    if (!std::regex_match(line, match, pattern))
        return line;

    return match[1].str() + (value ? "true" : "false") + match[3].str();
}

std::string BuildJsonBooleanLine(std::string const& referenceLine, char const* key, bool value)
{
    std::size_t const indentLength = referenceLine.find_first_not_of(" \t");
    std::string const indent = indentLength == std::string::npos
        ? std::string()
        : referenceLine.substr(0, indentLength);

    return indent + "\"" + key + "\": " + (value ? "true" : "false") + ",";
}

std::optional<bool> TryExtractJsonBoolean(std::string const& line, char const* key)
{
    std::regex const pattern(
        std::string(R"(^\s*")") + key + R"("\s*:\s*(true|false)(?:\s*,\s*)?$)");
    std::smatch match;
    if (!std::regex_match(line, match, pattern))
        return std::nullopt;

    return match[1].str() == "true";
}

double ResolveGroundedRouteZ(Map* map, float x, float y, float fallbackZ)
{
    if (!map)
        return fallbackZ;

    float resolved = fallbackZ;
    float hintedGround = INVALID_HEIGHT;
    if (std::fabs(fallbackZ) > 0.01f)
        hintedGround = map->GetHeight(x, y, fallbackZ, true, 50.0f);

    if (hintedGround > INVALID_HEIGHT)
        resolved = hintedGround;
    else
    {
        float const skyGround = map->GetHeight(x, y, MAX_HEIGHT);
        if (skyGround > INVALID_HEIGHT)
            resolved = skyGround;
    }

    if (resolved <= INVALID_HEIGHT)
    {
        float const water = map->GetWaterLevel(x, y);
        if (water > INVALID_HEIGHT)
            resolved = water;
    }

    return resolved;
}

bool BakeRouteFileZHeights(std::filesystem::path const& path, std::uint32_t mapId)
{
    Map* map = sMapMgr->CreateBaseMap(mapId);
    if (!map)
        return false;

    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open())
        return false;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
        lines.push_back(line);
    input.close();

    std::optional<double> pendingX;
    std::optional<double> pendingY;
    bool touched = false;
    std::vector<std::string> rebuiltLines;
    rebuiltLines.reserve(lines.size() + 128u);
    bool sawBakedFlag = false;
    bool alreadyBaked = false;

    for (std::string& currentLine : lines)
    {
        if (auto const baked = TryExtractJsonBoolean(currentLine, "z_baked"))
        {
            sawBakedFlag = true;
            alreadyBaked = *baked;
            rebuiltLines.push_back(currentLine);
            continue;
        }

        if (auto const x = TryExtractJsonNumber(currentLine, "world_x"))
            pendingX = *x;
        if (auto const y = TryExtractJsonNumber(currentLine, "world_y"))
            pendingY = *y;

        if (pendingX && pendingY &&
            currentLine.find("\"distance_from_start_yards\"") != std::string::npos)
        {
            double const bakedZ = ResolveGroundedRouteZ(
                map,
                static_cast<float>(*pendingX),
                static_cast<float>(*pendingY),
                0.0f);
            rebuiltLines.push_back(BuildJsonNumberLine(currentLine, "world_z", bakedZ));
            touched = true;
            pendingX.reset();
            pendingY.reset();
        }

        auto const currentZ = TryExtractJsonNumber(currentLine, "world_z");
        if (!currentZ)
        {
            rebuiltLines.push_back(currentLine);
            continue;
        }

        if (pendingX && pendingY)
        {
            double const bakedZ = ResolveGroundedRouteZ(
                map,
                static_cast<float>(*pendingX),
                static_cast<float>(*pendingY),
                static_cast<float>(*currentZ));
            if (std::fabs(bakedZ - *currentZ) > 0.01)
            {
                currentLine = ReplaceJsonNumber(currentLine, "world_z", bakedZ);
                touched = true;
            }
        }

        pendingX.reset();
        pendingY.reset();
        rebuiltLines.push_back(currentLine);
    }

    if (!touched && alreadyBaked)
        return true;

    if (sawBakedFlag)
    {
        for (std::string& rebuiltLine : rebuiltLines)
        {
            if (TryExtractJsonBoolean(rebuiltLine, "z_baked").has_value())
            {
                rebuiltLine = ReplaceJsonBoolean(rebuiltLine, "z_baked", true);
                break;
            }
        }
    }
    else
    {
        std::size_t insertIndex = 0;
        for (std::size_t index = 0; index < rebuiltLines.size(); ++index)
        {
            if (rebuiltLines[index].find("\"map_id\"") != std::string::npos)
            {
                insertIndex = index + 1u;
                break;
            }
        }

        std::string const referenceLine =
            (insertIndex > 0u && insertIndex <= rebuiltLines.size())
                ? rebuiltLines[insertIndex - 1u]
                : std::string("  ");
        rebuiltLines.insert(
            rebuiltLines.begin() + static_cast<std::ptrdiff_t>(insertIndex),
            BuildJsonBooleanLine(referenceLine, "z_baked", true));
    }

    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
        return false;

    for (std::size_t index = 0; index < rebuiltLines.size(); ++index)
    {
        output << rebuiltLines[index];
        if ((index + 1u) < rebuiltLines.size())
            output << '\n';
    }

    return true;
}

struct RouteBakeScanResult
{
    std::uint32_t scannedFiles = 0;
    std::uint32_t bakedFiles = 0;
    std::uint32_t skippedAlreadyBaked = 0;
};

RouteBakeScanResult BakeRouteBundleZHeights(
    std::filesystem::path const& routeRoot,
    std::unordered_set<std::uint32_t> const* zoneFilter)
{
    RouteBakeScanResult result;

    std::error_code ec;
    if (!std::filesystem::exists(routeRoot, ec) || !std::filesystem::is_directory(routeRoot, ec))
        return result;

    for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator(routeRoot, ec))
    {
        if (ec || !entry.is_regular_file())
            continue;

        std::string const filename = entry.path().filename().string();
        bool const isRouteFile = filename.find("__routes.json") != std::string::npos;
        bool const isConnectorFile = filename.find("__connectors.json") != std::string::npos;
        if (!isRouteFile && !isConnectorFile)
            continue;

        if (zoneFilter && !zoneFilter->empty() && isRouteFile)
        {
            std::optional<std::uint32_t> const zoneId = TryParseZoneIdFromRouteFilename(filename);
            if (!zoneId || !zoneFilter->contains(*zoneId))
                continue;
        }

        std::ifstream input(entry.path(), std::ios::in | std::ios::binary);
        std::string fileText;
        if (input.is_open())
        {
            std::ostringstream stream;
            stream << input.rdbuf();
            fileText = stream.str();
        }
        if (fileText.find("\"z_baked\": true") != std::string::npos)
        {
            ++result.skippedAlreadyBaked;
            continue;
        }

        std::optional<std::uint32_t> const mapId = TryParseMapIdFromRouteFilename(filename);
        if (!mapId)
            continue;

        ++result.scannedFiles;
        if (BakeRouteFileZHeights(entry.path(), *mapId))
            ++result.bakedFiles;
    }

    return result;
}

void MaybeBakeRouteBundleOnStartup()
{
    bool const startupBakeEnabled =
        sConfigMgr->GetOption<bool>("LivingWorld.RouteExportBakeZOnStartup", true);
    bool const debugHarnessBakeEnabled =
        sConfigMgr->GetOption<bool>("LivingWorld.DebugRouteHarnessEnabled", false) &&
        sConfigMgr->GetOption<bool>("LivingWorld.DebugRouteHarnessBakeRouteZ", false);

    if (!startupBakeEnabled && !debugHarnessBakeEnabled)
        return;

    std::filesystem::path const routeRoot = ResolveWorldBotRouteExportRoot();
    std::error_code ec;
    if (!std::filesystem::exists(routeRoot, ec) || !std::filesystem::is_directory(routeRoot, ec))
    {
        LOG_ERROR("server.worldserver",
            "[LivingWorld] Route Z bake could not find route root {}.",
            routeRoot.string());
        return;
    }

    std::unordered_set<std::uint32_t> zoneFilter;
    if (!startupBakeEnabled)
    {
        for (std::uint32_t const zoneId : ParseDebugIdentityIdList(
            sConfigMgr->GetOption<std::string>("LivingWorld.DebugRouteHarnessBakeZoneIds", "")))
        {
            zoneFilter.insert(zoneId);
        }
    }

    RouteBakeScanResult const result = BakeRouteBundleZHeights(
        routeRoot,
        zoneFilter.empty() ? nullptr : &zoneFilter);

    LOG_INFO("server.worldserver",
        "[LivingWorld] Route Z bake scanned {} file(s), baked {}, skipped {} already-marked file(s) from {}.",
        result.scannedFiles,
        result.bakedFiles,
        result.skippedAlreadyBaked,
        routeRoot.string());
}

std::vector<std::uint32_t> ParseDebugIdentityIdList(std::string const& value)
{
    std::vector<std::uint32_t> ids;
    std::unordered_set<std::uint32_t> seen;
    std::istringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        auto const begin = token.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
            continue;

        auto const end = token.find_last_not_of(" \t\r\n");
        std::string const trimmed = token.substr(begin, end - begin + 1);

        try
        {
            std::uint32_t const id = static_cast<std::uint32_t>(std::stoul(trimmed));
            if (id != 0 && seen.insert(id).second)
                ids.push_back(id);
        }
        catch (...)
        {
        }
    }

    return ids;
}

std::string JoinZoneIdCsv(std::vector<std::uint32_t> const& zoneIds)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < zoneIds.size(); ++i)
    {
        if (i != 0)
            oss << ',';
        oss << zoneIds[i];
    }

    return oss.str();
}

std::string QuoteCharactersString(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::string ResolveDefaultSpecKeyForClass(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return "Arms";
        case CLASS_PALADIN:
            return "Retribution";
        case CLASS_HUNTER:
            return "Beast Mastery";
        case CLASS_ROGUE:
            return "Assassination";
        case CLASS_PRIEST:
            return "Shadow";
        case CLASS_DEATH_KNIGHT:
            return "Blood";
        case CLASS_SHAMAN:
            return "Elemental";
        case CLASS_MAGE:
            return "Frost";
        case CLASS_WARLOCK:
            return "Affliction";
        case CLASS_DRUID:
            return "Feral";
        default:
            return "Arms";
    }
}

std::optional<living_world::integration::BotIdentityRecord> BuildDebugRouteHarnessIdentityTemplate(
    std::uint8_t level,
    std::uint8_t raceId,
    std::uint8_t classId,
    std::uint8_t gender)
{
    PlayerInfo const* playerInfo = sObjectMgr->GetPlayerInfo(raceId, classId);
    if (!playerInfo)
        return std::nullopt;

    living_world::integration::BotIdentityRecord identity;
    identity.name = "RouteHarnessL" + std::to_string(level);
    identity.raceId = raceId;
    identity.classId = classId;
    identity.specKey = ResolveDefaultSpecKeyForClass(classId);
    identity.faction = Player::TeamIdForRace(raceId) == TEAM_HORDE ? 2u : 1u;
    identity.displayId = gender == GENDER_FEMALE ? playerInfo->displayId_f : playerInfo->displayId_m;
    identity.gender = gender;
    identity.level = level;
    identity.gearTier = static_cast<std::uint8_t>(std::clamp<std::uint32_t>(std::max<std::uint32_t>(1u, level / 20u), 1u, 5u));
    identity.personalityKey = "uninterested";
    identity.isAvailable = true;
    return identity;
}

std::optional<living_world::integration::BotIdentityRecord> EnsureDebugRouteHarnessIdentity(
    std::uint8_t level,
    std::uint8_t raceId,
    std::uint8_t classId,
    std::uint8_t gender)
{
    auto const identityTemplate = BuildDebugRouteHarnessIdentityTemplate(level, raceId, classId, gender);
    if (!identityTemplate)
        return std::nullopt;

    living_world::integration::SqlBotIdentityRepository repo;
    auto identity = repo.FindByName(identityTemplate->name);
    if (!identity)
    {
        CharacterDatabase.Execute(
            "INSERT INTO living_world_bot_identity "
            "(name, race_id, class_id, spec_key, loadout_key, faction, display_id, gender, level, gear_tier, personality_key, "
            "has_herbalism, has_mining, has_fishing, home_zone_id, home_anchor_point_key, home_bind_point_key, "
            "is_available, session_count, total_world_online_ms, world_online_ms_since_level, post_max_world_online_ms, "
            "active_world_session_ms, active_world_session_start, is_retired, successor_spawned, retired_at, last_seen_zone, last_seen_at) "
            "VALUES ({}, {}, {}, {}, '', {}, {}, {}, {}, {}, {}, 0, 0, 0, NULL, NULL, NULL, 1, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL, NULL)",
            QuoteCharactersString(identityTemplate->name),
            identityTemplate->raceId,
            identityTemplate->classId,
            QuoteCharactersString(identityTemplate->specKey),
            identityTemplate->faction,
            identityTemplate->displayId,
            identityTemplate->gender,
            identityTemplate->level,
            identityTemplate->gearTier,
            QuoteCharactersString(identityTemplate->personalityKey));

        identity = repo.FindByName(identityTemplate->name);
    }
    else
    {
        CharacterDatabase.Execute(
            "UPDATE living_world_bot_identity "
            "SET race_id = {}, class_id = {}, spec_key = {}, loadout_key = '', faction = {}, display_id = {}, gender = {}, "
            "level = {}, gear_tier = {}, personality_key = {}, has_herbalism = 0, has_mining = 0, has_fishing = 0, "
            "is_available = 1, active_world_session_ms = 0, active_world_session_start = NULL, "
            "is_retired = 0, successor_spawned = 0, retired_at = NULL "
            "WHERE id = {}",
            identityTemplate->raceId,
            identityTemplate->classId,
            QuoteCharactersString(identityTemplate->specKey),
            identityTemplate->faction,
            identityTemplate->displayId,
            identityTemplate->gender,
            identityTemplate->level,
            identityTemplate->gearTier,
            QuoteCharactersString(identityTemplate->personalityKey),
            identity->id);

        identity = repo.FindById(identity->id);
    }

    return identity;
}

living_world::service::AmbientSession BuildDebugRouteHarnessSession(
    std::uint32_t mapId,
    std::uint32_t destZoneId,
    float destX,
    float destY,
    float destZ,
    std::uint32_t idleDurationSec,
    std::string const& transitRouteKey = "")
{
    living_world::service::AmbientSession session;

    if (!transitRouteKey.empty())
    {
        living_world::integration::SqlTaskPointRepository pointRepo;
        auto const transitRoute = pointRepo.FindTransitRouteByKey(transitRouteKey);
        if (transitRoute)
        {
            living_world::service::AmbientSessionTask embarkTask;
            embarkTask.activityId = 0;
            embarkTask.activityKey = "debug_route_harness_embark";
            embarkTask.displayName = "Debug Transit Embark";
            embarkTask.activityType = "travel_debug";
            embarkTask.taskFamily = "debug";
            embarkTask.targetZoneId = transitRoute->sourceZoneId;
            session.tasks.push_back(std::move(embarkTask));

            living_world::service::AmbientStep travelStep;
            travelStep.type = living_world::service::AmbientStepType::Travel;
            travelStep.mapId = transitRoute->sourceMapId;
            travelStep.x = transitRoute->sourceX;
            travelStep.y = transitRoute->sourceY;
            travelStep.z = transitRoute->sourceZ;
            travelStep.durationSec = 0;
            travelStep.taskIndex = 0;
            travelStep.label = "Travel to " + transitRoute->sourcePointName;
            session.steps.push_back(std::move(travelStep));

            living_world::service::AmbientSessionTask arriveTask;
            arriveTask.activityId = 0;
            arriveTask.activityKey = "debug_route_harness_arrive";
            arriveTask.displayName = "Debug Transit Arrival";
            arriveTask.activityType = "travel_debug";
            arriveTask.taskFamily = "debug";
            arriveTask.targetZoneId = transitRoute->destZoneId;
            session.tasks.push_back(std::move(arriveTask));

            living_world::service::AmbientStep transitStep;
            transitStep.type = living_world::service::AmbientStepType::Transit;
            transitStep.mapId = transitRoute->destMapId;
            transitStep.x = transitRoute->destX;
            transitStep.y = transitRoute->destY;
            transitStep.z = transitRoute->destZ;
            transitStep.durationSec = std::max<std::uint32_t>(15u, transitRoute->durationSec);
            transitStep.taskIndex = 1;
            transitStep.transitType = transitRoute->transitType;
            transitStep.transitRouteKey = transitRoute->routeKey;
            transitStep.transitSourcePointKey = transitRoute->sourcePointKey;
            transitStep.transitDestPointKey = transitRoute->destPointKey;
            transitStep.transitSourceLabel = transitRoute->sourcePointName;
            transitStep.transitDestLabel = transitRoute->destPointName;
            transitStep.label = transitRoute->displayName.empty()
                ? ("Debug transit " + transitRoute->sourcePointName + " -> " + transitRoute->destPointName)
                : transitRoute->displayName;
            session.steps.push_back(std::move(transitStep));

            living_world::service::AmbientStep idleStep;
            idleStep.type = living_world::service::AmbientStepType::Idle;
            idleStep.mapId = transitRoute->destMapId;
            idleStep.x = transitRoute->destX;
            idleStep.y = transitRoute->destY;
            idleStep.z = transitRoute->destZ;
            idleStep.durationSec = std::max<std::uint32_t>(idleDurationSec, 5u);
            idleStep.taskIndex = 1;
            idleStep.label = "Debug transit arrival hold";
            session.steps.push_back(std::move(idleStep));

            session.activityId = 0;
            session.activityKey = "debug_route_harness";
            session.displayName = "Debug Route Harness";
            session.sourceKind = "debug_route_harness";
            session.sourceKey = "debug_route_harness";
            return session;
        }
    }

    living_world::service::AmbientSessionTask task;
    task.activityId = 0;
    task.activityKey = "debug_route_harness";
    task.displayName = "Debug Route Harness";
    task.activityType = "travel_debug";
    task.taskFamily = "debug";
    task.targetZoneId = destZoneId;
    session.tasks.push_back(std::move(task));

    living_world::service::AmbientStep travelStep;
    travelStep.type = living_world::service::AmbientStepType::Travel;
    travelStep.mapId = mapId;
    travelStep.x = destX;
    travelStep.y = destY;
    travelStep.z = destZ;
    travelStep.durationSec = 0;
    travelStep.taskIndex = 0;
    travelStep.label = "Debug travel route";
    session.steps.push_back(std::move(travelStep));

    living_world::service::AmbientStep idleStep;
    idleStep.type = living_world::service::AmbientStepType::Idle;
    idleStep.mapId = mapId;
    idleStep.x = destX;
    idleStep.y = destY;
    idleStep.z = destZ;
    idleStep.durationSec = std::max<std::uint32_t>(idleDurationSec, 5u);
    idleStep.taskIndex = 0;
    idleStep.label = "Debug route arrival hold";
    session.steps.push_back(std::move(idleStep));

    session.activityId = 0;
    session.activityKey = "debug_route_harness";
    session.displayName = "Debug Route Harness";
    session.sourceKind = "debug_route_harness";
    session.sourceKey = "debug_route_harness";
    return session;
}

bool SessionStartsInZone(
    living_world::service::AmbientSession const& session,
    std::uint32_t zoneId)
{
    return zoneId == 0
        || (!session.tasks.empty() && session.tasks.front().targetZoneId == zoneId);
}

living_world::service::AmbientStepType ActivityTypeToSandboxStepType(std::string const& activityType)
{
    if (activityType == "gather_herb")
        return living_world::service::AmbientStepType::GatherHerb;
    if (activityType == "gather_ore")
        return living_world::service::AmbientStepType::GatherOre;
    if (activityType == "fish")
        return living_world::service::AmbientStepType::Fish;
    if (activityType == "patrol")
        return living_world::service::AmbientStepType::Patrol;
    return living_world::service::AmbientStepType::Idle;
}

std::optional<living_world::service::AmbientSession> BuildForcedZoneSandboxSession(
    living_world::integration::SqlActivityLibraryRepository& activityRepo,
    living_world::integration::SqlZoneIndexRepository& zoneRepo,
    living_world::integration::BotIdentityRecord const& identity,
    std::uint32_t targetZoneId)
{
    if (targetZoneId == 0)
        return std::nullopt;

    auto const zone = zoneRepo.Find(targetZoneId);
    if (!zone)
        return std::nullopt;

    std::vector<living_world::model::ActivityEntry> eligible = activityRepo.LoadEligible(
        identity.faction,
        identity.level,
        identity.hasHerbalism,
        identity.hasMining,
        identity.hasFishing);

    eligible.erase(
        std::remove_if(
            eligible.begin(),
            eligible.end(),
            [&](living_world::model::ActivityEntry const& entry)
            {
                return entry.targetZoneId != targetZoneId;
            }),
        eligible.end());

    if (eligible.empty())
        return std::nullopt;

    auto const picked = std::min_element(
        eligible.begin(),
        eligible.end(),
        [](living_world::model::ActivityEntry const& left,
           living_world::model::ActivityEntry const& right)
        {
            auto score = [](living_world::model::ActivityEntry const& entry)
            {
                if (entry.activityType == "patrol")
                    return 0;
                if (entry.activityType == "idle_city" || entry.activityType == "idle_inn")
                    return 1;
                if (entry.activityType == "gather_ore")
                    return 2;
                if (entry.activityType == "gather_herb")
                    return 3;
                if (entry.activityType == "fish")
                    return 4;
                return 5;
            };

            int const leftScore = score(left);
            int const rightScore = score(right);
            if (leftScore != rightScore)
                return leftScore < rightScore;
            if (left.weight != right.weight)
                return left.weight > right.weight;
            return left.activityId < right.activityId;
        });

    if (picked == eligible.end())
        return std::nullopt;

    living_world::service::AmbientSession session;
    living_world::service::AmbientSessionTask task;
    task.activityId = picked->activityId;
    task.activityKey = picked->activityKey;
    task.displayName = picked->displayName;
    task.activityType = picked->activityType;
    task.taskFamily = picked->taskFamily;
    task.targetZoneId = picked->targetZoneId;
    session.tasks.push_back(std::move(task));

    living_world::service::AmbientStep travelStep;
    travelStep.type = living_world::service::AmbientStepType::Travel;
    travelStep.mapId = zone->mapId;
    travelStep.x = zone->anchorX;
    travelStep.y = zone->anchorY;
    travelStep.z = zone->anchorZ;
    travelStep.durationSec = 0;
    travelStep.taskIndex = 0;
    travelStep.label = "Travel to " + zone->zoneName;
    session.steps.push_back(std::move(travelStep));

    living_world::service::AmbientStep activityStep;
    activityStep.type = ActivityTypeToSandboxStepType(picked->activityType);
    activityStep.mapId = zone->mapId;
    activityStep.x = zone->anchorX;
    activityStep.y = zone->anchorY;
    activityStep.z = zone->anchorZ;
    activityStep.durationSec = std::max<std::uint32_t>(picked->durationMinSec, 600u);
    activityStep.taskIndex = 0;
    activityStep.label = picked->displayName;
    session.steps.push_back(std::move(activityStep));

    session.activityId = picked->activityId;
    session.activityKey = picked->activityKey;
    session.displayName = picked->displayName;
    session.sourceKind = "debug_zone_forced";
    session.sourceKey = "zone_" + std::to_string(targetZoneId) + ":activity_" + picked->activityKey;
    return session;
}
} // namespace

class LivingWorldWorldScript final : public WorldScript
{
public:
    LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        float const scale = sConfigMgr->GetOption<float>("LivingWorld.EconomyScale", 1.0f);
        living_world::ApplyEconomyScale(scale, reload);

        _targetAmbientPop = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientPopulation", 3);
        _populationTickMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientPopulationTickMs", 15 * 1000);

        _forcedSpawnCount = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientForceSpawnCount", 0);
        _forcedSpawnPoint.mapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.AmbientForceSpawnMapId", 0);
        _forcedSpawnPoint.x = sConfigMgr->GetOption<float>(
            "LivingWorld.AmbientForceSpawnX", 0.0f);
        _forcedSpawnPoint.y = sConfigMgr->GetOption<float>(
            "LivingWorld.AmbientForceSpawnY", 0.0f);
        _forcedSpawnPoint.z = sConfigMgr->GetOption<float>(
            "LivingWorld.AmbientForceSpawnZ", 0.0f);

        _debugSyntheticInterestEnabled = sConfigMgr->GetOption<bool>(
            "LivingWorld.DebugSyntheticInterestEnabled", false);
        _debugSyntheticInterestMapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestMapId", 0);
        _debugSyntheticInterestZoneId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestZoneId", 0);
        _debugSyntheticInterestSwitchMapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestSwitchMapId", 0);
        _debugSyntheticInterestSwitchZoneId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestSwitchZoneId", 0);
        _debugSyntheticInterestSwitchMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestSwitchMs", 0);
        _debugSyntheticInterestClearMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugSyntheticInterestClearMs", 0);
        _debugSyntheticInterestElapsedMs = 0;
        _debugSyntheticInterestSwitched = false;
        _debugSyntheticInterestCleared = false;
        _debugForcedIdentityIds = ParseDebugIdentityIdList(
            sConfigMgr->GetOption<std::string>("LivingWorld.DebugForceIdentityIds", ""));
        _debugForcedSessionZoneId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugForceSessionZoneId", 0);
        _debugForcedSessionComposeAttempts = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugForceSessionComposeAttempts", 24);
        _debugRouteHarnessEnabled = sConfigMgr->GetOption<bool>(
            "LivingWorld.DebugRouteHarnessEnabled", false);
        _debugRouteHarnessSpawned = false;
        _debugRouteHarnessLevels = ParseDebugIdentityIdList(
            sConfigMgr->GetOption<std::string>("LivingWorld.DebugRouteHarnessLevels", "10,60"));
        _debugRouteHarnessRaceId = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugRouteHarnessRaceId", RACE_HUMAN));
        _debugRouteHarnessClassId = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugRouteHarnessClassId", CLASS_WARRIOR));
        _debugRouteHarnessGender = static_cast<std::uint8_t>(sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugRouteHarnessGender", GENDER_MALE));
        _debugRouteHarnessMapId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugRouteHarnessMapId", 0);
        _debugRouteHarnessStartX = sConfigMgr->GetOption<float>(
            "LivingWorld.DebugRouteHarnessStartX", -8833.0f);
        _debugRouteHarnessStartY = sConfigMgr->GetOption<float>(
            "LivingWorld.DebugRouteHarnessStartY", 628.0f);
        _debugRouteHarnessStartZ = sConfigMgr->GetOption<float>(
            "LivingWorld.DebugRouteHarnessStartZ", 95.0f);
        _debugRouteHarnessDestZoneId = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugRouteHarnessDestZoneId", 40);
        _debugRouteHarnessDestX = sConfigMgr->GetOption<float>(
            "LivingWorld.DebugRouteHarnessDestX", -10053.198f);
        _debugRouteHarnessDestY = sConfigMgr->GetOption<float>(
            "LivingWorld.DebugRouteHarnessDestY", 1455.3373f);
        _debugRouteHarnessDestZ = sConfigMgr->GetOption<float>(
            "LivingWorld.DebugRouteHarnessDestZ", 0.0f);
        _debugRouteHarnessTransitRouteKey = sConfigMgr->GetOption<std::string>(
            "LivingWorld.DebugRouteHarnessTransitRouteKey", "");
        _debugRouteHarnessExploredZones = ParseDebugIdentityIdList(
            sConfigMgr->GetOption<std::string>("LivingWorld.DebugRouteHarnessExploredZones", ""));
        _debugRouteHarnessIdleDurationSec = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugRouteHarnessIdleDurationSec", 30);

        std::uint32_t const debugHotZoneCooldownMs = sConfigMgr->GetOption<std::uint32_t>(
            "LivingWorld.DebugHotZoneCooldownMs", 0);
        living_world::script::SetWorldBotHotZoneCooldownOverrideMs(debugHotZoneCooldownMs);

        if (_debugSyntheticInterestEnabled)
        {
            living_world::script::SetSyntheticWorldBotInterest(
                _debugSyntheticInterestMapId,
                _debugSyntheticInterestZoneId);
            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] SyntheticInterest enabled map={} zone={} switch_ms={} switch_map={} switch_zone={} hot_cooldown_override_ms={}",
                _debugSyntheticInterestMapId,
                _debugSyntheticInterestZoneId,
                _debugSyntheticInterestSwitchMs,
                _debugSyntheticInterestSwitchMapId,
                _debugSyntheticInterestSwitchZoneId,
                _debugSyntheticInterestClearMs,
                debugHotZoneCooldownMs);
        }
        else
        {
            living_world::script::ClearSyntheticWorldBotInterest();
        }

        if (!_debugForcedIdentityIds.empty() || _debugForcedSessionZoneId != 0)
        {
            std::ostringstream oss;
            for (std::size_t i = 0; i < _debugForcedIdentityIds.size(); ++i)
            {
                if (i != 0)
                    oss << ',';
                oss << _debugForcedIdentityIds[i];
            }

            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] ForcedSandbox identities='{}' forced_session_zone={} compose_attempts={}",
                oss.str(),
                _debugForcedSessionZoneId,
                _debugForcedSessionComposeAttempts);
        }

        if (_debugRouteHarnessEnabled)
        {
            std::ostringstream oss;
            for (std::size_t i = 0; i < _debugRouteHarnessLevels.size(); ++i)
            {
                if (i != 0)
                    oss << ',';
                oss << _debugRouteHarnessLevels[i];
            }

            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] RouteHarness enabled levels='{}' race={} class={} gender={} map={} start=({:.1f},{:.1f},{:.1f}) dest_zone={} dest=({:.1f},{:.1f},{:.1f}) explored_zones='{}' idle_sec={}",
                oss.str(),
                _debugRouteHarnessRaceId,
                _debugRouteHarnessClassId,
                _debugRouteHarnessGender,
                _debugRouteHarnessMapId,
                _debugRouteHarnessStartX,
                _debugRouteHarnessStartY,
                _debugRouteHarnessStartZ,
                _debugRouteHarnessDestZoneId,
                _debugRouteHarnessDestX,
                _debugRouteHarnessDestY,
                _debugRouteHarnessDestZ,
                JoinZoneIdCsv(_debugRouteHarnessExploredZones),
                _debugRouteHarnessIdleDurationSec);
        }

        // Force the first population check on the first world update after
        // startup/reload instead of waiting a full tick interval. This makes
        // autonomous world-bot activity visible immediately while preserving
        // the configured steady-state cadence for subsequent checks.
        _populationTimer = _populationTickMs;
    }

    void OnStartup() override
    {
        living_world::service::BotQuestRewardService().EnsureSchema();
        living_world::integration::SqlBotHazardConfigRepository().EnsureSchema();
        living_world::integration::SqlBotGlobalConfigRepository().EnsureSchema();
        living_world::integration::SqlBotOocConfigRepository().EnsureSchema();
        living_world::integration::SqlBotTalentPreferenceRepository().EnsureSchema();
        living_world::integration::SqlBotAssignedGearRepository().EnsureSchema();
        living_world::integration::SqlBotExploredZoneRepository().EnsureSchema();

        living_world::integration::SqlBotIdentityRepository identityRepo;
        identityRepo.EnsureSchema();
        std::uint32_t const recovered = identityRepo.RecoverStaleActiveSessions();
        if (recovered > 0)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] Recovered {} stale active world-bot identities on startup.",
                recovered);
        }

        MaybeBakeRouteBundleOnStartup();
    }

    void OnUpdate(std::uint32_t diff) override
    {
        TickSyntheticInterestBeacon(diff);
        MaybeSpawnDebugRouteHarness();

        _abstractTickAccum += diff;
        if (_abstractTickAccum >= 1000)
        {
            living_world::script::PruneWorldBotHotZones();
            DematerializeInactiveWorldBots();
            TickAbstractWorldBots(_abstractTickAccum);
            _abstractTickAccum = 0;
        }

        _populationTimer += diff;
        if (_populationTimer < _populationTickMs)
            return;
        _populationTimer = 0;

        if (_targetAmbientPop == 0)
            return;

        TickAmbientPopulation();
    }

private:
    // Spawn position chosen for a world bot materialization.
    struct SpawnPoint { std::uint32_t mapId; float x, y, z; };

    struct AbstractWorldBotRuntime
    {
        living_world::integration::BotIdentityRecord identity;
        living_world::service::AmbientSession session;
        living_world::ai::AbstractWorldBotProgressState progress;
        living_world::ai::AbstractWorldBotTravelPhaseKind lastTravelPhase =
            living_world::ai::AbstractWorldBotTravelPhaseKind::None;
        std::unordered_set<std::uint32_t> exploredZoneIds;
        std::uint64_t worldOnlineMs = 0;
        std::uint64_t lastRuntimeLedgerSyncMs = 0;
        std::string lastRuntimeState;
        std::string lastRuntimeDetail;
        std::uint32_t physicalTransitTransportEntry = 0;
        float physicalTransitLocalX = 0.0f;
        float physicalTransitLocalY = 0.0f;
        float physicalTransitLocalZ = 0.0f;
        float physicalTransitLocalO = 0.0f;
    };

    struct MaterializedWorldBotHandle
    {
        std::uint32_t mapId = 0;
        ObjectGuid guid;
    };

    struct CityReservePolicy
    {
        std::uint32_t zoneId = 0;
        std::uint32_t mapId = 0;
        std::uint8_t  faction = 0;
        std::uint32_t targetVisible = 0;
    };

    std::uint32_t _populationTimer   = 0;
    std::uint32_t _targetAmbientPop  = 3;
    std::uint32_t _populationTickMs  = 15 * 1000;
    std::uint32_t _forcedSpawnCount  = 0;
    std::uint32_t _abstractTickAccum = 0;
    bool _debugSyntheticInterestEnabled = false;
    bool _debugSyntheticInterestSwitched = false;
    bool _debugSyntheticInterestCleared = false;
    std::uint32_t _debugSyntheticInterestMapId = 0;
    std::uint32_t _debugSyntheticInterestZoneId = 0;
    std::uint32_t _debugSyntheticInterestSwitchMapId = 0;
    std::uint32_t _debugSyntheticInterestSwitchZoneId = 0;
    std::uint32_t _debugSyntheticInterestSwitchMs = 0;
    std::uint32_t _debugSyntheticInterestClearMs = 0;
    std::uint32_t _debugSyntheticInterestElapsedMs = 0;
    std::vector<std::uint32_t> _debugForcedIdentityIds;
    std::uint32_t _debugForcedSessionZoneId = 0;
    std::uint32_t _debugForcedSessionComposeAttempts = 24;
    bool _debugRouteHarnessEnabled = false;
    bool _debugRouteHarnessSpawned = false;
    std::vector<std::uint32_t> _debugRouteHarnessLevels;
    std::uint8_t _debugRouteHarnessRaceId = RACE_HUMAN;
    std::uint8_t _debugRouteHarnessClassId = CLASS_WARRIOR;
    std::uint8_t _debugRouteHarnessGender = GENDER_MALE;
    std::uint32_t _debugRouteHarnessMapId = 0;
    float _debugRouteHarnessStartX = 0.0f;
    float _debugRouteHarnessStartY = 0.0f;
    float _debugRouteHarnessStartZ = 0.0f;
    std::uint32_t _debugRouteHarnessDestZoneId = 0;
    float _debugRouteHarnessDestX = 0.0f;
    float _debugRouteHarnessDestY = 0.0f;
    float _debugRouteHarnessDestZ = 0.0f;
    std::string _debugRouteHarnessTransitRouteKey;
    std::vector<std::uint32_t> _debugRouteHarnessExploredZones;
    std::uint32_t _debugRouteHarnessIdleDurationSec = 30;
    SpawnPoint _forcedSpawnPoint { 0u, 0.0f, 0.0f, 0.0f };
    std::unordered_map<std::uint32_t, AbstractWorldBotRuntime> _abstractWorldBots;
    std::unordered_map<std::uint32_t, MaterializedWorldBotHandle> _materializedWorldBots;

    // Entry ID of the generic world bot creature_template.
    // Defined in data/sql/world/living_world_world_bot_template.sql.
    static constexpr std::uint32_t WorldBotEntry = 9900001;

    static std::optional<SpawnPoint> ToSpawnPoint(
        living_world::model::ZoneEntry const& zone)
    {
        return SpawnPoint{ zone.mapId, zone.anchorX, zone.anchorY, zone.anchorZ };
    }

    bool HasForcedSpawnOverride() const
    {
        return living_world::script::HasForcedSpawnOverride({
            _forcedSpawnCount,
            _forcedSpawnPoint.mapId
        });
    }

    static std::uint32_t GetHubZoneIdForIdentity(
        living_world::integration::BotIdentityRecord const& identity)
    {
        if (identity.populationRole == "city_reserve"
            && identity.reserveCityZoneId != 0)
        {
            return identity.reserveCityZoneId;
        }

        if (identity.homeZoneId != 0)
            return identity.homeZoneId;

        if (identity.level < 60)
            return identity.faction == 2 ? 1637u : 1519u;

        if (identity.level < 70)
            return 3703u;

        return 4395u;
    }

    static std::optional<SpawnPoint> ResolveSpawnPoint(
        living_world::integration::BotIdentityRecord const& identity,
        living_world::service::AmbientSession const& session)
    {
        living_world::integration::SqlZoneIndexRepository zoneRepo;

        std::optional<std::uint16_t> requiredMapId;
        if (!session.steps.empty())
            requiredMapId = session.steps.front().mapId;

        auto matchesRequiredMap =
            [&](living_world::model::ZoneEntry const& zone) -> bool
            {
                return !requiredMapId || zone.mapId == *requiredMapId;
            };

        if (identity.populationRole == "city_reserve"
            && identity.reserveCityZoneId != 0)
        {
            if (auto const zone = zoneRepo.Find(identity.reserveCityZoneId))
            {
                if (matchesRequiredMap(*zone))
                    return ToSpawnPoint(*zone);
            }
        }

        if (identity.lastSeenZoneId != 0)
        {
            if (auto const zone = zoneRepo.Find(identity.lastSeenZoneId))
            {
                if (matchesRequiredMap(*zone))
                    return ToSpawnPoint(*zone);
            }
        }

        if (auto const hubZone = zoneRepo.Find(GetHubZoneIdForIdentity(identity)))
        {
            if (matchesRequiredMap(*hubZone))
                return ToSpawnPoint(*hubZone);
        }

        if (!session.steps.empty())
        {
            living_world::service::AmbientStep const& first = session.steps.front();
            return SpawnPoint{ first.mapId, first.x, first.y, first.z };
        }

        return std::nullopt;
    }

    // Count currently active creature world bots in identity ledger.
    static std::uint32_t CountOnlineWorldBots()
    {
        QueryResult qr = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM living_world_bot_identity "
            "WHERE is_available = 0 AND is_retired = 0");
        if (!qr)
            return 0;
        return qr->Fetch()[0].Get<std::uint32_t>();
    }

    static std::vector<CityReservePolicy> GetCityReservePolicies()
    {
        return {
            CityReservePolicy{ 1519u, 0u, 1u, 50u },
            CityReservePolicy{ 1637u, 1u, 2u, 50u }
        };
    }

    std::uint32_t CountMaterializedWorldBotsInZone(std::uint32_t zoneId) const
    {
        if (zoneId == 0)
            return 0;

        std::uint32_t count = 0;
        for (auto const& [identityId, handle] : _materializedWorldBots)
        {
            (void)identityId;
            Map* map = sMapMgr->FindMap(handle.mapId, 0);
            if (!map)
                continue;

            Creature* creature = map->GetCreature(handle.guid);
            if (!creature || !creature->IsInWorld())
                continue;

            if (creature->GetZoneId() == zoneId)
                ++count;
        }

        return count;
    }

    std::vector<living_world::integration::BotIdentityRecord> LoadReserveCityCandidates(
        living_world::integration::SqlBotIdentityRepository& identityRepo,
        std::uint32_t limit) const
    {
        std::vector<living_world::integration::BotIdentityRecord> results;
        if (limit == 0)
            return results;

        for (CityReservePolicy const& policy : GetCityReservePolicies())
        {
            if (!IsZoneHotOrInterested(policy.mapId, policy.zoneId))
                continue;

            std::uint32_t const visible = CountMaterializedWorldBotsInZone(policy.zoneId);
            if (visible >= policy.targetVisible)
                continue;

            std::uint32_t const remaining = limit - static_cast<std::uint32_t>(results.size());
            if (remaining == 0)
                break;

            std::uint32_t const deficit = policy.targetVisible - visible;
            auto cityReserve = identityRepo.LoadAvailableReserveForCity(
                policy.zoneId,
                policy.faction,
                std::min(remaining, deficit));
            for (auto& record : cityReserve)
            {
                results.push_back(std::move(record));
                if (results.size() >= limit)
                    return results;
            }
        }

        return results;
    }

    static void LogActiveWorldBotRoster()
    {
        QueryResult qr = CharacterDatabase.Query(
            "SELECT i.id, i.name, a.event_type, a.detail, a.map_id, a.zone_id, a.pos_x, a.pos_y, a.pos_z "
            "FROM living_world_bot_identity i "
            "LEFT JOIN living_world_bot_activity_log a ON a.id = ("
            "  SELECT MAX(id) FROM living_world_bot_activity_log WHERE bot_guid = i.id) "
            "WHERE i.is_available = 0 AND i.is_retired = 0 "
            "ORDER BY i.id ASC");
        if (!qr)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] ActiveWorldBots: none");
            return;
        }

        do
        {
            Field const* f = qr->Fetch();
            std::uint64_t const identityId = f[0].Get<std::uint64_t>();
            std::string const name = f[1].Get<std::string>();
            std::string const eventType = f[2].IsNull() ? "unknown" : f[2].Get<std::string>();
            std::string const detail = f[3].IsNull() ? "" : f[3].Get<std::string>();
            std::uint32_t const mapId = f[4].IsNull() ? 0u : f[4].Get<std::uint32_t>();
            std::uint32_t const zoneId = f[5].IsNull() ? 0u : f[5].Get<std::uint32_t>();
            float const x = f[6].IsNull() ? 0.f : f[6].Get<float>();
            float const y = f[7].IsNull() ? 0.f : f[7].Get<float>();
            float const z = f[8].IsNull() ? 0.f : f[8].Get<float>();

            LOG_INFO("server.worldserver",
                "[LivingWorld] ActiveWorldBot: name='{}' identity={} event='{}' detail='{}' map={} zone={} pos=({:.1f},{:.1f},{:.1f})",
                name, identityId, eventType, detail, mapId, zoneId, x, y, z);
        } while (qr->NextRow());
    }

    static std::uint32_t ResolveStepZoneId(
        living_world::service::AmbientSession const& session,
        std::size_t stepIndex)
    {
        if (stepIndex >= session.steps.size())
            return 0;

        living_world::service::AmbientStep const& step = session.steps[stepIndex];
        if (step.taskIndex < 0)
            return 0;

        std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
        if (taskIndex >= session.tasks.size())
            return 0;

        return session.tasks[taskIndex].targetZoneId;
    }

    static living_world::ai::AbstractWorldBotProgressConfig BuildAbstractProgressConfig(
        living_world::service::AmbientSession const& session,
        living_world::integration::BotIdentityRecord const& identity)
    {
        living_world::ai::AbstractWorldBotProgressConfig config;
        living_world::service::WorldBotTravelCapabilityConfig const capabilityConfig =
            living_world::service::LoadWorldBotTravelCapabilityConfig();
        living_world::service::WorldBotTravelCapabilityPolicy const capabilityPolicy =
            living_world::service::LoadWorldBotTravelCapabilityPolicy();
        std::unordered_set<std::uint32_t> exploredZones;
        for (std::uint32_t const zoneId :
            living_world::integration::SqlBotExploredZoneRepository().LoadExploredZones(identity.id))
        {
            exploredZones.insert(zoneId);
        }
        std::uint8_t const botLevel = static_cast<std::uint8_t>(identity.level);
        config.travelYardsPerSecond = capabilityConfig.footYardsPerSecond;
        config.routePlanResolver = [&session, botLevel, capabilityConfig, capabilityPolicy](
            living_world::service::AmbientStep const& step,
            std::uint16_t startMapId,
            float startX,
            float startY,
            float startZ) -> std::optional<living_world::service::WorldBotResolvedTravelPlan>
        {
            if (step.type != living_world::service::AmbientStepType::Travel)
                return std::nullopt;
            if (startMapId != step.mapId)
                return std::nullopt;

            std::uint32_t zoneId = 0;
            if (step.taskIndex >= 0)
            {
                std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
                if (taskIndex < session.tasks.size())
                    zoneId = session.tasks[taskIndex].targetZoneId;
            }

            if (zoneId == 0)
                return std::nullopt;

            return GetWorldBotRoutePlanner().ResolveTravelPlan(
                step.mapId,
                0,
                zoneId,
                startX,
                startY,
                startZ,
                step.x,
                step.y,
                step.z,
                living_world::service::ResolveWorldBotTravelCapabilityTierForLevel(
                    botLevel,
                    false,
                    capabilityPolicy),
                capabilityConfig);
        };
        config.travelOptionResolver =
            [&session, identity, exploredZones, botLevel, capabilityConfig, capabilityPolicy](
                living_world::service::AmbientStep const& step,
                std::uint16_t startMapId,
                float startX,
                float startY,
                float startZ) -> std::optional<living_world::service::WorldBotResolvedTravelOption>
        {
            if (step.type != living_world::service::AmbientStepType::Travel)
                return std::nullopt;
            if (startMapId != step.mapId)
                return std::nullopt;

            std::uint32_t zoneId = 0;
            if (step.taskIndex >= 0)
            {
                std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
                if (taskIndex < session.tasks.size())
                    zoneId = session.tasks[taskIndex].targetZoneId;
            }

            if (zoneId == 0)
                return std::nullopt;

            auto const groundResolver =
                [](std::uint16_t mapId,
                   std::uint32_t startZoneIdHint,
                   std::uint32_t destZoneId,
                   float legStartX,
                   float legStartY,
                   float legStartZ,
                   float legDestX,
                   float legDestY,
                   float legDestZ,
                   living_world::service::WorldBotTravelCapabilityTier tier,
                   living_world::service::WorldBotTravelCapabilityConfig const& config)
                    -> std::optional<living_world::service::WorldBotResolvedTravelPlan>
            {
                return GetWorldBotRoutePlanner().ResolveTravelPlan(
                    mapId,
                    startZoneIdHint,
                    destZoneId,
                    legStartX,
                    legStartY,
                    legStartZ,
                    legDestX,
                    legDestY,
                    legDestZ,
                    tier,
                    config);
            };

            return living_world::service::ResolveBestTravelOption(
                GetWorldBotTaxiNetwork(),
                groundResolver,
                step.mapId,
                0,
                zoneId,
                startX,
                startY,
                startZ,
                step.x,
                step.y,
                step.z,
                exploredZones,
                identity.faction,
                living_world::service::ResolveWorldBotTravelCapabilityTierForLevel(
                    botLevel,
                    false,
                    capabilityPolicy),
                capabilityConfig);
        };
        return config;
    }

    static std::unordered_set<std::uint32_t> LoadExploredZoneSet(std::uint32_t identityId)
    {
        std::unordered_set<std::uint32_t> exploredZones;
        for (std::uint32_t const zoneId :
            living_world::integration::SqlBotExploredZoneRepository().LoadExploredZones(identityId))
        {
            exploredZones.insert(zoneId);
        }

        return exploredZones;
    }

    static std::uint32_t ResolveZoneIdAtPosition(
        std::uint16_t mapId,
        float x,
        float y,
        float z)
    {
        return sMapMgr->GetZoneId(PHASEMASK_NORMAL, mapId, x, y, z);
    }

    static bool HasInterestedPlayerForMapAndZone(std::uint32_t mapId, std::uint32_t zoneId)
    {
        if (living_world::script::HasSyntheticWorldBotInterest(mapId, zoneId))
            return true;

        std::shared_lock<std::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
        {
            (void)guid;
            if (!player || !player->IsInWorld() || !player->GetSession() || player->GetSession()->IsBotSession())
                continue;

            if (player->IsInFlight())
                continue;

            if (player->GetMapId() != mapId)
                continue;

            if (zoneId != 0 && player->GetZoneId() != zoneId)
                continue;

            return true;
        }

        return false;
    }

    static bool IsZoneHotOrInterested(std::uint32_t mapId, std::uint32_t zoneId)
    {
        return HasInterestedPlayerForMapAndZone(mapId, zoneId)
            || living_world::script::IsWorldBotZoneHot(mapId, zoneId);
    }

    void ObserveAbstractRuntimeExploration(
        AbstractWorldBotRuntime& runtime,
        std::uint16_t mapId,
        float x,
        float y,
        float z,
        char const* detail)
    {
        std::uint32_t const zoneId = ResolveZoneIdAtPosition(mapId, x, y, z);
        if (zoneId == 0)
            return;

        if (!runtime.exploredZoneIds.insert(zoneId).second)
            return;

        living_world::integration::SqlBotExploredZoneRepository().MarkExplored(runtime.identity.id, zoneId);
        living_world::integration::BotActivityLog::RecordAbstract(
            runtime.identity.name,
            runtime.identity.id,
            "zone_explored",
            std::string("Unlocked taxi knowledge for zone_id=") + std::to_string(zoneId)
                + (detail && *detail ? " via " + std::string(detail) : std::string()),
            mapId,
            zoneId,
            x,
            y,
            z);
    }

    static std::string ResolveRuntimeZoneName(std::uint32_t zoneId)
    {
        if (zoneId == 0)
            return "Unknown";

        if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(zoneId))
        {
            if (area->area_name[0] && area->area_name[0][0] != '\0')
                return area->area_name[0];
        }

        return "Zone " + std::to_string(zoneId);
    }

    static std::string ResolveRuntimeObjectiveLabel(
        living_world::service::AmbientSession const& session,
        std::size_t stepIndex)
    {
        if (stepIndex < session.steps.size())
        {
            living_world::service::AmbientStep const& step = session.steps[stepIndex];
            if (step.taskIndex >= 0)
            {
                std::size_t const taskIndex = static_cast<std::size_t>(step.taskIndex);
                if (taskIndex < session.tasks.size())
                {
                    living_world::service::AmbientSessionTask const& task = session.tasks[taskIndex];
                    if (!task.displayName.empty())
                        return task.displayName;
                    if (!task.activityKey.empty())
                        return task.activityKey;
                }
            }

            if (!step.label.empty())
                return step.label;
        }

        if (!session.displayName.empty())
            return session.displayName;

        return "current objective";
    }

    static std::string NormalizeRuntimeTransitType(std::string transitType)
    {
        if (transitType.empty())
            return "transit";

        std::transform(
            transitType.begin(),
            transitType.end(),
            transitType.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return transitType;
    }

    static bool IsKnownPhysicalTransitRoute(living_world::service::AmbientStep const& step)
    {
        std::string const transitType = NormalizeRuntimeTransitType(step.transitType);
        if (transitType == "boat")
        {
            return step.transitRouteKey == "ratchet_to_booty_bay"
                || step.transitRouteKey == "booty_bay_to_ratchet";
        }

        if (transitType == "zeppelin")
        {
            return step.transitRouteKey == "orgrimmar_to_tirisfal_zeppelin"
                || step.transitRouteKey == "undercity_to_durotar_zeppelin";
        }

        return false;
    }

    static std::uint32_t ResolveKnownPhysicalTransitTransportEntry(
        living_world::service::AmbientStep const& step)
    {
        if (!IsKnownPhysicalTransitRoute(step))
            return 0;

        std::string const transitType = NormalizeRuntimeTransitType(step.transitType);
        if (transitType == "boat")
            return 20808u;
        if (transitType == "zeppelin")
            return 164871u;
        return 0u;
    }

    static std::optional<SpawnPoint> ResolveAbstractPhysicalTransitAnchorPosition(
        AbstractWorldBotRuntime const& runtime,
        Transport** outTransport = nullptr)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return std::nullopt;

        living_world::service::AmbientStep const& step = runtime.session.steps[runtime.progress.currentStep];
        if (step.type != living_world::service::AmbientStepType::Transit
            || !IsKnownPhysicalTransitRoute(step))
        {
            return std::nullopt;
        }

        std::uint32_t const transportEntry =
            runtime.physicalTransitTransportEntry != 0
                ? runtime.physicalTransitTransportEntry
                : ResolveKnownPhysicalTransitTransportEntry(step);
        if (transportEntry == 0)
            return std::nullopt;

        Map* map = sMapMgr->CreateBaseMap(step.mapId);
        if (!map)
            return std::nullopt;

        Transport* selectedTransport = nullptr;
        for (Transport* transport : map->GetAllTransports())
        {
            if (!transport
                || !transport->IsInWorld()
                || transport->GetEntry() != transportEntry)
            {
                continue;
            }

            float const dockDist = transport->GetDistance(step.x, step.y, step.z);
            if (dockDist > 125.0f)
                continue;

            selectedTransport = transport;
            break;
        }

        if (!selectedTransport)
            return std::nullopt;

        float spawnX = step.x;
        float spawnY = step.y;
        float spawnZ = step.z;
        float spawnO = 0.0f;

        if (MotionTransport* motionTransport = dynamic_cast<MotionTransport*>(selectedTransport))
        {
            for (WorldObject* passenger : motionTransport->GetStaticPassengers())
            {
                Creature* creature = passenger ? passenger->ToCreature() : nullptr;
                if (!creature || creature->GetMapId() != step.mapId)
                    continue;

                spawnX = creature->GetPositionX() + 1.5f;
                spawnY = creature->GetPositionY() + 1.5f;
                spawnZ = creature->GetPositionZ();
                spawnO = creature->GetOrientation();
                break;
            }
        }

        if ((std::fabs(spawnX - step.x) <= 0.01f)
            && (std::fabs(spawnY - step.y) <= 0.01f)
            && (std::fabs(spawnZ - step.z) <= 0.01f))
        {
            float localX = runtime.physicalTransitLocalX;
            float localY = runtime.physicalTransitLocalY;
            float localZ = runtime.physicalTransitLocalZ;
            float localO = runtime.physicalTransitLocalO;
            if (std::fabs(localX) > 0.01f || std::fabs(localY) > 0.01f || std::fabs(localZ) > 0.01f)
            {
                selectedTransport->CalculatePassengerPosition(localX, localY, localZ, &localO);
                spawnX = localX;
                spawnY = localY;
                spawnZ = localZ;
                spawnO = localO;
            }
        }

        if (!Acore::IsValidMapCoord(spawnX, spawnY, spawnZ))
            return std::nullopt;

        if (outTransport)
            *outTransport = selectedTransport;

        return SpawnPoint{ step.mapId, spawnX, spawnY, spawnZ };
    }

    void TickSyntheticInterestBeacon(std::uint32_t diff)
    {
        if (!_debugSyntheticInterestEnabled)
            return;

        _debugSyntheticInterestElapsedMs += diff;

        if (!_debugSyntheticInterestSwitched
            && _debugSyntheticInterestSwitchMs != 0
            && _debugSyntheticInterestElapsedMs >= _debugSyntheticInterestSwitchMs)
        {
            living_world::script::SetSyntheticWorldBotInterest(
                _debugSyntheticInterestSwitchMapId,
                _debugSyntheticInterestSwitchZoneId);
            _debugSyntheticInterestSwitched = true;

            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] SyntheticInterest switched map={} zone={} after_ms={}",
                _debugSyntheticInterestSwitchMapId,
                _debugSyntheticInterestSwitchZoneId,
                _debugSyntheticInterestSwitchMs);
        }

        if (!_debugSyntheticInterestCleared
            && _debugSyntheticInterestClearMs != 0
            && _debugSyntheticInterestElapsedMs >= _debugSyntheticInterestClearMs)
        {
            living_world::script::ClearSyntheticWorldBotInterest();
            _debugSyntheticInterestCleared = true;

            LOG_INFO("server.worldserver",
                "[LivingWorldDebug] SyntheticInterest cleared after_ms={}",
                _debugSyntheticInterestClearMs);
        }
    }

    static char const* DescribeAbstractTravelPhase(
        living_world::ai::AbstractWorldBotTravelPhaseKind phase)
    {
        switch (phase)
        {
            case living_world::ai::AbstractWorldBotTravelPhaseKind::None:
                return "none";
            case living_world::ai::AbstractWorldBotTravelPhaseKind::GroundOnly:
                return "ground";
            case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiSourceGround:
                return "taxi_source_ground";
            case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiFlight:
                return "taxi_flight";
            case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiDestinationGround:
                return "taxi_destination_ground";
        }

        return "unknown";
    }

    static std::optional<living_world::ai::AbstractWorldBotTravelPhase> ResolveAbstractRuntimeTravelPhase(
        AbstractWorldBotRuntime const& runtime,
        living_world::ai::AbstractWorldBotProgressConfig const& progressConfig)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return std::nullopt;

        living_world::service::AmbientStep const& step = runtime.session.steps[runtime.progress.currentStep];
        if (step.type != living_world::service::AmbientStepType::Travel)
            return std::nullopt;

        if (auto const option =
            living_world::ai::ResolveAbstractWorldBotTravelOption(step, runtime.progress, progressConfig))
        {
            return living_world::ai::ResolveAbstractWorldBotTravelOptionPhase(*option, runtime.progress);
        }

        if (!runtime.progress.stepStartKnown || runtime.progress.stepStartMapId != step.mapId)
            return std::nullopt;

        living_world::ai::AbstractWorldBotInterpolatedPosition const position =
            living_world::ai::ComputeAbstractWorldBotInterpolatedPosition(
                runtime.session,
                runtime.progress,
                progressConfig);
        std::uint32_t zoneId = ResolveZoneIdAtPosition(
            position.mapId,
            position.x,
            position.y,
            position.z);
        if (zoneId == 0)
            zoneId = ResolveStepZoneId(runtime.session, runtime.progress.currentStep);

        return living_world::ai::AbstractWorldBotTravelPhase{
            living_world::ai::AbstractWorldBotTravelPhaseKind::GroundOnly,
            position,
            zoneId};
    }

    static bool CanMaterializeAbstractRuntime(
        AbstractWorldBotRuntime const& runtime,
        living_world::ai::AbstractWorldBotProgressConfig const& progressConfig)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return false;

        living_world::service::AmbientStep const& step = runtime.session.steps[runtime.progress.currentStep];
        if (step.type == living_world::service::AmbientStepType::Transit)
        {
            std::uint32_t const zoneId = ResolveStepZoneId(runtime.session, runtime.progress.currentStep);
            if (!IsZoneHotOrInterested(step.mapId, zoneId))
                return false;

            return ResolveAbstractPhysicalTransitAnchorPosition(runtime).has_value();
        }
        if (step.type == living_world::service::AmbientStepType::Travel
            && (!runtime.progress.stepStartKnown || runtime.progress.stepStartMapId != step.mapId))
        {
            return false;
        }

        if (auto const phase = ResolveAbstractRuntimeTravelPhase(runtime, progressConfig))
        {
            if (phase->kind == living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiFlight)
                return false;

            return IsZoneHotOrInterested(phase->position.mapId, phase->zoneId);
        }

        std::uint32_t const zoneId = ResolveStepZoneId(runtime.session, runtime.progress.currentStep);
        std::uint32_t const mapId = runtime.progress.stepStartKnown
            ? runtime.progress.stepStartMapId
            : step.mapId;
        return IsZoneHotOrInterested(mapId, zoneId);
    }

    static std::string DescribeAbstractRuntime(
        AbstractWorldBotRuntime const& runtime)
    {
        return "source_kind='" + (runtime.session.sourceKind.empty() ? std::string("unknown") : runtime.session.sourceKind)
            + "' source_key='" + (!runtime.session.sourceKey.empty() ? runtime.session.sourceKey : runtime.session.activityKey)
            + "' step=" + std::to_string(runtime.progress.currentStep)
            + " step_elapsed_ms=" + std::to_string(runtime.progress.stepElapsedMs)
            + " travel_phase='" + DescribeAbstractTravelPhase(runtime.lastTravelPhase) + "'"
            + " world_online_ms=" + std::to_string(runtime.worldOnlineMs);
    }

    static std::string DescribeAbstractRuntimeStateKey(
        AbstractWorldBotRuntime const& runtime,
        living_world::ai::AbstractWorldBotProgressConfig const& progressConfig)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return "session_complete";

        living_world::service::AmbientStep const& step = runtime.session.steps[runtime.progress.currentStep];
        if (step.type == living_world::service::AmbientStepType::Travel)
        {
            if (auto const phase = ResolveAbstractRuntimeTravelPhase(runtime, progressConfig))
            {
                switch (phase->kind)
                {
                    case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiSourceGround:
                        return "travel_taxi_approach";
                    case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiFlight:
                        return "travel_taxi_flight";
                    case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiDestinationGround:
                        return "travel_taxi_final_leg";
                    case living_world::ai::AbstractWorldBotTravelPhaseKind::GroundOnly:
                        return "travel_ground";
                    case living_world::ai::AbstractWorldBotTravelPhaseKind::None:
                    default:
                        break;
                }
            }

            return "travel_planning";
        }

        switch (step.type)
        {
            case living_world::service::AmbientStepType::GatherHerb:
                return "activity_gather_herb";
            case living_world::service::AmbientStepType::GatherOre:
                return "activity_gather_ore";
            case living_world::service::AmbientStepType::Fish:
                return "activity_fish";
            case living_world::service::AmbientStepType::Patrol:
                return "activity_patrol";
            case living_world::service::AmbientStepType::Idle:
                return "activity_idle";
            case living_world::service::AmbientStepType::Transit:
                return std::string("travel_transit_") + NormalizeRuntimeTransitType(step.transitType);
            case living_world::service::AmbientStepType::Travel:
            default:
                return "activity_unknown";
        }
    }

    static std::string DescribeAbstractRuntimeStateDetail(
        AbstractWorldBotRuntime const& runtime,
        living_world::ai::AbstractWorldBotProgressConfig const& progressConfig)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return "Session complete";

        living_world::service::AmbientStep const& step = runtime.session.steps[runtime.progress.currentStep];
        std::string const objective = ResolveRuntimeObjectiveLabel(runtime.session, runtime.progress.currentStep);
        std::string const zoneName = ResolveRuntimeZoneName(
            ResolveStepZoneId(runtime.session, runtime.progress.currentStep));

        if (step.type == living_world::service::AmbientStepType::Travel)
        {
            std::string prefix = "En route to " + zoneName;
            if (!objective.empty() && objective != zoneName)
                prefix += " for " + objective;

            if (auto const option =
                living_world::ai::ResolveAbstractWorldBotTravelOption(step, runtime.progress, progressConfig))
            {
                if (auto const phase = living_world::ai::ResolveAbstractWorldBotTravelOptionPhase(*option, runtime.progress))
                {
                    if (option->taxiJourney.has_value() && !option->taxiJourney->empty())
                    {
                        auto const& journey = *option->taxiJourney;
                        switch (phase->kind)
                        {
                            case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiSourceGround:
                                return prefix + " | heading to flight master "
                                    + journey.taxiCandidate.sourceNode.name;
                            case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiFlight:
                                return "Flying "
                                    + journey.taxiCandidate.sourceNode.name
                                    + " -> " + journey.taxiCandidate.destinationNode.name
                                    + " offscreen";
                            case living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiDestinationGround:
                                return prefix + " | final ground leg after taxi from "
                                    + journey.taxiCandidate.sourceNode.name
                                    + " -> " + journey.taxiCandidate.destinationNode.name;
                            case living_world::ai::AbstractWorldBotTravelPhaseKind::GroundOnly:
                                return prefix + " | road travel";
                            case living_world::ai::AbstractWorldBotTravelPhaseKind::None:
                            default:
                                break;
                        }
                    }

                    return prefix + " | road travel";
                }
            }

            return prefix + " | planning route";
        }

        if (step.type == living_world::service::AmbientStepType::Transit)
        {
            std::string const transitType = NormalizeRuntimeTransitType(step.transitType);
            std::string const sourceLabel = step.transitSourceLabel.empty()
                ? std::string("source")
                : step.transitSourceLabel;
            std::string const destLabel = step.transitDestLabel.empty()
                ? std::string("destination")
                : step.transitDestLabel;
            std::string detail = transitType + " " + sourceLabel + " -> " + destLabel + " offscreen";
            if (!step.label.empty() && step.label != detail)
                detail += " | " + step.label;
            return detail;
        }

        if (!objective.empty())
            return objective + " offscreen";

        return "Offscreen activity";
    }

    void SyncAbstractRuntimeLedgerState(
        AbstractWorldBotRuntime& runtime,
        living_world::ai::AbstractWorldBotProgressConfig const& progressConfig,
        living_world::ai::AbstractWorldBotInterpolatedPosition const& position,
        bool force)
    {
        std::string const state = DescribeAbstractRuntimeStateKey(runtime, progressConfig);
        std::string const detail = DescribeAbstractRuntimeStateDetail(runtime, progressConfig);
        bool const intervalElapsed =
            runtime.lastRuntimeLedgerSyncMs == 0
            || (runtime.worldOnlineMs - runtime.lastRuntimeLedgerSyncMs) >= 5000;

        if (!force
            && !intervalElapsed
            && state == runtime.lastRuntimeState
            && detail == runtime.lastRuntimeDetail)
        {
            return;
        }

        runtime.lastRuntimeLedgerSyncMs = runtime.worldOnlineMs;
        runtime.lastRuntimeState = state;
        runtime.lastRuntimeDetail = detail;

        living_world::integration::SqlBotIdentityRepository().UpdateActiveRuntimeState(
            runtime.identity.id,
            ResolveZoneIdAtPosition(position.mapId, position.x, position.y, position.z),
            runtime.worldOnlineMs,
            state,
            detail);
    }

    static std::string DescribeAbstractResumeState(
        living_world::integration::BotIdentityRecord const& identity)
    {
        if (identity.lastSeenZoneId != 0)
        {
            return "resume_from_zone=" + std::to_string(identity.lastSeenZoneId)
                + " session_count=" + std::to_string(identity.sessionCount);
        }

        return "fresh_spawn session_count=" + std::to_string(identity.sessionCount);
    }

    static std::string DescribeMaterializationIdentityRefresh(
        living_world::integration::BotIdentityRecord const& cachedIdentity,
        living_world::integration::BotIdentityRecord const& selectedIdentity)
    {
        return "cached_level=" + std::to_string(cachedIdentity.level)
            + " refreshed_level=" + std::to_string(selectedIdentity.level)
            + " cached_spec='" + cachedIdentity.specKey
            + "' refreshed_spec='" + selectedIdentity.specKey
            + "' cached_personality='" + cachedIdentity.personalityKey
            + "' refreshed_personality='" + selectedIdentity.personalityKey + "'";
    }

    bool MaterializeAbstractWorldBot(AbstractWorldBotRuntime const& runtime)
    {
        if (runtime.progress.currentStep >= runtime.session.steps.size())
            return false;

        living_world::ai::AbstractWorldBotProgressConfig const progressConfig =
            BuildAbstractProgressConfig(runtime.session, runtime.identity);
        living_world::service::AmbientStep const& step =
            runtime.session.steps[runtime.progress.currentStep];
        Transport* materializeTransport = nullptr;
        std::optional<SpawnPoint> physicalTransitSpawn;
        if (step.type == living_world::service::AmbientStepType::Transit)
            physicalTransitSpawn = ResolveAbstractPhysicalTransitAnchorPosition(runtime, &materializeTransport);

        living_world::ai::AbstractWorldBotInterpolatedPosition const pos =
            physicalTransitSpawn.has_value()
                ? living_world::ai::AbstractWorldBotInterpolatedPosition{
                    static_cast<std::uint16_t>(physicalTransitSpawn->mapId),
                    physicalTransitSpawn->x,
                    physicalTransitSpawn->y,
                    physicalTransitSpawn->z }
                : living_world::ai::ComputeAbstractWorldBotInterpolatedPosition(
                    runtime.session,
                    runtime.progress,
                    progressConfig);

        living_world::integration::SqlBotIdentityRepository identityRepository;
        auto const refreshedIdentity = identityRepository.FindById(runtime.identity.id);
        living_world::integration::BotIdentityRecord const identity =
            living_world::script::SelectWorldBotMaterializationIdentity(runtime.identity, refreshedIdentity);

        Map* map = sMapMgr->FindMap(pos.mapId, 0);
        if (!map)
            return false;

        Position spawnPos;
        spawnPos.Relocate(pos.x, pos.y, pos.z, 0.0f);
        Creature* bot = map->SummonCreature(WorldBotEntry, spawnPos);
        if (!bot)
            return false;
        (void)materializeTransport;

        living_world::integration::BotActivityLog::RecordAbstract(
            identity.name,
            identity.id,
            "status_change",
            "materializing_from_abstract -> " + DescribeAbstractRuntime(runtime),
            pos.mapId,
            ResolveStepZoneId(runtime.session, runtime.progress.currentStep),
            pos.x,
            pos.y,
            pos.z);

        if (identity.level != runtime.identity.level
            || identity.specKey != runtime.identity.specKey
            || identity.personalityKey != runtime.identity.personalityKey)
        {
            living_world::integration::BotActivityLog::RecordAbstract(
                identity.name,
                identity.id,
                "status_change",
                "materialization_identity_refresh " + DescribeMaterializationIdentityRefresh(runtime.identity, identity),
                pos.mapId,
                ResolveStepZoneId(runtime.session, runtime.progress.currentStep),
                pos.x,
                pos.y,
                pos.z);
        }

        if (auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI()))
        {
            std::size_t resumeStep = runtime.progress.currentStep;
            std::uint32_t resumeElapsedMs = runtime.progress.stepElapsedMs;
            if (step.type == living_world::service::AmbientStepType::Transit && physicalTransitSpawn.has_value())
            {
                resumeStep = std::min(runtime.progress.currentStep + 1u, runtime.session.steps.size());
                resumeElapsedMs = 0;
            }

            ai->SetIdentityAndSession(
                identity,
                runtime.session,
                resumeStep,
                resumeElapsedMs,
                runtime.worldOnlineMs,
                true,
                true);
            _materializedWorldBots[identity.id] = MaterializedWorldBotHandle{
                pos.mapId,
                bot->GetGUID()
            };
            return true;
        }

        bot->DespawnOrUnsummon(Milliseconds(0));
        return false;
    }

    void MaybeSpawnDebugRouteHarness()
    {
        if (!_debugRouteHarnessEnabled || _debugRouteHarnessSpawned || _debugRouteHarnessLevels.empty())
            return;

        Map* map = sMapMgr->FindMap(_debugRouteHarnessMapId, 0);
        if (!map)
            return;

        living_world::service::AmbientSession const session = BuildDebugRouteHarnessSession(
            _debugRouteHarnessMapId,
            _debugRouteHarnessDestZoneId,
            _debugRouteHarnessDestX,
            _debugRouteHarnessDestY,
            _debugRouteHarnessDestZ,
            _debugRouteHarnessIdleDurationSec,
            _debugRouteHarnessTransitRouteKey);

        std::uint32_t spawned = 0;
        for (std::size_t i = 0; i < _debugRouteHarnessLevels.size(); ++i)
        {
            std::uint32_t const levelValue = _debugRouteHarnessLevels[i];
            std::uint8_t const level = static_cast<std::uint8_t>(std::clamp<std::uint32_t>(levelValue, 1u, 80u));
            auto const identity = EnsureDebugRouteHarnessIdentity(
                level,
                _debugRouteHarnessRaceId,
                _debugRouteHarnessClassId,
                _debugRouteHarnessGender);
            if (!identity)
                continue;

            living_world::integration::SqlBotExploredZoneRepository().ReplaceExploredZones(
                identity->id,
                _debugRouteHarnessExploredZones);

            Position spawnPos;
            float const lateralOffset = static_cast<float>(i) * 3.0f;
            spawnPos.Relocate(
                _debugRouteHarnessStartX + lateralOffset,
                _debugRouteHarnessStartY,
                _debugRouteHarnessStartZ,
                0.0f);

            Creature* bot = map->SummonCreature(WorldBotEntry, spawnPos);
            if (!bot)
                continue;

            bot->setActive(true);

            if (auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI()))
            {
                ai->SetIdentityAndSession(*identity, session, 0, 0, 0, false, false);
                _materializedWorldBots[identity->id] = MaterializedWorldBotHandle{
                    _debugRouteHarnessMapId,
                    bot->GetGUID()
                };
                ++spawned;

                LOG_INFO("server.worldserver",
                    "[LivingWorldDebug] RouteHarness spawned '{}' identity={} level={} class={} race={} start=({:.1f},{:.1f},{:.1f}) dest_zone={} dest=({:.1f},{:.1f},{:.1f}) explored_zones='{}'",
                    identity->name,
                    identity->id,
                    identity->level,
                    identity->classId,
                    identity->raceId,
                    _debugRouteHarnessStartX,
                    _debugRouteHarnessStartY,
                    _debugRouteHarnessStartZ,
                    _debugRouteHarnessDestZoneId,
                    _debugRouteHarnessDestX,
                    _debugRouteHarnessDestY,
                    _debugRouteHarnessDestZ,
                    JoinZoneIdCsv(_debugRouteHarnessExploredZones));
            }
            else
            {
                bot->DespawnOrUnsummon(Milliseconds(0));
            }
        }

        if (spawned > 0)
            _debugRouteHarnessSpawned = true;
    }

    void DematerializeInactiveWorldBots()
    {
        if (_materializedWorldBots.empty())
            return;

        for (auto itr = _materializedWorldBots.begin(); itr != _materializedWorldBots.end(); )
        {
            MaterializedWorldBotHandle const& handle = itr->second;
            Map* map = sMapMgr->FindMap(handle.mapId, 0);
            if (!map)
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            Creature* bot = map->GetCreature(handle.guid);
            if (!bot || !bot->IsInWorld())
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI());
            if (!ai)
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            living_world::ai::WorldBotCreatureAI::RuntimeSnapshot snapshot;
            if (!ai->BuildRuntimeSnapshot(snapshot))
            {
                itr = _materializedWorldBots.erase(itr);
                continue;
            }

            if (snapshot.session.sourceKind == "debug_route_harness" && !snapshot.inPhysicalTransit)
            {
                ++itr;
                continue;
            }

            std::uint32_t const zoneId = ResolveStepZoneId(snapshot.session, snapshot.progress.currentStep);
            std::uint32_t const mapId = snapshot.progress.stepStartKnown
                ? snapshot.progress.stepStartMapId
                : (snapshot.progress.currentStep < snapshot.session.steps.size()
                    ? snapshot.session.steps[snapshot.progress.currentStep].mapId
                    : 0u);
            if ((!snapshot.inTaxiTransit && !snapshot.inPhysicalTransit)
                && IsZoneHotOrInterested(mapId, zoneId))
            {
                ++itr;
                continue;
            }

            if (snapshot.inPhysicalTransit && !snapshot.physicalTransitReadyForAbstract)
            {
                ++itr;
                continue;
            }

            AbstractWorldBotRuntime runtime;
            runtime.identity = snapshot.identity;
            runtime.session = snapshot.session;
            runtime.progress = snapshot.progress;
            runtime.exploredZoneIds = LoadExploredZoneSet(runtime.identity.id);
            runtime.worldOnlineMs = snapshot.worldOnlineMs;
            runtime.physicalTransitTransportEntry = snapshot.physicalTransitTransportEntry;
            runtime.physicalTransitLocalX = snapshot.physicalTransitLocalX;
            runtime.physicalTransitLocalY = snapshot.physicalTransitLocalY;
            runtime.physicalTransitLocalZ = snapshot.physicalTransitLocalZ;
            runtime.physicalTransitLocalO = snapshot.physicalTransitLocalO;
            if (snapshot.inTaxiTransit)
                runtime.lastTravelPhase = living_world::ai::AbstractWorldBotTravelPhaseKind::TaxiFlight;
            ObserveAbstractRuntimeExploration(
                runtime,
                runtime.progress.stepStartMapId,
                runtime.progress.stepStartX,
                runtime.progress.stepStartY,
                runtime.progress.stepStartZ,
                "dematerialize");

            _abstractWorldBots[runtime.identity.id] = runtime;

            living_world::integration::BotActivityLog::RecordAbstract(
                runtime.identity.name,
                runtime.identity.id,
                "status_change",
                std::string(snapshot.inTaxiTransit
                        ? "dematerializing_to_abstract_taxi -> "
                        : (snapshot.inPhysicalTransit
                            ? "dematerializing_to_abstract_transit -> "
                            : "dematerializing_to_abstract -> "))
                    + DescribeAbstractRuntime(runtime),
                runtime.progress.stepStartMapId,
                zoneId,
                runtime.progress.stepStartX,
                runtime.progress.stepStartY,
                runtime.progress.stepStartZ);

            bot->DespawnOrUnsummon(Milliseconds(0));
            itr = _materializedWorldBots.erase(itr);
        }
    }

    void TickAbstractWorldBots(std::uint32_t diff)
    {
        if (_abstractWorldBots.empty())
            return;

        for (auto itr = _abstractWorldBots.begin(); itr != _abstractWorldBots.end(); )
        {
            AbstractWorldBotRuntime& runtime = itr->second;

            living_world::ai::AbstractWorldBotProgressConfig const progressConfig =
                BuildAbstractProgressConfig(runtime.session, runtime.identity);

            if (CanMaterializeAbstractRuntime(runtime, progressConfig) && MaterializeAbstractWorldBot(runtime))
            {
                itr = _abstractWorldBots.erase(itr);
                continue;
            }

            runtime.worldOnlineMs += diff;
            auto const outcome = living_world::ai::AdvanceAbstractWorldBotProgress(
                runtime.session,
                runtime.progress,
                diff,
                progressConfig);
            auto const position = living_world::ai::ComputeAbstractWorldBotInterpolatedPosition(
                runtime.session,
                runtime.progress,
                progressConfig);
            SyncAbstractRuntimeLedgerState(runtime, progressConfig, position, false);
            ObserveAbstractRuntimeExploration(
                runtime,
                position.mapId,
                position.x,
                position.y,
                position.z,
                "abstract_travel");

            living_world::ai::AbstractWorldBotTravelPhaseKind currentPhase =
                living_world::ai::AbstractWorldBotTravelPhaseKind::None;
            if (auto const phase = ResolveAbstractRuntimeTravelPhase(runtime, progressConfig))
                currentPhase = phase->kind;

            if (currentPhase != runtime.lastTravelPhase)
            {
                runtime.lastTravelPhase = currentPhase;
                living_world::integration::BotActivityLog::RecordAbstract(
                    runtime.identity.name,
                    runtime.identity.id,
                    "status_change",
                    std::string("abstract_travel_phase -> phase=")
                        + DescribeAbstractTravelPhase(currentPhase)
                        + " " + DescribeAbstractRuntime(runtime),
                    position.mapId,
                    ResolveStepZoneId(runtime.session, std::min(runtime.progress.currentStep, runtime.session.steps.empty() ? std::size_t{0} : runtime.session.steps.size() - 1)),
                    position.x,
                    position.y,
                    position.z);
            }

            if (outcome.advancedStep)
            {
                runtime.lastTravelPhase = living_world::ai::AbstractWorldBotTravelPhaseKind::None;
                living_world::integration::BotActivityLog::RecordAbstract(
                    runtime.identity.name,
                    runtime.identity.id,
                    "status_change",
                    "abstract_progress -> " + DescribeAbstractRuntime(runtime),
                    runtime.progress.stepStartMapId,
                    ResolveStepZoneId(runtime.session, std::min(runtime.progress.currentStep, runtime.session.steps.empty() ? std::size_t{0} : runtime.session.steps.size() - 1)),
                    runtime.progress.stepStartX,
                    runtime.progress.stepStartY,
                    runtime.progress.stepStartZ);
            }

            if (outcome.sessionComplete)
            {
                std::uint32_t const lastSeenZoneId = runtime.session.steps.empty()
                    ? runtime.identity.lastSeenZoneId
                    : ResolveStepZoneId(runtime.session, runtime.session.steps.size() - 1);
                SessionCompletionMetadata const completionMetadata =
                    BuildSessionCompletionMetadata(runtime.session, runtime.progress.currentStep);

                living_world::integration::BotActivityLog::RecordAbstract(
                    runtime.identity.name,
                    runtime.identity.id,
                    "session_complete",
                    "abstract_offscreen session_complete world_online_ms=" + std::to_string(runtime.worldOnlineMs),
                    runtime.progress.stepStartMapId,
                    lastSeenZoneId,
                    runtime.progress.stepStartX,
                    runtime.progress.stepStartY,
                    runtime.progress.stepStartZ);

                living_world::integration::SqlBotIdentityRepository().CompleteWorldSession(
                    runtime.identity.id,
                    lastSeenZoneId,
                    runtime.worldOnlineMs,
                    completionMetadata.sourceKind,
                    completionMetadata.sourceKey,
                    completionMetadata.taskFamily,
                    completionMetadata.targetZoneId);
                itr = _abstractWorldBots.erase(itr);
                continue;
            }

            ++itr;
        }
    }

    void TickAmbientPopulation()
    {
        std::uint32_t const online = CountOnlineWorldBots();
        LOG_INFO("server.worldserver",
            "[LivingWorld] AmbientPopulationTick: online={} target={} tick_ms={}",
            online, _targetAmbientPop, _populationTickMs);

        if (HasForcedSpawnOverride())
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: forced spawn override active for up to {} bots at map={} pos=({:.1f},{:.1f},{:.1f})",
                _forcedSpawnCount,
                _forcedSpawnPoint.mapId,
                _forcedSpawnPoint.x,
                _forcedSpawnPoint.y,
                _forcedSpawnPoint.z);
        }

        LogActiveWorldBotRoster();

        if (online >= _targetAmbientPop)
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: population already satisfied.");
            return;
        }

        std::uint32_t const toSpawn = _targetAmbientPop - online;
        LOG_INFO("server.worldserver",
            "[LivingWorld] AmbientPopulationTick: requesting up to {} new world bots.",
            toSpawn);

        // Load available identities — mix of factions.
        living_world::integration::SqlBotIdentityRepository identityRepo;
        std::vector<living_world::integration::BotIdentityRecord> identities;
        if (!_debugForcedIdentityIds.empty())
        {
            identities.reserve(std::min<std::size_t>(_debugForcedIdentityIds.size(), toSpawn));
            for (std::uint32_t id : _debugForcedIdentityIds)
            {
                auto const identity = identityRepo.FindById(id);
                if (!identity || identity->isRetired || !identity->isAvailable)
                    continue;

                identities.push_back(*identity);
                if (identities.size() >= toSpawn)
                    break;
            }
        }
        else
        {
            identities = LoadReserveCityCandidates(identityRepo, toSpawn);

            if (identities.size() < toSpawn)
            {
                auto general = identityRepo.LoadAvailable(
                    0,
                    toSpawn - static_cast<std::uint32_t>(identities.size()));
                for (auto& record : general)
                    identities.push_back(std::move(record));
            }
        }

        if (identities.empty())
        {
            LOG_INFO("server.worldserver",
                "[LivingWorld] AmbientPopulationTick: no available bot identities.");
            return;
        }

        LOG_INFO("server.worldserver",
            "[LivingWorld] AmbientPopulationTick: selected {} candidate identities.",
            identities.size());

        living_world::service::BotActivitySessionComposer composer;
        std::uint32_t forcedSpawnedThisTick = 0;

        for (auto const& identity : identities)
        {
            bool usedForcedSpawn = false;
            std::unordered_set<std::uint32_t> const composeExploredZones =
                LoadExploredZoneSet(identity.id);

            // Compose a session for this identity.
            std::optional<living_world::service::AmbientSession> session;
            std::uint32_t const reserveCityZoneId = identity.populationRole == "city_reserve"
                ? identity.reserveCityZoneId
                : 0u;
            std::uint32_t const composeStartZoneId = _debugForcedSessionZoneId != 0
                ? _debugForcedSessionZoneId
                : (reserveCityZoneId != 0
                    ? reserveCityZoneId
                    : (identity.lastSeenZoneId != 0 ? identity.lastSeenZoneId : GetHubZoneIdForIdentity(identity)));
            std::uint32_t const composeHomeZoneId = _debugForcedSessionZoneId != 0
                ? _debugForcedSessionZoneId
                : (reserveCityZoneId != 0 ? reserveCityZoneId : identity.homeZoneId);
            std::string const composeHomeAnchorPointKey = _debugForcedSessionZoneId != 0
                ? std::string{}
                : identity.homeAnchorPointKey;
            std::string const composeHomeBindPointKey = _debugForcedSessionZoneId != 0
                ? std::string{}
                : identity.homeBindPointKey;
            living_world::service::AmbientSessionResumeHint const resumeHint{
                identity.lastSessionSourceKind,
                identity.lastSessionSourceKey,
                identity.lastTaskFamily,
                identity.lastTaskTargetZoneId
            };
            living_world::service::AmbientSessionComposeBias const composeBias{
                reserveCityZoneId != 0 ? std::string("city_errand") : std::string{},
                reserveCityZoneId
            };

            std::uint32_t const composeAttempts = std::max<std::uint32_t>(1u, _debugForcedSessionComposeAttempts);
            for (std::uint32_t attempt = 0; attempt < composeAttempts; ++attempt)
            {
                auto candidate = composer.Compose(
                    identity.faction,
                    identity.level,
                    identity.hasHerbalism,
                    identity.hasMining,
                    identity.hasFishing,
                    composeStartZoneId,
                    composeHomeZoneId,
                    composeHomeAnchorPointKey,
                    composeHomeBindPointKey,
                    &composeExploredZones,
                    &resumeHint,
                    reserveCityZoneId != 0 ? &composeBias : nullptr);
                if (!candidate)
                    continue;

                if (!SessionStartsInZone(*candidate, _debugForcedSessionZoneId))
                    continue;

                session = std::move(candidate);
                break;
            }

            if (!session && _debugForcedSessionZoneId != 0)
            {
                living_world::integration::SqlActivityLibraryRepository activityRepo;
                living_world::integration::SqlZoneIndexRepository zoneRepo;
                session = BuildForcedZoneSandboxSession(
                    activityRepo,
                    zoneRepo,
                    identity,
                    _debugForcedSessionZoneId);
            }

            if (!session)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: no session for "
                    "identity='{}' level={} faction={} forced_zone={}",
                    identity.name, identity.level, identity.faction, _debugForcedSessionZoneId);
                continue;
            }

            std::optional<SpawnPoint> sp;
            if (living_world::script::ShouldUseForcedSpawn(
                    { _forcedSpawnCount, _forcedSpawnPoint.mapId },
                    online,
                    forcedSpawnedThisTick))
            {
                sp = _forcedSpawnPoint;
                usedForcedSpawn = true;
            }
            else
            {
                sp = ResolveSpawnPoint(identity, *session);
            }

            if (!sp)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: no valid spawn point for "
                    "identity='{}' level={} faction={} lastSeenZone={}",
                    identity.name, identity.level, identity.faction, identity.lastSeenZoneId);
                continue;
            }

            AbstractWorldBotRuntime abstractRuntime;
            abstractRuntime.identity = identity;
            abstractRuntime.session = *session;
            abstractRuntime.progress.currentStep = 0;
            abstractRuntime.progress.stepElapsedMs = 0;
            abstractRuntime.progress.stepStartKnown = true;
            abstractRuntime.progress.stepStartMapId = static_cast<std::uint16_t>(sp->mapId);
            abstractRuntime.progress.stepStartX = sp->x;
            abstractRuntime.progress.stepStartY = sp->y;
            abstractRuntime.progress.stepStartZ = sp->z;
            abstractRuntime.exploredZoneIds = LoadExploredZoneSet(identity.id);
            ObserveAbstractRuntimeExploration(
                abstractRuntime,
                abstractRuntime.progress.stepStartMapId,
                abstractRuntime.progress.stepStartX,
                abstractRuntime.progress.stepStartY,
                abstractRuntime.progress.stepStartZ,
                "abstract_session_start");

            living_world::ai::AbstractWorldBotProgressConfig const initialProgressConfig =
                BuildAbstractProgressConfig(abstractRuntime.session, abstractRuntime.identity);

            if (!CanMaterializeAbstractRuntime(abstractRuntime, initialProgressConfig))
            {
                identityRepo.MarkActive(identity.id);
                _abstractWorldBots[identity.id] = abstractRuntime;

                living_world::integration::BotActivityLog::RecordAbstract(
                    identity.name,
                    identity.id,
                    "session_start",
                    "abstract_offscreen " + DescribeAbstractRuntime(abstractRuntime),
                    abstractRuntime.progress.stepStartMapId,
                    ResolveStepZoneId(abstractRuntime.session, 0),
                    abstractRuntime.progress.stepStartX,
                    abstractRuntime.progress.stepStartY,
                    abstractRuntime.progress.stepStartZ);

                living_world::integration::BotActivityLog::RecordAbstract(
                    identity.name,
                    identity.id,
                    "status_change",
                    DescribeAbstractResumeState(identity) + " -> abstract_offscreen",
                    abstractRuntime.progress.stepStartMapId,
                    ResolveStepZoneId(abstractRuntime.session, 0),
                    abstractRuntime.progress.stepStartX,
                    abstractRuntime.progress.stepStartY,
                    abstractRuntime.progress.stepStartZ);

                LOG_INFO("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: abstracted '{}' level={} spec='{}' source_kind='{}' source_key='{}' steps={} map={} pos=({:.1f},{:.1f},{:.1f}){}",
                    identity.name,
                    identity.level,
                    identity.specKey,
                    session->sourceKind.empty() ? "unknown" : session->sourceKind,
                    session->sourceKey.empty() ? session->activityKey : session->sourceKey,
                    session->steps.size(),
                    sp->mapId,
                    sp->x,
                    sp->y,
                    sp->z,
                    usedForcedSpawn ? " forced_spawn_override" : "");
                continue;
            }

            // Find the correct world map.
            Map* map = sMapMgr->FindMap(sp->mapId, 0);
            if (!map)
            {
                LOG_WARN("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: map {} not loaded, "
                    "skipping identity='{}'",
                    sp->mapId, identity.name);
                continue;
            }

            // Summon the creature.
            Position pos;
            pos.Relocate(sp->x, sp->y, sp->z, 0.f);
            Creature* bot = map->SummonCreature(WorldBotEntry, pos);
            if (!bot)
            {
                LOG_ERROR("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: SummonCreature failed "
                    "for identity='{}'",
                    identity.name);
                continue;
            }

            // Give the AI its identity and session.
            if (auto* ai = dynamic_cast<living_world::ai::WorldBotCreatureAI*>(bot->AI()))
            {
                ai->SetIdentityAndSession(identity, *session);
                _materializedWorldBots[identity.id] = MaterializedWorldBotHandle{
                    sp->mapId,
                    bot->GetGUID()
                };
                if (usedForcedSpawn)
                    ++forcedSpawnedThisTick;

                LOG_INFO("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: spawned '{}' "
                    "level={} spec='{}' source_kind='{}' source_key='{}' steps={} map={} pos=({:.1f},{:.1f},{:.1f}){}",
                    identity.name, identity.level,
                    identity.specKey,
                    session->sourceKind.empty() ? "unknown" : session->sourceKind,
                    session->sourceKey.empty() ? session->activityKey : session->sourceKey,
                    session->steps.size(),
                    sp->mapId, sp->x, sp->y, sp->z,
                    usedForcedSpawn
                        ? " forced_spawn_override"
                        : "");
            }
            else
            {
                LOG_ERROR("server.worldserver",
                    "[LivingWorld] AmbientPopulationTick: creature has wrong AI "
                    "for identity='{}' — check creature_template ScriptName",
                    identity.name);
                bot->DespawnOrUnsummon(Milliseconds(0));
            }
        }
    }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}
