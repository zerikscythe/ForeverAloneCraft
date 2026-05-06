#pragma once

#include "integration/BotOocConfigRepository.h"
#include "model/BotCombatProfile.h"

#include <chrono>
#include <mutex>
#include <unordered_map>

namespace living_world
{
namespace service
{

// Per-character out-of-combat behaviour cache.
// Each bot character's OOC config is loaded once, cached for CacheTtl, then
// reloaded. All public methods are thread-safe.
class BotOocConfigService
{
public:
    explicit BotOocConfigService(integration::BotOocConfigRepository& repo);

    // Returns the OOC config for the given bot source character GUID.
    // Inserts a default row if none exists.
    model::BotOocBehavior Get(std::uint64_t sourceCharGuid);

    // Force-evict a single entry (call after the editor saves a new config).
    void Invalidate(std::uint64_t sourceCharGuid);

private:
    struct Entry
    {
        model::BotOocBehavior             ooc;
        std::chrono::steady_clock::time_point expiresAt;
    };

    integration::BotOocConfigRepository& _repo;
    std::mutex                           _mutex;
    std::unordered_map<std::uint64_t, Entry> _cache;

    static constexpr auto CacheTtl = std::chrono::seconds(30);
};

} // namespace service
} // namespace living_world
