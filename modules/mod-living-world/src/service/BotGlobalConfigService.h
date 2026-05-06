#pragma once

#include "integration/BotGlobalConfigRepository.h"
#include "model/BotGlobalConfig.h"

#include <chrono>
#include <mutex>

namespace living_world
{
namespace service
{

class BotGlobalConfigService
{
public:
    explicit BotGlobalConfigService(
        integration::BotGlobalConfigRepository const& repo);

    model::BotGlobalConfig Get() const;
    void Invalidate();

private:
    void RefreshIfStale() const;

    integration::BotGlobalConfigRepository const& _repo;

    mutable std::mutex                            _mutex;
    mutable std::chrono::steady_clock::time_point _expiresAt;
    mutable model::BotGlobalConfig                _cached;

    static constexpr auto CacheTtl = std::chrono::seconds(60);
};

} // namespace service
} // namespace living_world
