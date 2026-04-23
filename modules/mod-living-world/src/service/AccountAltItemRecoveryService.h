#pragma once

#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace service
{
class AccountAltItemRecoveryService
{
public:
    model::AccountAltItemRecoveryPlan BuildRecoveryPlan(
        model::AccountAltSanityCheckResult const& sanityCheck) const;
};
} // namespace service
} // namespace living_world
