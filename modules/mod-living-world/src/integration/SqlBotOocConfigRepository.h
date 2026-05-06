#pragma once

#include "integration/BotOocConfigRepository.h"

namespace living_world
{
namespace integration
{

class SqlBotOocConfigRepository : public BotOocConfigRepository
{
public:
    model::BotOocBehavior Load(std::uint64_t sourceCharGuid) const override;
    void Save(std::uint64_t sourceCharGuid,
              model::BotOocBehavior const& ooc) const override;
    void EnsureSchema() const override;
};

} // namespace integration
} // namespace living_world
