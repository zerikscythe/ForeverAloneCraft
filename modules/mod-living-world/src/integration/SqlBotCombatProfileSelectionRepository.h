#pragma once

#include "integration/BotCombatProfileSelectionRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotCombatProfileSelectionRepository final
    : public BotCombatProfileSelectionRepository
{
public:
    std::optional<model::BotCombatRuntimeSelection> FindRuntimeSelection(
        std::uint64_t sourceCharacterGuid) const override;

    void SaveRuntimeSelection(
        model::BotCombatRuntimeSelection const& selection) override;
};
} // namespace integration
} // namespace living_world
