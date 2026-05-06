#pragma once

#include "integration/BotHazardConfigRepository.h"

namespace living_world
{
namespace integration
{

class SqlBotHazardConfigRepository final : public BotHazardConfigRepository
{
public:
    // Creates the three hazard config tables if they do not exist and seeds
    // default rows that mirror the previous hardcoded values exactly.
    // Safe to call on every startup — all statements are idempotent.
    void EnsureSchema() const;

    std::vector<model::HazardAuraEntry>  LoadHazardAuras()  const override;
    std::vector<model::HazardRoleRule>   LoadRoleRules()    const override;
    model::HazardTuning                  LoadTuning()       const override;
};

} // namespace integration
} // namespace living_world
