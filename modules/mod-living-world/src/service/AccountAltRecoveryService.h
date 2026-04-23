#pragma once

#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace service
{
// Builds a conservative recovery plan for an interrupted account-alt clone.
// This service only decides what is safe to do; a later DB executor will
// perform the actual character-table copy inside transactions.
class AccountAltRecoveryService
{
public:
    model::AccountAltRecoveryPlan BuildRecoveryPlan(
        model::AccountAltRuntimeRecord const& runtime,
        model::CharacterProgressSnapshot const& currentSourceSnapshot,
        model::CharacterProgressSnapshot const& currentCloneSnapshot,
        model::AccountAltSanityCheckResult const& sanityCheck) const;
};
} // namespace service
} // namespace living_world
