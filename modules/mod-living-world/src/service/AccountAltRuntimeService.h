#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotAccountPoolRepository.h"
#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace service
{
// Owns the account-alt clone lifecycle policy before any world mutation
// happens. It decides whether a request can reuse an active clone, needs
// recovery, or should reserve a fresh bot account for materialization.
class AccountAltRuntimeService
{
public:
    AccountAltRuntimeService(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::BotAccountPoolRepository& botAccountPool);

    model::AccountAltRuntimeDecision PrepareRuntime(
        model::AccountAltRuntimeRequest const& request) const;

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::BotAccountPoolRepository& _botAccountPool;
};
} // namespace service
} // namespace living_world
