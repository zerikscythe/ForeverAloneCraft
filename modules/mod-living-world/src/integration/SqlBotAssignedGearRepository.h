#pragma once

#include "integration/BotAssignedGearRepository.h"

namespace living_world
{
namespace integration
{
class SqlBotAssignedGearRepository final : public BotAssignedGearRepository
{
public:
    void EnsureSchema() const;

    std::vector<model::WorldBotAssignedGearEntry> LoadAssignments(
        std::uint32_t identityId) const override;

    void ReplaceAssignments(
        std::uint32_t identityId,
        std::uint8_t refreshBand,
        std::vector<model::WorldBotAssignedGearEntry> const& entries) const override;
};
} // namespace integration
} // namespace living_world