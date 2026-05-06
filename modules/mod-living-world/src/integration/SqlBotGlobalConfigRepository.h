#pragma once

#include "integration/BotGlobalConfigRepository.h"

namespace living_world
{
namespace integration
{

class SqlBotGlobalConfigRepository final : public BotGlobalConfigRepository
{
public:
    // Creates living_world_bot_global_config and seeds defaults.
    // Safe to call on every startup — idempotent.
    void EnsureSchema() const;

    model::BotGlobalConfig Load() const override;
};

} // namespace integration
} // namespace living_world
