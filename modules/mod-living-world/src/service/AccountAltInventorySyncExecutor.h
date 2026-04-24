#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterInventorySyncRepository.h"
#include "model/AccountAltRuntime.h"
#include "model/CharacterItemSnapshot.h"

namespace living_world
{
namespace service
{
class AccountAltInventorySyncExecutor
{
public:
    AccountAltInventorySyncExecutor(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterInventorySyncRepository& inventorySyncRepository);

    bool Execute(
        model::AccountAltRuntimeRecord const& runtime,
        model::CharacterItemSnapshot const& sourceSnapshot,
        model::CharacterItemSnapshot const& cloneSnapshot);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterInventorySyncRepository& _inventorySyncRepository;
};
} // namespace service
} // namespace living_world
