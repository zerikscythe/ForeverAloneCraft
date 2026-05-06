#pragma once

#include "integration/BotHazardConfigRepository.h"
#include "model/BotHazardConfig.h"

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace living_world
{
namespace service
{

// In-memory cache over BotHazardConfigRepository.
// All three data sets (auras, role rules, tuning) are loaded together and
// expire together after CacheTtl. BotHazardSensor calls through this service
// rather than hitting the DB or hardcoding values.
//
// Thread-safe: a single mutex guards all mutable cache state.
class BotHazardConfigService
{
public:
    explicit BotHazardConfigService(
        integration::BotHazardConfigRepository const& repo);

    // Returns the set of spell IDs that are known ground hazards.
    std::unordered_set<uint32_t> GetHazardAuraIds() const;

    // Returns the role rule for roleKey, or nullptr if none is configured.
    model::HazardRoleRule const* GetRoleRule(std::string const& roleKey) const;

    // Returns the current tuning snapshot.
    model::HazardTuning GetTuning() const;

    // Invalidates the cache immediately.
    void Invalidate();

private:
    void RefreshIfStale() const;

    integration::BotHazardConfigRepository const& _repo;

    mutable std::mutex                                             _mutex;
    mutable std::chrono::steady_clock::time_point                  _expiresAt;
    mutable std::unordered_set<uint32_t>                           _auraIds;
    mutable std::vector<model::HazardAuraEntry>                    _auras;
    mutable std::unordered_map<std::string, model::HazardRoleRule> _roleRules;
    mutable model::HazardTuning                                    _tuning;

    static constexpr auto CacheTtl = std::chrono::seconds(60);
};

} // namespace service
} // namespace living_world
