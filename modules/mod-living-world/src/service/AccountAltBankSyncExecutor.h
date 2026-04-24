#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterBankSyncRepository.h"
#include "model/AccountAltRuntime.h"
#include "model/CharacterItemSnapshot.h"

namespace living_world
{
namespace service
{
class AccountAltBankSyncExecutor
{
public:
    AccountAltBankSyncExecutor(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterBankSyncRepository& bankSyncRepository);

    bool Execute(
        model::AccountAltRuntimeRecord const& runtime,
        model::CharacterItemSnapshot const& sourceSnapshot,
        model::CharacterItemSnapshot const& cloneSnapshot);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterBankSyncRepository& _bankSyncRepository;
};
} // namespace service
} // namespace living_world
