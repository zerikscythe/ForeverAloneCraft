#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterEquipmentSyncRepository.h"
#include "model/CharacterItemSnapshot.h"

namespace living_world
{
namespace service
{
class AccountAltEquipmentSyncExecutor
{
public:
    AccountAltEquipmentSyncExecutor(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterEquipmentSyncRepository& equipmentSyncRepository);

    bool Execute(
        model::AccountAltRuntimeRecord const& runtime,
        model::CharacterItemSnapshot const& sourceSnapshot,
        model::CharacterItemSnapshot const& cloneSnapshot);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterEquipmentSyncRepository& _equipmentSyncRepository;
};
} // namespace service
} // namespace living_world
