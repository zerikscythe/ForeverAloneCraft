#pragma once

#include "model/BotHazardConfig.h"

#include <vector>

namespace living_world
{
namespace integration
{

class BotHazardConfigRepository
{
public:
    virtual ~BotHazardConfigRepository() = default;

    virtual std::vector<model::HazardAuraEntry>  LoadHazardAuras()  const = 0;
    virtual std::vector<model::HazardRoleRule>   LoadRoleRules()    const = 0;
    virtual model::HazardTuning                  LoadTuning()       const = 0;
};

} // namespace integration
} // namespace living_world
