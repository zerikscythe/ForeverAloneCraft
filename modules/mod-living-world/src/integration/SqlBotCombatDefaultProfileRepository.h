#pragma once

#include "integration/BotCombatDefaultProfileRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotCombatDefaultProfileRepository final
    : public BotCombatDefaultProfileRepository
{
public:
    std::vector<model::BotCombatDefaultProfileRecord> ListDefaultProfiles() const override;

    std::optional<model::BotCombatDefaultProfileRecord> FindDefaultProfile(
        std::string const& specKey,
        std::string const& roleKey,
        std::string const& contextKey = "PvE") const override;
};
} // namespace integration
} // namespace living_world
