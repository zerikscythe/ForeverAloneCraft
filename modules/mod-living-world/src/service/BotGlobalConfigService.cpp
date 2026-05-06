#include "service/BotGlobalConfigService.h"

namespace living_world
{
namespace service
{

BotGlobalConfigService::BotGlobalConfigService(
    integration::BotGlobalConfigRepository const& repo)
    : _repo(repo)
    , _expiresAt(std::chrono::steady_clock::time_point{})
{}

void BotGlobalConfigService::RefreshIfStale() const
{
    auto const now = std::chrono::steady_clock::now();
    if (now < _expiresAt)
        return;
    _cached    = _repo.Load();
    _expiresAt = now + CacheTtl;
}

model::BotGlobalConfig BotGlobalConfigService::Get() const
{
    std::lock_guard<std::mutex> lk(_mutex);
    RefreshIfStale();
    return _cached;
}

void BotGlobalConfigService::Invalidate()
{
    std::lock_guard<std::mutex> lk(_mutex);
    _expiresAt = std::chrono::steady_clock::time_point{};
}

} // namespace service
} // namespace living_world
