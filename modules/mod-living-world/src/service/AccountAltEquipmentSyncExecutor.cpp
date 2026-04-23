#include "service/AccountAltEquipmentSyncExecutor.h"

namespace living_world
{
namespace service
{
AccountAltEquipmentSyncExecutor::AccountAltEquipmentSyncExecutor(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterEquipmentSyncRepository& equipmentSyncRepository)
    : _runtimeRepository(runtimeRepository),
      _equipmentSyncRepository(equipmentSyncRepository)
{
}

bool AccountAltEquipmentSyncExecutor::Execute(
    model::AccountAltRuntimeRecord const& runtime,
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    model::AccountAltRuntimeRecord mutableRuntime = runtime;
    mutableRuntime.state = model::AccountAltRuntimeState::SyncingBack;
    _runtimeRepository.SaveRuntime(mutableRuntime);

    if (!_equipmentSyncRepository.SyncEquipmentToCharacter(
            runtime.sourceCharacterGuid,
            sourceSnapshot,
            runtime.cloneCharacterGuid,
            cloneSnapshot))
    {
        return false;
    }

    mutableRuntime.state = model::AccountAltRuntimeState::Active;
    _runtimeRepository.SaveRuntime(mutableRuntime);
    return true;
}
} // namespace service
} // namespace living_world
