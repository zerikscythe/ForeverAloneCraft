#pragma once

#include "model/BotGlobalConfig.h"

namespace living_world
{
namespace integration
{

class BotGlobalConfigRepository
{
public:
    virtual ~BotGlobalConfigRepository() = default;
    virtual model::BotGlobalConfig Load() const = 0;
};

} // namespace integration
} // namespace living_world
