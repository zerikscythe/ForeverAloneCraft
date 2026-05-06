#include "service/BotContextService.h"

namespace living_world
{
namespace service
{

std::string BotContextService::Get(std::uint64_t botGuid) const
{
    std::lock_guard<std::mutex> lk(_mutex);
    auto it = _contexts.find(botGuid);
    return (it != _contexts.end()) ? it->second : "PvE";
}

void BotContextService::Set(std::uint64_t botGuid, std::string const& contextKey)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _contexts[botGuid] = contextKey;
}

void BotContextService::Clear(std::uint64_t botGuid)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _contexts.erase(botGuid);
}

} // namespace service
} // namespace living_world
