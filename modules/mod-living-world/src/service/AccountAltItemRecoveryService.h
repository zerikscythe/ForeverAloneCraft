#pragma once

#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace service
{
struct AccountAltItemRecoveryOptions
{
    bool enableInventorySync = false;
    bool enableBankSync = false;
};

class AccountAltItemRecoveryService
{
public:
    explicit AccountAltItemRecoveryService(
        AccountAltItemRecoveryOptions options = {});

    model::AccountAltItemRecoveryPlan BuildRecoveryPlan(
        model::AccountAltSanityCheckResult const& sanityCheck) const;

private:
    AccountAltItemRecoveryOptions _options;
};
} // namespace service
} // namespace living_world
