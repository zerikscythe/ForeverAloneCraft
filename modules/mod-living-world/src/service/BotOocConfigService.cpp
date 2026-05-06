#include "service/BotOocConfigService.h"

namespace living_world
{
namespace service
{

BotOocConfigService::BotOocConfigService(
    integration::BotOocConfigRepository& repo)
    : _repo(repo)
{
}

model::BotOocBehavior BotOocConfigService::Get(std::uint64_t sourceCharGuid)
{
    auto const now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(_mutex);
        auto it = _cache.find(sourceCharGuid);
        if (it != _cache.end() && it->second.expiresAt > now)
            return it->second.ooc;
    }
    model::BotOocBehavior ooc = _repo.Load(sourceCharGuid);
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _cache[sourceCharGuid] = { ooc, now + CacheTtl };
    }
    return ooc;
}

void BotOocConfigService::Invalidate(std::uint64_t sourceCharGuid)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _cache.erase(sourceCharGuid);
}

} // namespace service
} // namespace living_world
