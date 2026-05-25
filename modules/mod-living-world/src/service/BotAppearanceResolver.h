#pragma once

#include "integration/SqlBotIdentityRepository.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace living_world::service
{

struct ResolvedBotAppearance
{
    std::uint8_t skin = 0;
    std::uint8_t face = 0;
    std::uint8_t hairStyle = 0;
    std::uint8_t hairColor = 0;
    std::uint8_t facialStyle = 0;
};

class BotAppearanceResolver
{
public:
    struct RaceGenderKey
    {
        std::uint8_t race = 0;
        std::uint8_t gender = 0;

        bool operator==(RaceGenderKey const& other) const
        {
            return race == other.race && gender == other.gender;
        }
    };

    struct RaceGenderKeyHash
    {
        std::size_t operator()(RaceGenderKey const& key) const
        {
            return (static_cast<std::size_t>(key.race) << 8) | static_cast<std::size_t>(key.gender);
        }
    };

    struct AppearanceOptions
    {
        std::vector<std::uint8_t> skins;
        std::unordered_map<std::uint8_t, std::vector<std::uint8_t>> facesBySkin;
        std::vector<std::uint8_t> hairStyles;
        std::unordered_map<std::uint8_t, std::vector<std::uint8_t>> hairColorsByStyle;
        std::vector<std::uint8_t> facialStyles;
    };

    std::optional<ResolvedBotAppearance> Resolve(
        integration::BotIdentityRecord const& identity) const;

    bool ResolveAndPersist(
        integration::BotIdentityRecord& identity,
        integration::SqlBotIdentityRepository const& repo) const;

    std::uint32_t ResolveMissingLedgerAppearances(
        integration::SqlBotIdentityRepository const& repo,
        std::uint32_t limit = 0) const;

private:
    void EnsureCatalogLoaded() const;
    void LoadCatalog() const;
    void LoadBarberOptions(std::unordered_map<RaceGenderKey, AppearanceOptions, RaceGenderKeyHash>& catalog) const;
    void LoadCharSectionsOptions(std::unordered_map<RaceGenderKey, AppearanceOptions, RaceGenderKeyHash>& catalog) const;

    mutable bool _catalogLoaded = false;
    mutable std::unordered_map<RaceGenderKey, AppearanceOptions, RaceGenderKeyHash> _catalog;
};

} // namespace living_world::service
