#include "service/BotAppearanceResolver.h"

#include "Config.h"
#include "DBCStores.h"
#include "Log.h"
#include "SharedDefines.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <string>

namespace
{
using RaceGenderKey = living_world::service::BotAppearanceResolver::RaceGenderKey;
using AppearanceOptions = living_world::service::BotAppearanceResolver::AppearanceOptions;

struct RawCharSectionRecord
{
    std::uint32_t id = 0;
    std::uint32_t race = 0;
    std::uint32_t gender = 0;
    std::uint32_t section = 0;
    std::uint32_t texture1Offset = 0;
    std::uint32_t texture2Offset = 0;
    std::uint32_t texture3Offset = 0;
    std::uint32_t texture4Offset = 0;
    std::uint32_t variationIndex = 0;
    std::uint32_t colorIndex = 0;
};

template <typename T>
void SortAndUnique(std::vector<T>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::optional<std::filesystem::path> ResolveCharSectionsPath()
{
    std::string const configuredDataDir = sConfigMgr->GetOption<std::string>("DataDir", "./");

    std::array<std::filesystem::path, 5> const candidates = {
        std::filesystem::path(configuredDataDir) / "dbc" / "CharSections.dbc",
        std::filesystem::path("dbc") / "CharSections.dbc",
        std::filesystem::path("var") / "extractors" / "dbc" / "CharSections.dbc",
        std::filesystem::path("..") / "var" / "extractors" / "dbc" / "CharSections.dbc",
        std::filesystem::path("..") / ".." / "var" / "extractors" / "dbc" / "CharSections.dbc"
    };

    for (std::filesystem::path const& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
            return candidate;
    }

    return std::nullopt;
}

template <typename T>
std::uint8_t PickRandom(std::vector<T> const& values)
{
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<std::size_t> dist(0, values.size() - 1);
    return static_cast<std::uint8_t>(values[dist(rng)]);
}

std::vector<std::uint8_t> CollectAllFaces(AppearanceOptions const& options)
{
    std::vector<std::uint8_t> faces;
    for (auto const& [skin, candidates] : options.facesBySkin)
    {
        (void)skin;
        faces.insert(faces.end(), candidates.begin(), candidates.end());
    }

    SortAndUnique(faces);
    return faces;
}

std::vector<std::uint8_t> CollectAllHairColors(AppearanceOptions const& options)
{
    std::vector<std::uint8_t> colors;
    for (auto const& [style, candidates] : options.hairColorsByStyle)
    {
        (void)style;
        colors.insert(colors.end(), candidates.begin(), candidates.end());
    }

    SortAndUnique(colors);
    return colors;
}

std::vector<std::uint8_t> BuildSkinPoolForIdentity(
    living_world::integration::BotIdentityRecord const& identity,
    AppearanceOptions const& options)
{
    (void)identity;
    return options.skins;
}
} // namespace

namespace living_world::service
{

void BotAppearanceResolver::EnsureCatalogLoaded() const
{
    if (_catalogLoaded)
        return;

    LoadCatalog();
    _catalogLoaded = true;
}

void BotAppearanceResolver::LoadCatalog() const
{
    _catalog.clear();
    LoadBarberOptions(_catalog);
    LoadCharSectionsOptions(_catalog);

    for (auto& [key, options] : _catalog)
    {
        (void)key;
        SortAndUnique(options.skins);
        SortAndUnique(options.hairStyles);
        SortAndUnique(options.facialStyles);

        for (auto& [skin, faces] : options.facesBySkin)
        {
            (void)skin;
            SortAndUnique(faces);
        }

        for (auto& [style, colors] : options.hairColorsByStyle)
        {
            (void)style;
            SortAndUnique(colors);
        }
    }
}

void BotAppearanceResolver::LoadBarberOptions(
    std::unordered_map<RaceGenderKey, AppearanceOptions, RaceGenderKeyHash>& catalog) const
{
    for (auto const* entry : sBarberShopStyleStore)
    {
        if (!entry)
            continue;

        if (entry->race > std::numeric_limits<std::uint8_t>::max()
            || entry->gender > std::numeric_limits<std::uint8_t>::max()
            || entry->hair_id > std::numeric_limits<std::uint8_t>::max())
        {
            continue;
        }

        RaceGenderKey const key{
            static_cast<std::uint8_t>(entry->race),
            static_cast<std::uint8_t>(entry->gender)
        };
        AppearanceOptions& options = catalog[key];
        std::uint8_t const value = static_cast<std::uint8_t>(entry->hair_id);

        switch (entry->type)
        {
            case 0:
                options.hairStyles.push_back(value);
                break;
            case 2:
                options.facialStyles.push_back(value);
                break;
            case 3:
                options.skins.push_back(value);
                break;
            default:
                break;
        }
    }
}

void BotAppearanceResolver::LoadCharSectionsOptions(
    std::unordered_map<RaceGenderKey, AppearanceOptions, RaceGenderKeyHash>& catalog) const
{
    std::optional<std::filesystem::path> const path = ResolveCharSectionsPath();
    if (!path)
    {
        LOG_WARN("server.worldserver",
            "[LivingWorld] Bot appearance resolver could not find CharSections.dbc. Face and hair-color validation will be limited.");
        return;
    }

    std::ifstream input(*path, std::ios::binary);
    if (!input.is_open())
    {
        LOG_WARN("server.worldserver",
            "[LivingWorld] Bot appearance resolver could not open {}.",
            path->string());
        return;
    }

    char magic[4];
    std::uint32_t recordCount = 0;
    std::uint32_t fieldCount = 0;
    std::uint32_t recordSize = 0;
    std::uint32_t stringBlockSize = 0;
    input.read(magic, sizeof(magic));
    input.read(reinterpret_cast<char*>(&recordCount), sizeof(recordCount));
    input.read(reinterpret_cast<char*>(&fieldCount), sizeof(fieldCount));
    input.read(reinterpret_cast<char*>(&recordSize), sizeof(recordSize));
    input.read(reinterpret_cast<char*>(&stringBlockSize), sizeof(stringBlockSize));

    if (!input.good()
        || std::string(magic, sizeof(magic)) != "WDBC"
        || fieldCount != 10
        || recordSize != sizeof(RawCharSectionRecord))
    {
        LOG_WARN("server.worldserver",
            "[LivingWorld] CharSections.dbc at {} had unexpected header values (fields={} recordSize={}).",
            path->string(),
            fieldCount,
            recordSize);
        return;
    }

    for (std::uint32_t i = 0; i < recordCount; ++i)
    {
        RawCharSectionRecord row;
        input.read(reinterpret_cast<char*>(&row), sizeof(row));
        if (!input.good())
            break;

        if (row.race > std::numeric_limits<std::uint8_t>::max()
            || row.gender > std::numeric_limits<std::uint8_t>::max()
            || row.variationIndex > std::numeric_limits<std::uint8_t>::max()
            || row.colorIndex > std::numeric_limits<std::uint8_t>::max())
        {
            continue;
        }

        RaceGenderKey const key{
            static_cast<std::uint8_t>(row.race),
            static_cast<std::uint8_t>(row.gender)
        };
        AppearanceOptions& options = catalog[key];
        std::uint8_t const variation = static_cast<std::uint8_t>(row.variationIndex);
        std::uint8_t const color = static_cast<std::uint8_t>(row.colorIndex);

        switch (row.section)
        {
            case 0: // skin rows: colorIndex maps to the valid skin ids
                options.skins.push_back(color);
                break;
            case 1: // face rows: variationIndex = skin, colorIndex = face
                options.skins.push_back(variation);
                options.facesBySkin[variation].push_back(color);
                break;
            case 2: // facial rows: variationIndex = facial style, colorIndex tracks hair-color pairing
                options.facialStyles.push_back(variation);
                break;
            case 3: // hair rows: variationIndex = hairstyle, colorIndex = hair color
                options.hairStyles.push_back(variation);
                options.hairColorsByStyle[variation].push_back(color);
                break;
            default:
                break;
        }
    }
}

std::optional<ResolvedBotAppearance> BotAppearanceResolver::Resolve(
    integration::BotIdentityRecord const& identity) const
{
    EnsureCatalogLoaded();

    RaceGenderKey const key{ identity.raceId, identity.gender };
    auto const itr = _catalog.find(key);
    if (itr == _catalog.end())
        return std::nullopt;

    AppearanceOptions const& options = itr->second;
    if (options.skins.empty() || options.hairStyles.empty())
        return std::nullopt;

    std::vector<std::uint8_t> const skinPool = BuildSkinPoolForIdentity(identity, options);
    if (skinPool.empty())
        return std::nullopt;

    ResolvedBotAppearance result;

    auto pickSkin = [&]() -> std::uint8_t
    {
        if (identity.appearanceResolved)
        {
            if (std::find(skinPool.begin(), skinPool.end(), identity.skin) != skinPool.end())
                return identity.skin;
        }
        else if (identity.skin != 0
            && std::find(skinPool.begin(), skinPool.end(), identity.skin) != skinPool.end())
        {
            return identity.skin;
        }

        return PickRandom(skinPool);
    };

    result.skin = pickSkin();

    auto const faceItr = options.facesBySkin.find(result.skin);
    std::vector<std::uint8_t> faces =
        faceItr != options.facesBySkin.end() ? faceItr->second : CollectAllFaces(options);
    if (faces.empty())
        faces.push_back(0);

    if (identity.appearanceResolved)
    {
        if (std::find(faces.begin(), faces.end(), identity.face) != faces.end())
            result.face = identity.face;
        else
            result.face = PickRandom(faces);
    }
    else if (identity.face != 0
        && std::find(faces.begin(), faces.end(), identity.face) != faces.end())
    {
        result.face = identity.face;
    }
    else
    {
        result.face = PickRandom(faces);
    }

    auto const hasHairStyle = [&](std::uint8_t value)
    {
        return std::find(options.hairStyles.begin(), options.hairStyles.end(), value) != options.hairStyles.end();
    };

    if (identity.appearanceResolved)
    {
        result.hairStyle = hasHairStyle(identity.hairStyle) ? identity.hairStyle : PickRandom(options.hairStyles);
    }
    else if (identity.hairStyle != 0 && hasHairStyle(identity.hairStyle))
    {
        result.hairStyle = identity.hairStyle;
    }
    else
    {
        result.hairStyle = PickRandom(options.hairStyles);
    }

    auto const colorItr = options.hairColorsByStyle.find(result.hairStyle);
    std::vector<std::uint8_t> hairColors =
        colorItr != options.hairColorsByStyle.end() ? colorItr->second : CollectAllHairColors(options);
    if (hairColors.empty())
        hairColors.push_back(0);

    if (identity.appearanceResolved)
    {
        if (std::find(hairColors.begin(), hairColors.end(), identity.hairColor) != hairColors.end())
            result.hairColor = identity.hairColor;
        else
            result.hairColor = PickRandom(hairColors);
    }
    else if (identity.hairColor != 0
        && std::find(hairColors.begin(), hairColors.end(), identity.hairColor) != hairColors.end())
    {
        result.hairColor = identity.hairColor;
    }
    else
    {
        result.hairColor = PickRandom(hairColors);
    }

    if (options.facialStyles.empty())
    {
        result.facialStyle = 0;
    }
    else if (identity.appearanceResolved)
    {
        if (std::find(options.facialStyles.begin(), options.facialStyles.end(), identity.facialStyle) != options.facialStyles.end())
            result.facialStyle = identity.facialStyle;
        else
            result.facialStyle = PickRandom(options.facialStyles);
    }
    else if (identity.facialStyle != 0
        && std::find(options.facialStyles.begin(), options.facialStyles.end(), identity.facialStyle) != options.facialStyles.end())
    {
        result.facialStyle = identity.facialStyle;
    }
    else
    {
        result.facialStyle = PickRandom(options.facialStyles);
    }

    return result;
}

bool BotAppearanceResolver::ResolveAndPersist(
    integration::BotIdentityRecord& identity,
    integration::SqlBotIdentityRepository const& repo) const
{
    std::optional<ResolvedBotAppearance> const resolved = Resolve(identity);
    if (!resolved)
        return false;

    repo.UpdateAppearance(
        identity.id,
        resolved->skin,
        resolved->face,
        resolved->hairStyle,
        resolved->hairColor,
        resolved->facialStyle,
        true);

    identity.skin = resolved->skin;
    identity.face = resolved->face;
    identity.hairStyle = resolved->hairStyle;
    identity.hairColor = resolved->hairColor;
    identity.facialStyle = resolved->facialStyle;
    identity.appearanceResolved = true;
    return true;
}

std::uint32_t BotAppearanceResolver::ResolveMissingLedgerAppearances(
    integration::SqlBotIdentityRepository const& repo,
    std::uint32_t limit) const
{
    std::vector<integration::BotIdentityRecord> identities = repo.LoadAppearanceUnresolved(limit);
    std::uint32_t updated = 0;
    for (integration::BotIdentityRecord& identity : identities)
    {
        if (ResolveAndPersist(identity, repo))
            ++updated;
    }

    return updated;
}

} // namespace living_world::service
