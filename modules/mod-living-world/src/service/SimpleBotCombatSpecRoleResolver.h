#pragma once

#include "service/BotCombatSpecRoleResolver.h"

namespace living_world
{
namespace service
{
class SimpleBotCombatSpecRoleResolver final
    : public BotCombatSpecRoleResolver
{
public:
    BotCombatSpecRoleResolutionResult Resolve(
        BotCombatSpecRoleResolutionRequest const& request) const override;
};
} // namespace service
} // namespace living_world
