#pragma once

#include "integration/BotVirtualLoadoutRepository.h"

#include "QueryResult.h"

namespace living_world
{
namespace integration
{
class SqlBotVirtualLoadoutRepository final : public BotVirtualLoadoutRepository
{
public:
    std::vector<model::WorldBotVirtualLoadout> ListLoadouts() const override;

    std::optional<model::WorldBotVirtualLoadout> FindLoadout(
        std::uint8_t classId,
        std::string const& specKey,
        std::string const& loadoutKey,
        std::uint8_t gearTier) const override;

private:
    static model::WorldBotVirtualLoadout BuildLoadout(Field const* fields);
};
} // namespace integration
} // namespace living_world