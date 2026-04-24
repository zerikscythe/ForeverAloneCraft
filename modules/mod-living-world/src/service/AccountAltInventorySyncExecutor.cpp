#include "service/AccountAltInventorySyncExecutor.h"

namespace living_world
{
namespace service
{
AccountAltInventorySyncExecutor::AccountAltInventorySyncExecutor(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterInventorySyncRepository& inventorySyncRepository)
    : _runtimeRepository(runtimeRepository),
      _inventorySyncRepository(inventorySyncRepository)
{
}

bool AccountAltInventorySyncExecutor::Execute(
    model::AccountAltRuntimeRecord const& runtime,
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    model::AccountAltRuntimeRecord mutableRuntime = runtime;
    mutableRuntime.state = model::AccountAltRuntimeState::SyncingInventory;
    _runtimeRepository.SaveRuntime(mutableRuntime);

    if (!_inventorySyncRepository.SyncInventoryToCharacter(
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
